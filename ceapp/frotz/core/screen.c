/*
 * screen.c
 *
 * Generic screen manipulation
 *
 */

#include "frotz.h"

#ifdef WINFROTZ

extern void adjustScreenSize( void );
extern void resetBitmap( void );
extern void resetScrollPos( void );

extern status_shown;

#endif

static font_height = 1;
static font_width = 1;

static discarding = 0;
static more_prompts = 1;

static struct {
    zword y_pos;
    zword x_pos;
    zword y_size;
    zword x_size;
    zword y_cursor;
    zword x_cursor;
    zword left;
    zword right;
    zword nl_routine;
    zword nl_countdown;
    zword style;
    zword colour;
    zword font;
    zword font_size;
    zword attribute;
    zword line_count;
} wp[8], *cwp;

/*
 * update_cursor
 *
 * Move the hardware cursor to make it match the window properties.
 *
 */

static void update_cursor (void)
{

    os_set_cursor (
	cwp->y_pos + cwp->y_cursor - 1,
	cwp->x_pos + cwp->x_cursor - 1);

}/* update_cursor */

/*
 * reset_cursor
 *
 * Reset the cursor of a given window to its initial position.
 *
 */

static void reset_cursor (int window)
{

    wp[window].y_cursor = 1;
    wp[window].x_cursor = wp[window].left + 1;

    if (h_version <= V4 && window == 0)
	wp[0].y_cursor = (wp[0].y_size / hi (wp[0].font_size) - 1) * hi (wp[0].font_size) + 1;

    if (window == cwin)
	update_cursor ();

}/* reset_cursor */

/*
 * set_more_prompts
 *
 * Turn more prompts on/off.
 *
 */

void set_more_prompts (int flag)
{

    more_prompts = flag;

    if (!flag)
	cwp->line_count = 0;

}/* set_more_prompts */

/*
 * units_left
 *
 * Return the #screen units from the cursor to the end of the line.
 *
 */

int units_left (void)
{

    return cwp->x_size - cwp->right - cwp->x_cursor + 1;

}/* units_left */

/*
 * get_max_width
 *
 * Return maximum width of a line in the given window.
 *
 */

int get_max_width (int window)
{

    return wp[window].x_size - wp[window].left - wp[window].right;

}/* get_max_width */

/*
 * screen_char
 *
 * Display a single character on the screen.
 *
 */

static void screen_char (int c)
{
    int width = os_char_width (c);

    if (units_left () < width) {

	if (!enable_wrapping) {
	    cwp->x_cursor = cwp->x_size - cwp->right;
	    return;
	}

	screen_new_line ();

    }

    os_display_char (c);

    cwp->x_cursor += width;

}/* screen_char */

/*
 * screen_word
 *
 * Display a string of characters on the screen. We make the following
 * assumptions about the wrapping and buffering attributes:
 *
 *	"buffering" OFF / "wrapping" OFF
 *		==> character clipping at the right margin
 *
 *	"buffering" OFF / "wrapping" ON
 *		==> character wrapping at the right margin
 *
 *	"buffering" ON / "wrapping" ON or OFF
 *		==> word wrapping at the right margin
 *
 * It would seem reasonable to expect
 *
 *	"buffering" ON / "wrapping" OFF
 *              ==> word clipping at the right margin
 *
 * but this doesn't work for the hints in Zork Zero, release 366.
 *
 */

void screen_word (const char *s)
{

    if (!discarding) {

	int width = os_string_width (s);

	if (units_left () < width) {

	    if (!enable_buffering) {

		int c;

		while ((c = (unsigned char) *s++) != 0)

		    if (c == NEW_FONT || c == NEW_STYLE) {

			int arg = (unsigned char) *s++;

			if (c == NEW_FONT)
			    os_set_font (arg);
			if (c == NEW_STYLE)
			    os_set_text_style (arg);

		    } else screen_char (c);

		return;

	    } else {

		while (*s == ' ' || *s == 9 || *s == 11)
		    s++;

		width = os_string_width (s);

#ifdef AMIGA
		Justifiable ();
#endif

		screen_new_line ();

	    }
	}

	cwp->x_cursor += width;

	os_display_string (s);

    }

}/* screen_word */

/*
 * countdown
 *
 * Decrement the newline counter. Call the newline interrupt when the
 * counter hits zero. This is a helper function for screen_new_line.
 *
 */

