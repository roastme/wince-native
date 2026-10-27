/*
 * redirect.c
 *
 * Output redirection to Z-machine memory
 *
 */

#include "frotz.h"

static zword redirect_size = 0;
static zword redirect_addr = 0;

/*
 * memory_open
 *
 * Begin output redirection to the memory of the Z-machine.
 *
 */

void memory_open (zword table, zword width)
{

    h_line_width = 0;

    redirect_addr = table;
    redirect_size = width;

    storew (redirect_addr, 0);

    if ((short) width <= 0)
	redirect_size = get_max_width (- (short) width);

    ostream_memory = 1;

}/* memory_open */

/*
 * memory_close
 *
 * End of output redirection.
 *
 */

void memory_close (void)
{

    if (redirect_size != 9999)
	memory_new_line ();

    if (h_version == V6)
	SET_WORD (H_LINE_WIDTH, h_line_width)

    ostream_memory = 0;

}/* memory_close */

/*
 * memory_word
 *
 * Redirect a string of characters to the memory of the Z-machine.
 *
 */

void memory_word (const char *s)
{
    zword size;

    int width = os_string_width (s);
    int c;

    if (redirect_size != 9999)

	if (h_line_width != 0 && h_line_width + width > redirect_size) {

	    while (*s == ' ' || *s == 9 || *s == 11)
		s++;

	    width = os_string_width (s);

	    memory_new_line ();

	}

    LOW_WORD (redirect_addr, size)

    while ((c = (unsigned char) *s++) != 0) {

	storeb (ZWORD (redirect_addr + 2 + (size++)), (zbyte) c);

	if (c == NEW_FONT || c == NEW_STYLE)
	    storeb (ZWORD (redirect_addr + 2 + (size++)), *s++);

    }

    storew (redirect_addr, size);

    h_line_width += width;

}/* memory_word */

/*
 * memory_new_line
 *
 * Redirect a newline to the memory of the Z-machine.
 *
 */

void memory_new_line (void)
{
    zword size;

    h_line_width = 0;

    LOW_WORD (redirect_addr, size)

    if (redirect_size != 9999) {

	redirect_addr += 2 + size;
	size = 0;

    } else storeb (ZWORD (redirect_addr + 2 + (size++)), 13);

    storew (redirect_addr, size);

}/* memory_new_line */
