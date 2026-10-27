/*
 * input.c
 *
 * High level input functions
 *
 */

#include "frotz.h"

static zbyte euro_tolower[] = {
    155, 156, 157, 155, 156, 157, 161, 162, 163, 164,
    165, 166, 164, 165, 169, 170, 171, 172, 173, 174,
    169, 170, 171, 172, 173, 174, 181, 182, 183, 184,
    185, 181, 182, 183, 184, 185, 191, 192, 193, 194,
    195, 191, 192, 193, 194, 195, 201, 201, 203, 203,
    205, 206, 207, 205, 206, 207, 211, 211, 213, 213,
    215, 216, 215, 216, 219, 220, 220, 222, 223
};

/*
 * is_terminator
 *
 * Check if the given key is an input terminator.
 *
 */

int is_terminator (int key)
{

    if (key == 0 || key == 13 || key >= HOT_KEY_MIN && key <= HOT_KEY_MAX)
	return 1;

    if (h_terminating_keys)

	if (key >= 129 && key <= 154 || key >= 252 && key <= 254) {

	    zword addr = h_terminating_keys;
	    zbyte c;

	    do {
		LOW_BYTE (addr, c)
		if (c == key || c == 255)
		    return 1;
		addr++;
	    } while (c);

	}

    return 0;

}/* is_terminator */

/*
 * z_make_menu, add or remove a menu and branch if successful.
 *
 * 	zargs[0] = number of menu
 *	zargs[1] = table of menu entries or 0 to remove menu
 *
 */

void z_make_menu (void)
{

    /* This opcode was only used for the Macintosh version of Journey.
       It controls menus with numbers greater than 2 (menus 0, 1 and 2
       are system menus). Frotz doesn't implement menus yet. */

    branch (0);

}/* z_make_menu */

/*
 * read_yes_or_no
 *
 * Ask the user a question; return true if the answer is yes.
 *
 */

int read_yes_or_no (const char *s)
{
    int key;

    print_string (s);
    print_string ("? (y/n) >");

    key = stream_read_key (0, 0, 0);

    if (key == 'y' || key == 'Y') {
	print_string ("y\n");
	return 1;
    } else {
	print_string ("n\n");
	return 0;
    }

}/* read_yes_or_no */

/*
 * read_number
 *
 * Ask the user to type in a number and return it.
 *
 */

int read_number (void)
{
    char buffer[6];
    int value = 0;
    int i;

    read_string (5, buffer);

    for (i = 0; buffer[i]; i++)
	if (buffer[i] >= '0' && buffer[i] <= '9')
	    value = 10 * value + buffer[i] - '0';

    return value;

}/* read_number */

/*
 * read_string
 *
 * Allow the user to type in a string.
 *
 */

void read_string (int max, char *buffer)
{

    buffer[0] = 0;

    while (stream_read_input (max, buffer, 0, 0, 0, 0) != 13);

}/* read_string */

/*
 * z_read, read a line of input and (in V5+) store the terminating key.
 *
 *	zargs[0] = address of text buffer
 *	zargs[1] = address of token buffer
 *	zargs[2] = timeout in tenths of a second (optional)
 *	zargs[3] = packed address of routine to be called on timeout
 *
 */

void z_read (void)
{
    char buffer[INPUT_BUFFER_SIZE];
    zword addr;
    zword key;
    zbyte max, size;
    zbyte c;
    int i;

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = 0;

    /* Clear the pause flag for a workaround in z_sound_effect */

    pause_flag = 0;

    /* Get maximum input size */

    addr = zargs[0];

    LOW_BYTE (addr, max)

    if (h_version <= V4)
	max--;

    if (max >= INPUT_BUFFER_SIZE)
	max = INPUT_BUFFER_SIZE - 1;

    /* Get initial input size */

    if (h_version >= V5) {
	addr++;
	LOW_BYTE (addr, size)
    } else size = 0;

    /* Copy initial input to local buffer */

    for (i = 0; i < size; i++) {
	addr++;
	LOW_BYTE (addr, c)
	buffer[i] = (char) c;
    }

    buffer[i] = 0;

    /* Draw status line for V1 to V3 games */

    if (h_version <= V3)
	z_show_status ();

    /* Read input from current input stream */

    key = stream_read_input ( max, buffer,	/* buffer and size */
			      zargs[2],		/* timeout value   */
			      zargs[3],		/* timeout routine */
			      1,	        /* enable hot keys */
			      h_version == V6 );/* no script in V6 */

    if (key == ZWORD (-1))
	return;

    /* Perform save_undo for V1 to V4 games */

    if (h_version <= V4)
	save_undo ();

    /* Copy local buffer back to dynamic memory */

    for (i = 0; buffer[i]; i++) {

	c = buffer[i];

	if (c >= EURO_MIN && c <= EURO_MAX)
	    c = euro_tolower[c - EURO_MIN];
	if (c >= 'A' && c <= 'Z')
	    c = c + 'a' - 'A';

	storeb (ZWORD (zargs[0] + ((h_version <= V4) ? 1 : 2) + i), c);

    }

    /* Add null character (V1-V4) or write input length into 2nd byte */

    if (h_version <= V4)
	storeb (ZWORD (zargs[0] + 1 + i), 0);
    else
	storeb (ZWORD (zargs[0] + 1), (zbyte) i);

    /* Tokenise line if a token buffer is present */

    if (zargs[1])
	tokenise_line (zargs[0], zargs[1], 0, 0);

    /* Store key */

    if (h_version >= V5)
	store (key);

}/* z_read */

/*
 * z_read_char, read and store a key.
 *
 *	zargs[0] = input device (must be 1)
 *	zargs[1] = timeout in tenths of a second (optional)
 *	zargs[2] = packed address of routine to be called on timeout
 *
 */

void z_read_char (void)
{
    zword key;

    /* Supply default arguments */

    if (zargc < 2)
	zargs[1] = 0;

    /* Read input from the current input stream */

    key = stream_read_key ( zargs[1],		/* timeout value   */
			    zargs[2],		/* timeout routine */
			    1 );		/* enable hot keys */

    if (key == ZWORD (-1))
	return;

    /* Store key */

    store (key);

}/* z_read_char */

/*
 * z_read_mouse, write the current mouse status into a table.
 *
 *	zargs[0] = address of table
 *
 */

void z_read_mouse (void)
{

    storew (ZWORD (zargs[0] + 0), ZWORD (mouse_y));
    storew (ZWORD (zargs[0] + 2), ZWORD (mouse_x));
    storew (ZWORD (zargs[0] + 4), 1);	/* mouse button bits  */
    storew (ZWORD (zargs[0] + 6), 0);	/* menu selection     */

}/* z_read_mouse */