static void countdown (void)
{

    if (cwp->nl_countdown)
	if (--cwp->nl_countdown == 0)
	    direct_call (cwp->nl_routine);

}/* countdown */

/*
 * screen_new_line
 *
 * Print a newline to the screen.
 *
 */

void screen_new_line (void)
{

    /* All systems but MS-DOS handle newline interrupts at the start */

    if (h_interpreter_number != INTERP_MSDOS)
	countdown ();

    if (!discarding) {

	/* When the cursor has not already reached the bottom line, then
	   move it to the start of the next line. Otherwise scroll the
	   window up (if scrolling is on) or reset the cursor to the top
	   left. */

	cwp->x_cursor = cwp->left + 1;

	if (cwp->y_cursor + 2 * font_height - 1 > cwp->y_size)

	    if (enable_scrolling) {

		zword y = cwp->y_pos;
		zword x = cwp->x_pos;

		os_scroll_area (y,
				x,
				y + cwp->y_size - 1,
				x + cwp->x_size - 1,
				font_height);

	    } else cwp->y_cursor = 1;

	else cwp->y_cursor += font_height;

	update_cursor ();

	/* See if we have filled the window */

	if (enable_scrolling) {

	    /* The usual way to calculate the number of lines in a window
	       is to divide its height by the font height. However, this
	       is only correct if there is no line of text just partially
	       visible at the top of the window. This effect can be seen
	       in Shogun after crossing the labyrinth of Osaka. */

	    zword above = (cwp->y_cursor - 1) / font_height;
	    zword below = (cwp->y_size - cwp->y_cursor + 1) / font_height;

	    /* The number of lines in the window is the sum of above and
	       below. We need an extra line for displaying a MORE prompt.
	       Note that the line counter is supposed to be signed. Some
	       games set it to -999 to avoid MORE prompts for a while. */

	    cwp->line_count++;

	    if ((short) cwp->line_count >= (short) above + below - 1) {

		if (more_prompts)
		    os_more_prompt ();

		cwp->line_count = option_context_lines;

	    }

	}

    }

    /* Only MS-DOS systems handle newline interrupts at the end */

    if (h_interpreter_number == INTERP_MSDOS)
	countdown ();

}/* screen_new_line */

/*
 * screen_write_input
 *
 * Display an input line on the screen. This is required during playback.
 *
 */

void screen_write_input (const char *buf, int key)
{
    int width = os_string_width (buf);

    /* Insert a newline if the line doesn't fit */

    if (units_left () < width)
	screen_new_line ();

    /* Send the input line to the IO interface */

    os_display_string (buf);

    cwp->x_cursor += width;

    /* Add a newline if the input was terminated by the return key */

    if (key == 13)
	screen_new_line ();

}/* screen_write_input */

/*
 * screen_erase_input
 *
 * Remove an input line that has already been printed from the screen
 * as if it was deleted by the player. This could be necessary during
 * playback.
 *
 */

void screen_erase_input (const char *buf)
{
    zword y;
    zword x;

    int width = os_string_width (buf);

    /* The following safety check is essential. The previous input activity
       may have been terminated by a time-out, and a time-out routine could
       have destroyed the input line. In this case, the cursor is placed at
       the left margin of the current window, and there is no input line to
       remove. The same thing happens when a hot key is pressed. */

    if (cwp->x_cursor <= cwp->left + width + 1 || width == 0)
	return;

    cwp->x_cursor -= width;

    y = cwp->y_pos + cwp->y_cursor - 1;
    x = cwp->x_pos + cwp->x_cursor - 1;

    os_erase_area (y, x, y + font_height - 1, x + width - 1);
    os_set_cursor (y, x);

}/* screen_erase_input */

/*
 * console_read_input
 *
 * Read an input line from the keyboard and return the terminating key.
 *
 */

int console_read_input (int max, char *buf, int timeout, int continued)
{
    int key;
    int i;

    int width = os_string_width (buf);

    /* Make sure there is some space for input */

    if (cwin == 0 && width == 0 && units_left () < os_string_width ("XXXXXXXXXX"))
	screen_new_line ();

    /* Make sure the input line is visible */

    if (width != 0 && cwp->x_cursor <= cwp->left + width + 1)
	screen_write_input (buf, -1);

    /* Get input line from the IO interface */

    cwp->x_cursor -= width;

    key = os_read_line (max, buf, timeout, units_left (), continued);

    cwp->x_cursor += os_string_width (buf);

    /* Reset all line counters */

    if (key != 0)
	for (i = 0; i < 8; i++)
	    wp[i].line_count = 0;

    /* Add a newline if the input was terminated by the return key */

    if (key == 13)
	screen_new_line ();

    return key;

}/* console_read_input */

