/*
 * files.c
 *
 * Transscription, recording and playback
 *
 */

#ifndef _WIN32_WCE
#include <stdio.h>
#endif

#include <string.h>
#include "frotz.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

char script_name[MAX_FILE_NAME + 1] = DEFAULT_SCRIPT_NAME;
char command_name[MAX_FILE_NAME + 1] = DEFAULT_COMMAND_NAME;

char euro_substitute[] =
"\
aeoeueAeOeUess>><<e \
i y E I a e i o u y \
A E I O U Y a e i o \
u A E I O U a e i o \
u A E I O U a A o O \
a n o A N O aeAEc C \
ththThThL oeOE! ? ";

static script_width = 0;

static FILE *sfp = NULL;
static FILE *rfp = NULL;
static FILE *pfp = NULL;

/*
 * script_open
 *
 * Open the transscript file. 'AMFV' makes this more complicated as it
 * turns transscription on/off several times to exclude some text from
 * the transscription file. This wasn't a problem for the original V4
 * interpreters which always sent transscription to the printer, but it
 * means a problem to modern interpreters that offer to open a new file
 * every time transscription is turned on. Our solution is to append to
 * the old transscription file in V1 to V4, and to ask for a new file
 * name in V5+.
 *
 */

void script_open (void)
{
    static script_valid = 0;

    char new_name[MAX_FILE_NAME + 1];

    h_flags &= ~SCRIPTING_FLAG;

    if (h_version >= V5 || !script_valid) {

	if (!os_read_file_name (new_name, script_name, FILE_SCRIPT))
	    goto done;

	strcpy (script_name, new_name);

    }

    sfp = fopen (script_name, "at");

    if (sfp != NULL) {

	h_flags |= SCRIPTING_FLAG;

	script_valid = 1;
	ostream_script = 1;

    } else print_string ("Cannot open file\n");

done:

    SET_WORD (H_FLAGS, h_flags)

    script_width = 0;

}/* script_open */

/*
 * script_close
 *
 * Stop transscription.
 *
 */

void script_close (void)
{

    h_flags &= ~SCRIPTING_FLAG;
    SET_WORD (H_FLAGS, h_flags)

    fclose (sfp);

    ostream_script = 0;

}/* script_close */

/*
 * script_char
 *
 * Write a single character to the transscript file.
 *
 */

void script_char (int c)
{

    if (c >= EURO_MIN && c <= EURO_MAX) {

	const char *ptr = euro_substitute + 2 * (c - EURO_MIN);

	char c1 = *ptr++;
	char c2 = *ptr;

	fputc (c1, sfp);

	if (c2 != ' ')
	    fputc (c2, sfp);

	script_width += (c2 == ' ') ? 1 : 2;

    } else if (c == 9) {

	fputc (' ', sfp);
	fputc (' ', sfp);

	script_width += 2;

    } else if (c == 11) {

	fputc (' ', sfp);
	fputc (' ', sfp);
	fputc (' ', sfp);

	script_width += 3;

    } else {

	fputc (c, sfp);

	script_width += 1;

    }

}/* script_char */

/*
 * script_word
 *
 * Write a string to the transscript file.
 *
 */

void script_word (const char *s)
{
    int width = 0;
    int i, c;

    /* Calculate the width of the string */

    for (i = 0; (c = (unsigned char) s[i]) != 0; i++)

	if (c >= EURO_MIN && c <= EURO_MAX)
	    width += (euro_substitute[2 * (c - EURO_MIN) + 1] == ' ') ? 1 : 2;
	else if (c == 9)
	    width += 2;
	else if (c == 11)
	    width += 3;
	else if (c != NEW_STYLE && c != NEW_FONT)
	    width += 1;
	else
	    i++;

    /* Insert a newline if the word doesn't fit */

    if (script_width != 0 && script_width + width > option_script_cols) {

	while (*s == ' ' || *s == 9 || *s == 11)
	    s++;

	script_new_line ();

    }

    /* Send the string to the transscript file */

    for (i = 0; (c = (unsigned char) s[i]) != 0; i++)

	if (c != NEW_FONT && c != NEW_STYLE)
	    script_char (c);
	else
	    i++;

}/* script_word */

/*
 * script_new_line
 *
 * Write a newline to the transscript file.
 *
 */

void script_new_line (void)
{

    if (fputc ('\n', sfp) == EOF)
	script_close ();

    script_width = 0;

}/* script_new_line */

/*
 * script_write_input
 *
 * Send an input line to the transscript file.
 *
 */

void script_write_input (const char *buf, int key)
{
    int width = 0;
    int i, c;

    /* Calculate the width of the input line */

    for (i = 0; (c = (unsigned char) buf[i]) != 0; i++)

	if (c >= EURO_MIN && c <= EURO_MAX)
	    width += (euro_substitute[2 * (c - EURO_MIN) + 1] == ' ') ? 1 : 2;
	else
	    width++;

    /* Insert a newline if the line doesn't fit */

    if (script_width != 0 && script_width + width > option_script_cols)
	script_new_line ();

    /* Send the input line to the transscript file */

    for (i = 0; (c = (unsigned char) buf[i]) != 0; i++)
	script_char (c);

    /* Add a newline if the input was terminated by the return key */

    if (key == 13)
	script_new_line ();

}/* script_write_input */

