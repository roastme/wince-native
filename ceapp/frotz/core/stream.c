/*
 * stream.c
 *
 * IO stream implementation
 *
 */

#include "frotz.h"

static message;

/*
 * stream_mssg_on
 *
 * Start printing a "debugging" message.
 *
 */

void stream_mssg_on (void)
{

    flush_buffer ();

    if (ostream_screen)
	screen_mssg_on ();
    if (ostream_script && enable_scripting)
	script_mssg_on ();

    message = 1;

}/* stream_mssg_on */

/*
 * stream_mssg_off
 *
 * Stop printing a "debugging" message.
 *
 */

void stream_mssg_off (void)
{

    flush_buffer ();

    if (ostream_screen)
	screen_mssg_off ();
    if (ostream_script && enable_scripting)
	script_mssg_off ();

    message = 0;

}/* stream_mssg_off */

/*
 * z_output_stream, open or close an output stream.
 *
 *	zargs[0] = stream to open (positive) or close (negative)
 *	zargs[1] = address to redirect output to (stream 3 only)
 *	zargs[2] = width of redirected output (stream 3 only, optional)
 *
 */

void z_output_stream (void)
{

    flush_buffer ();

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = 9999;

    /* Turn output stream on or off */

    switch ((short) zargs[0]) {

	case  1: ostream_screen = 1;
		 break;
	case -1: ostream_screen = 0;
		 break;
	case  2: if (!ostream_script) script_open ();
		 break;
	case -2: if (ostream_script) script_close ();
		 break;
	case  3: if (!ostream_memory) memory_open (zargs[1], zargs[2]);
		 break;
	case -3: if (ostream_memory) memory_close ();
		 break;
	case  4: if (!ostream_record) record_open ();
		 break;
	case -4: if (ostream_record) record_close ();
		 break;

    }

}/* z_output_stream */

/*
 * stream_word
 *
 * Send a string of characters to the output streams.
 *
 */

void stream_word (const char *s)
{

    if (ostream_memory && !message)

	memory_word (s);

    else {

	if (ostream_screen)
	    screen_word (s);
	if (ostream_script && enable_scripting)
	    script_word (s);

    }

}/* stream_word */

/*
 * stream_new_line
 *
 * Send a newline to the output streams.
 *
 */

void stream_new_line (void)
{

    if (ostream_memory && !message)

	memory_new_line ();

    else {

	if (ostream_screen)
	    screen_new_line ();
	if (ostream_script && enable_scripting)
	    script_new_line ();

    }

}/* stream_new_line */

/*
 * z_input_stream, select an input stream.
 *
 *	zargs[0] = input stream to be selected
 *
 */

void z_input_stream (void)
{

    flush_buffer ();

    if (zargs[0] == 0 && istream_replay)
	replay_close ();

    if (zargs[0] == 1 && !istream_replay)
	replay_open ();

}/* z_input_stream */

/*
 * stream_read_key
 *
 * Read a single keystroke from the current input stream.
 *
 */

int stream_read_key ( zword timeout, zword routine,
		      int hot_keys )
{
    int key;

    flush_buffer ();

    /* Read key from current input stream */

continue_input:

    do {

	if (istream_replay)
	    key = replay_read_key ();
	else
	    key = console_read_key (timeout);

    } while (key == -1);

    /* Verify mouse clicks */

    if (key == 253 || key == 254)
	if (!validate_click ())
	    goto continue_input;

    /* Copy key to the command file */

    if (ostream_record && !istream_replay)
	record_write_key (key);

    /* Handle timeouts */

    if (key == 0)
	if (direct_call (routine) == 0)
	    goto continue_input;

    /* Handle hot keys */

    if (hot_keys && key >= HOT_KEY_MIN && key <= HOT_KEY_MAX) {

	if (h_version == V4 && key == HOT_KEY_UNDO || handle_hot_key (key) == 0)
	    goto continue_input;

	return -1;

    }

    /* Return key */

    return key;

}/* stream_read_key */

/*
 * stream_read_input
 *
 * Read a line of input from the current input stream.
 *
 */

int stream_read_input ( int max, char *buf,
			zword timeout, zword routine,
			int hot_keys,
			int no_scripting )
{
    int continued;
    int key;

    flush_buffer ();

    /* Remove initial input from the transscript file or from the screen */

    if (!no_scripting && ostream_script && enable_scripting)
	script_erase_input (buf);
    if (istream_replay)
	screen_erase_input (buf);

    /* Read input line from current input stream */

    continued = 0;

continue_input:

    do {

	if (istream_replay)
	    key = replay_read_input (buf);
	else
	    key = console_read_input (max, buf, timeout, continued);

    } while (key == -1);

    continued = 1;

    /* Verify mouse clicks */

    if (key == 253 || key == 254)
	if (!validate_click ())
	    goto continue_input;

    /* Copy input line to the command file */

    if (ostream_record && !istream_replay)
	record_write_input (buf, key);

    /* Handle timeouts */

    if (key == 0)
	if (direct_call (routine) == 0)
	    goto continue_input;

    /* Handle hot keys */

    if (hot_keys && key >= HOT_KEY_MIN && key <= HOT_KEY_MAX) {

	if (handle_hot_key (key) == 0)
	    goto continue_input;

	return -1;

    }

    /* Copy input line to transscript file or to the screen */

    if (!no_scripting && ostream_script && enable_scripting)
	script_write_input (buf, key);
    if (istream_replay)
	screen_write_input (buf, key);

    /* Return terminating key */

    return key;

}/* stream_read_input */