/*
 * console_read_key
 *
 * Read a single keystroke and return it.
 *
 */

int console_read_key (int timeout)
{
    int key;
    int i;

    /* Get key from the IO interface */

    key = os_read_key (timeout);

    /* Reset all line counters */

    if (key != 0)
	for (i = 0; i < 8; i++)
	    wp[i].line_count = 0;

    return key;

}/* console_read_key */

/*
 * update_attributes
 *
 * Set the four enable_*** variables to make them match the attributes
 * of the current window.
 *
 */

static void update_attributes (void)
{
    zword attr = cwp->attribute;

    enable_wrapping = attr & 1;
    enable_scrolling = attr & 2;
    enable_scripting = attr & 4;
    enable_buffering = attr & 8;

}/* update_attributes */

/*
 * set_window
 *
 * Set the current window. In V6 every window has its own set of window
 * properties such as colours, text style, cursor position and size.
 *
 */

static void set_window (zword window)
{

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window != -3)
	cwin = window;

    /* Select new window */

    cwp = wp + window;

    if (h_version == V6) {

	os_set_colour (lo (cwp->colour), hi (cwp->colour));

	if (os_font_data (cwp->font, &font_height, &font_width))
	    os_set_font (cwp->font);

	os_set_text_style (cwp->style);

    } else refresh_text_style ();

    update_attributes ();

    /* Before V6 only window 0 remembers its cursor position */

    if (h_version != V6 && window != 0)
	reset_cursor (window);
    else
	update_cursor ();

}/* set_window */

/*
 * erase_window
 *
 * Erase a window or the entire screen to background colour.
 *
 */

static void erase_window (zword window)
{

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* See if the argument is positve or negative */

    if ((short) window < 0) {	/* erase the screen */

	int i;

	os_erase_area (1, 1, h_screen_height, h_screen_width);

	if ((short) window == -1) {

	    split_window (0);
	    set_window (0);
	    reset_cursor (0);

	}

	for (i = 0; i < 8; i++)
	    wp[i].line_count = 0;

    } else {			/* erase a window */

	zword y = wp[window].y_pos;
	zword x = wp[window].x_pos;

	if (h_version == V6 && window != cwin)
	    os_set_colour (lo (wp[window].colour), hi (wp[window].colour));

	os_erase_area (y,
		       x,
		       y + wp[window].y_size - 1,
		       x + wp[window].x_size - 1);

	if (h_version == V6 && window != cwin)
	    os_set_colour (lo (cwp->colour), hi (cwp->colour));

	reset_cursor (window);

	wp[window].line_count = 0;

    }

}/* erase_window */

/*
 * refresh_text_style
 *
 * Set the right text style. This can be necessary when the fixed font
 * flag is changed, or when a new window is selected, or when the game
 * uses the set_text_style opcode.
 *
 */

void refresh_text_style (void)
{
    zword style;

    if (h_version != V6) {

	style = wp[0].style;

	if (cwin != 0 || h_flags & FIXED_FONT_FLAG)
	    style |= FIXED_WIDTH_STYLE;

    } else style = cwp->style;

    print_char (NEW_STYLE);
    print_char (style);

}/* refresh_text_style */

/*
 * resize_screen
 *
 * Try to adapt the window properties to a new screen size.
 *
 */

#ifdef AMIGA

void resize_screen (void)
{

    if (h_version == V6)
	return;

    wp[0].x_size = h_screen_width;
    wp[1].x_size = h_screen_width;
    wp[7].x_size = h_screen_width;

    wp[0].y_size = h_screen_height - wp[1].y_size - wp[7].y_size;

}/* resize_screen */

#endif

/*
 * restart_screen
 *
 * Prepare the screen for a new game.
 *
 */