/*
 * script_erase_input
 *
 * Remove an input line from the transscript file.
 *
 */

void script_erase_input (const char *buf)
{
    int width = 0;
    int i, c;

    /* Calculate the width of the input line */

    for (i = 0; (c = (unsigned char) buf[i]) != 0; i++)

	if (c >= EURO_MIN && c <= EURO_MAX)
	    width += (euro_substitute[2 * (c - EURO_MIN) + 1] == ' ') ? 1 : 2;
	else
	    width++;

    /* Remove the input line from the transscript file */

    fseek (sfp, -width, SEEK_CUR);

    script_width -= width;

}/* script_erase_input */

/*
 * script_mssg_on
 *
 * Start sending a "debugging" message to the transscript file.
 *
 */

void script_mssg_on (void)
{

    if (script_width != 0)
	script_new_line ();

    script_char (9);

}/* script_mssg_on */

/*
 * script_mssg_off
 *
 * Stop writing a "debugging" message.
 *
 */

void script_mssg_off (void)
{

    script_new_line ();

}/* script_mssg_off */

/*
 * record_open
 *
 * Open a file to record the player's input.
 *
 */

void record_open (void)
{
    char new_name[MAX_FILE_NAME + 1];

    if (os_read_file_name (new_name, command_name, FILE_RECORD)) {

	strcpy (command_name, new_name);

	if ((rfp = fopen (new_name, "wt")) != NULL)
	    ostream_record = 1;
	else
	    print_string ("Cannot open file\n");

    }

}/* record_open */

/*
 * record_close
 *
 * Stop recording the player's input.
 *
 */

void record_close (void)
{

    fclose (rfp);

    ostream_record = 0;

}/* record_close */

/*
 * record_char
 *
 * Helper function for record_write_key and record_write_input.
 *
 */

static void record_char (int c, int force_encoding)
{
    int i;

    if (c < 32 || c > 126 || c == '[' || force_encoding) {

	fputc ('[', rfp);

	for (i = 10000; i != 0; i /= 10)
	    if (c >= i || i == 1)
		record_char ('0' + (c / i) % 10, 0);

	fputc (']', rfp);

    } else fputc (c, rfp);

}/* record_char */

/*
 * record_write_key
 *
 * Copy a keystroke to the command file.
 *
 */

void record_write_key (int key)
{

    if (key != 13)
	record_char (key, 0);

    if (key == 253 || key == 254) {
	record_char (mouse_x, 1);
	record_char (mouse_y, 1);
    }

    if (fputc ('\n', rfp) == EOF)
	record_close ();

}/* record_write_key */

/*
 * record_write_input
 *
 * Copy a line of input to a command file.
 *
 */

void record_write_input (const char *buf, int key)
{
    int c;

    while ((c = (unsigned char) *buf++) != 0)
	record_char (c, 0);

    record_write_key (key);

}/* record_write_input */

/*
 * replay_open
 *
 * Open a file of commands that are executed as if they were typed
 * in by the player.
 *
 */

void replay_open (void)
{
    char new_name[MAX_FILE_NAME + 1];

    if (os_read_file_name (new_name, command_name, FILE_PLAYBACK)) {

	strcpy (command_name, new_name);

	if ((pfp = fopen (new_name, "rt")) != NULL) {

	    set_more_prompts (read_yes_or_no ("Do you want MORE prompts"));

	    istream_replay = 1;

	} else print_string ("Cannot open file\n");

    }

}/* replay_open */

/*
 * replay_close
 *
 * Stop playback of commands.
 *
 */

void replay_close (void)
{

    fclose (pfp);

    set_more_prompts (1);

    istream_replay = 0;

}/* replay_close */

/*
 * replay_char
 *
 * Helper function for replay_key and replay_line.
 *
 */

static int replay_char (void)
{
    int c1, c2;

    c1 = fgetc (pfp);

    if (c1 == '[') {

	c1 = 0;

	while ((c2 = fgetc (pfp)) != EOF && c2 != ']')
	    if (c2 >= '0' && c2 <= '9')
		c1 = 10 * c1 + c2 - '0';

    }

    return c1;

}/* replay_char */

/*
 * _replay_key
 *
 * Read a keystroke from a command file. The first character, however,
 * is not taken from the file; it is given as an argument instead.
 *
 */

static int _replay_read_key (int c)
{

    if (c == '\n')
	return 13;

    if (c == 253 || c == 254) {
	mouse_x = replay_char ();
	mouse_y = replay_char ();
    }

    if (fgetc (pfp) != '\n') {

	replay_close ();
	return -1;

    } else return c;

}/* _replay_read_key */

/*
 * replay_read_key
 *
 * Read a keystroke from a command file.
 *
 */

int replay_read_key (void)
{

    return _replay_read_key (replay_char ());

}/* replay_read_key */

/*
 * replay_read_input
 *
 * Read a line of input from a command file.
 *
 */

int replay_read_input (char *buf)
{
    char *ptr = buf;
    int key, c;

    do {

	c = replay_char ();

	if (c == EOF || c == '\n' || is_terminator (c))
	    break;

	*ptr++ = c;

    } while (c);

    *ptr = 0;

    key = _replay_read_key (c);

    return key;

}/* replay_read_input */
