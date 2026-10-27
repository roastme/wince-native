/*
 * buffer.c
 *
 * Text buffering and word wrapping
 *
 */

#include "frotz.h"

static char buffer[TEXT_BUFFER_SIZE];

static bufpos = 0;
static prev_c = 0;

/*
 * flush_buffer
 *
 * Copy the contents of the text buffer to the output streams.
 *
 */

void flush_buffer (void)
{
    static locked = 0;

    /* Make sure we stop when flush_buffer is called from flush_buffer.
       Note that this is difficult to avoid as we might print a newline
       during flush_buffer, which might cause a newline interrupt, that
       might execute any arbitrary opcode, which might flush the buffer. */

    if (locked || bufpos == 0)
	return;

    buffer[bufpos] = 0;

    /* Send the buffer to the output streams */

    locked = 1;

    stream_word (buffer);

    locked = 0;

    /* Reset the buffer */

    bufpos = 0;
    prev_c = 0;

}/* flush_buffer */

/*
 * print_char
 *
 * High level character output function.
 *
 */

void print_char (int c)
{
    static flag = 0;

    if (!flag) {

	/* Character 0 prints nothing, character 13 prints newline */

	if (c == 13)
	    new_line ();
	if (c == 0 || c == 13)
	    return;

	/* Flush the buffer before a whitespace or after a hyphen */

	if (c == ' ' || c == 9 || c == 11 || prev_c == '-' && c != '-')
	    flush_buffer ();

	/* Set the flag if this is part one of a style or font change */

	if (c == NEW_FONT || c == NEW_STYLE)
	    flag = 1;

	/* Remember the current character code */

	prev_c = c;

    } else flag = 0;

    /* Insert the character into the buffer */

    buffer[bufpos++] = (char) c;

    if (bufpos == TEXT_BUFFER_SIZE)
	runtime_error ("Text buffer overflow");

}/* print_char */

/*
 * new_line
 *
 * High level newline function.
 *
 */

void new_line (void)
{

    flush_buffer ();

    stream_new_line ();

}/* new_line */