void restart_screen (void)
{

#ifdef AMIGA
    CheckReset ();
#endif

    /* Use default settings */

    os_set_colour (h_default_foreground, h_default_background);

    if (os_font_data (TEXT_FONT, &font_height, &font_width))
	os_set_font (TEXT_FONT);

    os_set_text_style (0);

    os_cursor_on ();

    /* Initialise window properties */

    mwin = 1;

    for (cwp = wp; cwp < wp + 8; cwp++) {
	cwp->y_pos = 1;
	cwp->x_pos = 1;
	cwp->y_size = 0;
	cwp->x_size = 0;
	cwp->y_cursor = 1;
	cwp->x_cursor = 1;
	cwp->left = 0;
	cwp->right = 0;
	cwp->nl_routine = 0;
	cwp->nl_countdown = 0;
	cwp->style = 0;
	cwp->colour = (h_default_background << 8) | h_default_foreground;
	cwp->font = TEXT_FONT;
	cwp->font_size = (font_height << 8) | font_width;
	cwp->attribute = 8;
    }

    /* Prepare lower/upper windows and status line */

    wp[0].x_size = h_screen_width;
    wp[0].attribute = 15;

    wp[1].x_size = h_screen_width;
    wp[1].attribute = (h_version == V6) ? 8 : 0;

    if (h_version <= V3) {
	wp[7].x_size = h_screen_width;
	wp[7].attribute = 0;
    }

    /* Set the user margins */

    wp[0].left = option_left_margin;
    wp[0].right = option_right_margin;

    /* Clear the screen, unsplit it and select window 0 */

#ifdef WINFROTZ
    adjustScreenSize();
    resetBitmap();
#endif

    erase_window (ZWORD (-1));

}/* restart_screen */

/*
 * split_window
 *
 * Divide the screen into upper and lower windows. In V3 the upper
 * window appears below the status line.
 *
 */

void split_window (zword height)
{
    zword statline = 0;

    flush_buffer ();

    /* Calculate height of status line and upper window */

    if (h_version != V6)
	height *= hi (wp[1].font_size);
    if (h_version <= V3)
	statline = hi (wp[7].font_size);

    /* Erase the upper window in V3 only */

    if (h_version == V3 && height != 0)
	erase_window (1);

#ifdef WINFROTZ
    if (height != wp[1].y_size)
	resetScrollPos();
#endif

    /* Cursor of upper window mustn't be swallowed by the lower window */

    wp[1].y_cursor += wp[1].y_pos - 1 - statline;

    wp[1].y_pos = 1 + statline;
    wp[1].y_size = height;

    if ((short) wp[1].y_cursor > (short) wp[1].y_size)
	reset_cursor (1);

    /* Cursor of lower window mustn't be swallowed by the upper window */

    wp[0].y_cursor += wp[0].y_pos - 1 - statline - height;

    wp[0].y_pos = 1 + statline + height;
    wp[0].y_size = h_screen_height - statline - height;

    if ((short) wp[0].y_cursor < 1)
	reset_cursor (0);

}/* split_window */

/*
 * validate_click
 *
 * Return false if the last mouse click occured outside the current
 * mouse window; otherwise write the mouse arrow coordinates to the
 * memory of the Z-machine at h_mouse_table and return true.
 *
 */

int validate_click (void)
{

    if (mwin != -1) {

	zword y = wp[mwin].y_pos;
	zword x = wp[mwin].x_pos;

	if (mouse_y < y || mouse_y >= y + wp[mwin].y_size)
	    return 0;
	if (mouse_x < x || mouse_x >= x + wp[mwin].x_size)
	    return 0;

    } else {

	if (mouse_y < 1 || mouse_y >= h_screen_height)
	    return 0;
	if (mouse_x < 1 || mouse_x >= h_screen_width)
	    return 0;

    }

    if (h_version != V6) {
	mouse_y /= h_font_height;
	mouse_x /= h_font_width;
    }

    if (h_mouse_table) {
	storew (ZWORD (h_mouse_table + 2), ZWORD (mouse_x));
	storew (ZWORD (h_mouse_table + 4), ZWORD (mouse_y));
    }

    return 1;

}/* validate_click */

/*
 * screen_mssg_on
 *
 * Start printing a so-called debugging message. The contents of the
 * message are passed to the message stream, a Frotz specific output
 * stream with maximum priority.
 *
 */

void screen_mssg_on (void)
{

    if (cwin == 0) {		/* only messages in window 0 */

	os_set_text_style (0);

	if (cwp->x_cursor != cwp->left + 1)
	    screen_new_line ();

	screen_char (9);

    } else discarding = 1; 	/* discard messages in other windows */

}/* screen_mssg_on */

/*
 * screen_mssg_off
 *
 * Stop printing a "debugging" message.
 *
 */

void screen_mssg_off (void)
{

    if (cwin == 0) {		/* only messages in window 0 */

	screen_new_line ();

	refresh_text_style ();

    } else discarding = 0;  	/* message has been discarded */

}/* screen_mssg_off */

/*
 * z_buffer_mode, turn text buffering on/off.
 *
 *	zargs[0] = new text buffering flag (0 or 1)
 *
 */

void z_buffer_mode (void)
{
    zword window = 0;

    if (h_version == V6)
	window = cwin;

    flush_buffer ();

    if (zargs[0])
	wp[window].attribute |= 8;
    else
	wp[window].attribute &= ~8;

    update_attributes ();

}/* z_buffer_mode */

/*
 * z_draw_picture, draw a picture.
 *
 *	zargs[0] = number of picture to draw
 *	zargs[1] = y-coordinate of top left corner (optional)
 *	zargs[2] = x-coordinate of top left corner (optional)
 *
 */

void z_draw_picture (void)
{
    zword y = zargs[1];
    zword x = zargs[2];

    flush_buffer ();

    if (zargc < 2 || y == 0)	/* use cursor line if y-coordinate is 0 */
	y = cwp->y_cursor;
    if (zargc < 3 || x == 0)    /* use cursor column if x-coordinate is 0 */
	x = cwp->x_cursor;

    os_draw_picture (zargs[0], y + cwp->y_pos - 1, x + cwp->x_pos - 1);

}/* z_draw_picture */

/*
 * z_erase_line, erase the line starting at the cursor position.
 *
 *	zargs[0] = 1 + #units to erase (1 clears to the end of the line)
 *
 */

void z_erase_line (void)
{
    zword pixels = zargs[0];

    zword y;
    zword x;

    flush_buffer ();

    /* Clipping at the right border of the current window */

    if (--pixels == 0 || pixels > units_left ())
	pixels = units_left ();

    /* Erase from cursor position */

    y = cwp->y_pos + cwp->y_cursor - 1;
    x = cwp->x_pos + cwp->x_cursor - 1;

    os_erase_area (y, x, y + font_height - 1, x + pixels - 1);

}/* z_erase_line */

/*
 * z_erase_picture, erase a picture with background colour.
 *
 *	zargs[0] = number of picture to erase
 *	zargs[1] = y-coordinate of top left corner (optional)
 *	zargs[2] = x-coordinate of top left corner (optional)
 *
 */

void z_erase_picture (void)
{
    int height, width;

    zword y = zargs[1];
    zword x = zargs[2];

    flush_buffer ();

    if (zargc < 2 || y == 0)	/* use cursor line if y-coordinate is 0 */
	y = cwp->y_cursor;
    if (zargc < 3 || x == 0)    /* use cursor column if x-coordinate is 0 */
	x = cwp->x_cursor;

    os_picture_data (zargs[0], &height, &width);

    y += cwp->y_pos - 1;
    x += cwp->x_pos - 1;

    os_erase_area (y, x, y + height - 1, x + width - 1);

}/* z_erase_picture */

/*
 * z_erase_window, erase a window or the screen to background colour.
 *
 *	zargs[0] = window (-3 current, -2 screen, -1 screen & unsplit)
 *
 */

void z_erase_window (void)
{

    erase_window (zargs[0]);

}/* z_erase_window */

/*
 * z_get_cursor, write the cursor coordinates into a table.
 *
 *	zargs[0] = address to write information to
 *
 */

void z_get_cursor (void)
{
    zword y, x;

    flush_buffer ();

    /* Get cursor position */

    y = cwp->y_cursor;
    x = cwp->x_cursor;

    /* Convert screen units to grid positions if this is not V6 */

    if (h_version != V6) {
	y = (y - 1) / h_font_height + 1;
	x = (x - 1) / h_font_width + 1;
    }

    /* Store the cursor position */

    storew (ZWORD (zargs[0] + 0), y);
    storew (ZWORD (zargs[0] + 2), x);

}/* z_get_cursor */

/*
 * z_get_wind_prop, store the value of a window property.
 *
 *	zargs[0] = window (-3 is the current one)
 *	zargs[1] = number of window property to be stored
 *
 */

void z_get_wind_prop (void)
{
    zword window = zargs[0];

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Return window property to Z-machine */

    store (((zword *) (wp + window)) [zargs[1]]);

}/* z_get_wind_prop */

/*
 * z_mouse_window, select a window as mouse window.
 *
 *	zargs[0] = window number (-3 is the current) or -1 for the screen
 *
 */

void z_mouse_window (void)
{
    zword window = zargs[0];

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Set mouse window */

    mwin = (short) window;

}/* z_mouse_window */

/*
 * z_move_window, place a window on the screen.
 *
 *	zargs[0] = window (-3 is the current one)
 *	zargs[1] = y-coordinate
 *	zargs[2] = x-coordinate
 *
 */

void z_move_window (void)
{
    zword window = zargs[0];

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Move window */

    wp[window].y_pos = zargs[1];
    wp[window].x_pos = zargs[2];

    /* Hardware cursor follows the window */

    if (window == cwin)
	update_cursor ();

}/* z_move_window */

/*
 * z_picture_data, get information on a picture or the graphics file.
 *
 *	zargs[0] = number of picture or 0 for the graphics file
 *	zargs[1] = address to write information to
 *
 */

void z_picture_data (void)
{
    int height, width;

    int avail = os_picture_data (zargs[0], &height, &width);

    storew (ZWORD (zargs[1] + 0), ZWORD (height));
    storew (ZWORD (zargs[1] + 2), ZWORD (width));

    branch (avail);

}/* z_picture_data */

/*
 * z_picture_table, prepare a group of pictures for faster display.
 *
 *	zargs[0] = address of table holding the picture numbers
 *
 */

void z_picture_table (void)
{

    /* This opcode is used by Shogun and Zork Zero when the player
       encounters built-in games such as Peggleboz. Nowadays it is
       not very helpful to hold the picture data in memory because
       even a small disk cache avoids re-loading of data. */

}/* z_picture_table */

/*
 * z_print_table, print ASCII text in a rectangular area.
 *
 *	zargs[0] = address of text to be printed
 *	zargs[1] = width of rectangular area
 *	zargs[2] = height of rectangular area (optional)
 *	zargs[3] = number of char's to skip between lines (optional)
 *
 */

void z_print_table (void)
{
    zword addr = zargs[0];
    zword x;
    int i, j;

    flush_buffer ();

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = 1;
    if (zargc < 4)
	zargs[3] = 0;

    /* Write text in width x height rectangle */

    x = cwp->x_cursor;

    for (i = 0; i < zargs[2]; i++) {

	if (i != 0) {

	    cwp->y_cursor += font_height;
	    cwp->x_cursor = x;

	    update_cursor ();

	}

	for (j = 0; j < zargs[1]; j++) {

	    zbyte c;

	    LOW_BYTE (addr, c)
	    addr++;

	    print_char (c);

	}

	addr += zargs[3];

	flush_buffer ();

    }

}/* z_print_table */

/*
 * z_put_wind_prop, set the value of a window property.
 *
 *	zargs[0] = window (-3 is the current one)
 *	zargs[1] = number of window property to set
 *	zargs[2] = value to set window property to
 *
 */

void z_put_wind_prop (void)
{
    zword window = zargs[0];

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Set property */

    ((zword *) (wp + window)) [zargs[1]] = zargs[2];

}/* z_put_wind_prop */

/*
 * z_scroll_window, scroll a window up or down.
 *
 *	zargs[0] = window (-3 is the current one)
 *	zargs[1] = #screen units to scroll up (positive) or down (negative)
 *
 */

void z_scroll_window (void)
{
    zword window = zargs[0];
    zword y, x;

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Use the correct set of colours when scrolling the window */

    if (window != cwin)
	os_set_colour (lo (wp[window].colour), hi (wp[window].colour));

    y = wp[window].y_pos;
    x = wp[window].x_pos;

    os_scroll_area (y,
		    x,
		    y + wp[window].y_size - 1,
		    x + wp[window].x_size - 1,
		    (short) zargs[1]);

    if (window != cwin)
	os_set_colour (lo (cwp->colour), hi (cwp->colour));

}/* z_scroll_window */

/*
 * z_set_colour, set the foreground and background colours.
 *
 *	zargs[0] = foreground colour
 *	zargs[1] = background colour
 *
 */

void z_set_colour (void)
{
    zword window = 0;

    zword fg = zargs[0];
    zword bg = zargs[1];

    if (h_version == V6)
	window = cwin;

    flush_buffer ();

    if ((short) fg == -1)	/* colour -1 is the colour at the cursor */
	fg = os_peek_colour ();
    if ((short) bg == -1)
	bg = os_peek_colour ();

    if (fg == 0)		/* colour 0 means keep current colour */
	fg = lo (wp[window].colour);
    if (bg == 0)
	bg = hi (wp[window].colour);

    if (fg == 1)		/* colour 1 is the system default colour */
	fg = h_default_foreground;
    if (bg == 1)
	bg = h_default_background;

    wp[window].colour = (bg << 8) | fg;

    os_set_colour (fg, bg);

}/* z_set_colour */

/*
 * z_set_font, set the font for text output and store the previous font.
 *
 * 	zargs[0] = number of font or 0 to keep current font
 *
 */

void z_set_font (void)
{
    zword window = 0;
    zword font = zargs[0];

    if (h_version == V6)
	window = cwin;

    store (wp[window].font);

    if (font && os_font_data (font, &font_height, &font_width)) {

	wp[window].font = font;
	wp[window].font_size = (font_height << 8) | font_width;

	print_char (NEW_FONT);
	print_char (font);

    }

}/* z_set_font */

/*
 * z_set_cursor, set the cursor position or turn the cursor on/off.
 *
 *	zargs[0] = y-coordinate or -2/-1 for cursor on/off
 *	zargs[1] = x-coordinate
 *	zargs[2] = window (-3 is the current one, optional)
 *
 */

void z_set_cursor (void)
{
    zword window;

    zword y = zargs[0];
    zword x = zargs[1];

    flush_buffer ();

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = ZWORD (-3);

    /* Handle cursor on/off */

    if ((short) y < 0) {

	if ((short) y == -2)
	    os_cursor_on ();
	if ((short) y == -1)
	    os_cursor_off ();

	return;

    }

    /* Window -3 means the current window */

    window = zargs[2];

    if ((short) window == -3)
	window = cwin;

    /* In V1 to V5 cursor movement is restricted to the upper window */

    if (h_version != V6)
	window = 1;

    /* Convert grid positions to screen units if this is not V6 */

    if (h_version != V6) {
	y = (y - 1) * h_font_height + 1;
	x = (x - 1) * h_font_width + 1;
    }

    /* Protect the margins */

    if (x <= wp[window].left || x > wp[window].x_size - wp[window].right)
	x = wp[window].left + 1;

    /* Move the cursor */

    wp[window].y_cursor = y;
    wp[window].x_cursor = x;

    if (window == cwin)
	update_cursor ();

}/* z_set_cursor */

/*
 * z_set_margins, set the left and right margins of a window.
 *
 *	zargs[0] = left margin in pixels
 *	zargs[1] = right margin in pixels
 *	zargs[2] = window (-3 is the current one, optional)
 *
 */

void z_set_margins (void)
{
    zword window;
    zword x;

    flush_buffer ();

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = cwin;

    /* Window -3 is the current window */

    window = zargs[2];

    if ((short) window == -3)
	window = cwin;

    /* Adjust margins */

    wp[window].left = zargs[0];
    wp[window].right = zargs[1];

    /* Protect the margins */

    x = wp[window].x_cursor;

    if (x <= zargs[0] || x > wp[window].x_size - zargs[1]) {

	wp[window].x_cursor = zargs[0] + 1;

	if (window == cwin)
	    update_cursor ();

    }

}/* z_set_margins */

/*
 * z_set_text_style, set the style for text output.
 *
 * 	zargs[0] = style flags to set or 0 to reset text style
 *
 */

void z_set_text_style (void)
{
    zword window = 0;
    zword style = zargs[0];

    if (h_version == V6)
	window = cwin;

    wp[window].style |= style;

    if (style == 0)
	wp[window].style = 0;

    refresh_text_style ();

}/* z_set_text_style */

/*
 * z_set_window, select the current window.
 *
 *	zargs[0] = window to be selected (-3 is the current one)
 *
 */

void z_set_window (void)
{

    set_window (zargs[0]);

}/* z_set_window */

/*
 * pad_status_line
 *
 * Pad the status line with spaces up to the given position.
 *
 */

static void pad_status_line (int column)
{
    int spaces;

    flush_buffer ();

    spaces = units_left () / os_char_width (' ');

    /* CE port: the window is wider than the story's screen, so stop at h_screen_cols */
    if (h_screen_cols - (cwp->x_cursor - 1) < spaces)
	spaces = h_screen_cols - (cwp->x_cursor - 1);

    while (spaces-- > column)
	screen_char (' ');

}/* pad_status_line */

/*
 * z_show_status, display the status line for V1 to V3 games.
 *
 *	no zargs used
 *
 */

void z_show_status (void)
{
    zword global0;
    zword global1;
    zword global2;
    zword addr;

    int brief = 0;

    /* One V5 game (Wishbringer Solid Gold) contains this opcode by
       accident, so just return if the version number does not fit */

    if (h_version >= V4)
	return;

#ifdef WINFROTZ
    status_shown = 1;
#endif

    /* Read all relevant global variables from the memory of the
       Z-machine into local variables */

    addr = h_globals;
    LOW_WORD (addr, global0)
    addr += 2;
    LOW_WORD (addr, global1)
    addr += 2;
    LOW_WORD (addr, global2)

    /* Frotz uses window 7 for the status line */

    set_window (7);

    print_char (NEW_STYLE);
    print_char (REVERSE_STYLE | FIXED_WIDTH_STYLE);

    /* If the screen width is below 55 characters then we have to use
       the brief status line format */

    if (h_screen_cols < 55)
	brief = 1;

    /* Print the object description for the global variable 0 */

    print_char (' ');
    print_object (global0);

    /* A header flag tells us whether we have to display the current
       time or the score/moves information */

    if (h_config & CONFIG_TIME) {	/* print hours and minutes */

	zword hours = (global1 + 11) % 12 + 1;

	pad_status_line (brief ? 15 : 20);

	print_string ("Time: ");

	if (hours < 10)
	    print_char (' ');
	print_num (hours);

	print_char (':');

	if (global2 < 10)
	    print_char ('0');
	print_num (global2);

	print_char (' ');

	print_char ((global1 >= 12) ? 'p' : 'a');
	print_char ('m');

    } else {				/* print score and moves */

	pad_status_line (brief ? 15 : 30);

	print_string (brief ? "S: " : "Score: ");
	print_num (global1);

	pad_status_line (brief ? 8 : 14);

	print_string (brief ? "M: " : "Moves: ");
	print_num (global2);

    }

    /* Pad the end of the status line with spaces */

    pad_status_line (0);

    /* Return to the lower window */

    set_window (0);

}/* z_show_status */

/*
 * z_split_window, split the screen into an upper (1) and lower (0) window.
 *
 *	zargs[0] = height of upper window in screen units (V6) or #lines
 *
 */

void z_split_window (void)
{

    split_window (zargs[0]);

}/* z_split_window */

/*
 * z_window_size, change the width and height of a window.
 *
 *	zargs[0] = window (-3 is the current one)
 *	zargs[1] = new height in screen units
 *	zargs[2] = new width in screen units
 *
 */

void z_window_size (void)
{
    zword window = zargs[0];

    flush_buffer ();

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Resize the window */

    wp[window].y_size = zargs[1];
    wp[window].x_size = zargs[2];

    /* Keep the cursor within the window */

    if (wp[window].y_cursor > zargs[1] || wp[window].x_cursor > zargs[2])
	reset_cursor (window);

}/* z_window_size */

/*
 * z_window_style, set / clear / toggle window attributes.
 *
 *	zargs[0] = window (-3 is current one)
 *	zargs[1] = window attribute flags
 *	zargs[2] = operation to perform (optional, default to 0)
 *
 */

void z_window_style (void)
{
    zword window = zargs[0];
    zword new_flags = zargs[1];
    zword attr;

    flush_buffer ();

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = 0;

    /* Window -3 means the current window */

    if ((short) window == -3)
	window = cwin;

    /* Set window style */

    attr = wp[window].attribute;

    switch (zargs[2]) {
	case 0: attr = new_flags; break;
	case 1: attr |= new_flags; break;
	case 2: attr &= ~new_flags; break;
	case 3: attr ^= new_flags; break;
    }

    wp[window].attribute = attr;

    if (window == cwin)
	update_attributes ();

}/* z_window_style */
