/*
 * text.c
 *
 * Text manipulation functions
 *
 */

#include "frotz.h"

enum string_type {
    LOW_STRING, ABBREVIATION, HIGH_STRING, EMBEDDED_STRING, VOCABULARY
};

static char decoded[10];
static zword encoded[3];

/*
 * alphabet
 *
 * Return a character from one of the three character sets.
 *
 */

static int alphabet (int set, int index)
{

    if (h_alphabet) {		/* game uses its own alphabet */

	zbyte c;

	zword char_addr = h_alphabet + 26 * set + index;
	LOW_BYTE (char_addr, c)

	return c;

    } else			/* game uses default alphabet */

	if (set == 0)
	    return 'a' + index;
	else if (set == 1)
	    return 'A' + index;
	else if (h_version == V1)
	    return " 0123456789.,!?_#'\"/\\<-:()"[index];
	else
	    return " ^0123456789.,!?_#'\"/\\-:()"[index];

}/* alphabet */

/*
 * encode_text
 *
 * Encode the ASCII text in the global "decoded" string then write
 * the result to the global "encoded" array. (This is used to look
 * up words in the dictionary.) Up to V3 the vocabulary resolution
 * was six, since V4 nine Z-characters. Long words are chopped to
 * the proper size, shorter words are are padded out with 5's. For
 * word completion we pad with 0s and 31s, the minimum and maximum
 * Z-characters.
 *
 */

static void encode_text (char padding)
{
    const char *ptr;
    char zchar[12];
    int count = 0;
    int i, j;
    int c;

    ptr = decoded;

    /* Expand abbreviations that some old Infocom games lack */

    if (option_expand_abbreviations)

	if (padding == 0x05 && decoded[1] == 0)

	    switch (decoded[0]) {
		case 'g': ptr = "again"; break;
		case 'x': ptr = "examine"; break;
		case 'z': ptr = "wait"; break;
	    }

    /* Translate ASCII characters to Z-characters */

    do {

	if ((c = (unsigned char) *ptr) != 0) {

	    int index, set;

	    ptr++;

	    /* Search for the letter in the alphabet */

	    for (set = 0; set < 3; set++)
		for (index = 0; index < 26; index++)
		    if (c == alphabet (set, index))
			goto letter_found;

	    /* Character not found, so store its ASCII value */

	    zchar[count++] = 5;
	    zchar[count++] = 6;
	    zchar[count++] = c >> 5;
	    zchar[count++] = c & 0x1f;

	    continue;

	letter_found:

	    /* Character found, so store its index */

	    if (set)
		zchar[count++] = set + ((h_version <= V2) ? 1 : 3);

	    zchar[count++] = index + 6;

	} else zchar[count++] = padding;

    } while (count < 9);

    /* Three codes make a single 16bit word */

    ptr = zchar;

    for (i = 0; i < 3; i++) {

	encoded[i] = 0;

	for (j = 10; j >= 0; j -= 5)
	    encoded[i] |= *ptr++ << j;

    }

    /* Set the highest bit on the last word */

    encoded[(h_version <= V3) ? 1 : 2] |= 0x8000;

}/* encode_text */

/*
 * load_string
 *
 * Copy an ASCII string to the global "decoded" string.
 *
 */

static void load_string (zword addr, zword length)
{
    int i;

    for (i = 0; i < length && i < 9; i++) {

	zbyte c;

	LOW_BYTE (addr, c)
	addr++;

	decoded[i] = c;

    }

    decoded[i] = 0;

}/* load_string */

/*
 * z_encode_text, encode an ASCII string for use in a dictionary.
 *
 *	zargs[0] = address of text buffer
 *	zargs[1] = length of ASCII string
 *	zargs[2] = offset of ASCII string within the text buffer
 *	zargs[3] = address to store encoded text in
 *
 */

void z_encode_text (void)
{
    int i;

    load_string (ZWORD (zargs[0] + zargs[2]), zargs[1]);

    encode_text (0x05);

    for (i = 0; i < 3; i++)
	storew (ZWORD (zargs[3] + 2 * i), encoded[i]);

}/* z_encode_text */

/*
 * decode_text
 *
 * Convert encoded text to ASCII. The encoded text consists of 16bit
 * words. Every word holds 3 Z-characters (5 bits each) plus a spare
 * bit to mark the last word. The Z-characters translate to ASCII by
 * looking at the current current character set. Some select another
 * character set, others refer to abbreviations.
 *
 * There are several different string types:
 *
 *    LOW_STRING - from the lower 64KB (byte address)
 *    ABBREVIATION - from the abbreviations table (word address)
 *    HIGH_STRING - from the end of the memory map (packed address)
 *    EMBEDDED_STRING - from the instruction stream (at PC)
 *    VOCABULARY - from the dictionary (byte address)
 *
 * The last type is only used for word completion.
 *
 */

#define outchar(c)	if (st==VOCABULARY) *ptr++=c; else print_char(c)

static void decode_text (enum string_type st, zword addr)
{
    char *ptr;
    long byte_addr;
    zword code;
    int prev_c = 0;
    int c = 0;
    int shift_state;
    int shift_lock;
    int status;

    shift_state = shift_lock = status = 0;

    /* Calculate the byte address if necessary */

    if (st == ABBREVIATION)

	byte_addr = (long) addr << 1;

    else if (st == HIGH_STRING) {

	if (h_version <= V3)
	    byte_addr = (long) addr << 1;
	else if (h_version <= V5)
	    byte_addr = (long) addr << 2;
	else if (h_version <= V7)
	    byte_addr = ((long) addr << 2) + ((long) h_strings_offset << 3);
	else /* h_version == V8 */
	    byte_addr = (long) addr << 3;

	if (byte_addr >= story_size)
	    runtime_error ("Print at illegal address");

    }

    /* Loop until a 16bit word has the highest bit set */

    if (st == VOCABULARY)
	ptr = decoded;

    do {

	int i;

	/* Fetch the next 16bit word */

	if (st == LOW_STRING || st == VOCABULARY) {
	    LOW_WORD (addr, code)
	    addr += 2;
	} else if (st == HIGH_STRING || st == ABBREVIATION) {
	    HIGH_WORD (byte_addr, code)
	    byte_addr += 2;
	} else
	    CODE_WORD (code)

	/* Read its three Z-characters */

	for (i = 10; i >= 0; i -= 5) {

	    zword abbr_addr;
	    zword ptr_addr;

	    c = (code >> i) & 0x1f;

	    switch (status) {

	    case 0:	/* normal operation */

		if (shift_state == 2 && c == 6)
		    status = 2;

		else if (h_version == V1 && c == 1)
		    new_line ();

		else if (h_version >= V2 && shift_state == 2 && c == 7)
		    new_line ();

		else if (c >= 6)
		    outchar (alphabet (shift_state, c - 6));

		else if (c == 0)
		    outchar (' ');

		else if (h_version >= V2 && c == 1)
		    status = 1;

		else if (h_version >= V3 && c <= 3)
		    status = 1;

		else {

		    shift_state = (shift_lock + (c & 1) + 1) % 3;

		    if (h_version <= V2 && c >= 4)
			shift_lock = shift_state;

		    break;

		}

		shift_state = shift_lock;

		break;

	    case 1:	/* abbreviation */

		ptr_addr = h_abbreviations + 64 * (prev_c - 1) + 2 * c;

		LOW_WORD (ptr_addr, abbr_addr)
		decode_text (ABBREVIATION, abbr_addr);

		status = 0;
		break;

	    case 2:	/* ASCII code - first part */

		status = 3;
		break;

	    case 3:	/* ASCII code - second part */

		outchar ((prev_c << 5) | c);

		status = 0;
		break;

	    }

	    prev_c = c;

	}

    } while (!(code & 0x8000));

    if (st == VOCABULARY)
	*ptr = 0;

}/* decode_text */

#undef outchar

/*
 * z_new_line, print a new line.
 *
 * 	no zargs used
 *
 */

void z_new_line (void)
{

    new_line ();

}/* z_new_line */

/*
 * z_print, print a string embedded in the instruction stream.
 *
 *	no zargs used
 *
 */

void z_print (void)
{

    decode_text (EMBEDDED_STRING, 0);

}/* z_print */

/*
 * z_print_addr, print a string from the lower 64KB.
 *
 *	zargs[0] = address of string to print
 *
 */

void z_print_addr (void)
{

    decode_text (LOW_STRING, zargs[0]);

}/* z_print_addr */

/*
 * z_print_char, print a single ASCII character.
 *
 *	zargs[0] = character to be printed
 *
 */

void z_print_char (void)
{

    print_char (zargs[0]);

}/* z_print_char */

/*
 * z_print_form, print a formatted table.
 *
 *	zargs[0] = address of formatted table to be printed
 *
 */

void z_print_form (void)
{
    zword count;
    zword addr = zargs[0];

    int first = 1;

    for (;;) {

	LOW_WORD (addr, count)
	addr += 2;

	if (count == 0)
	    break;

	if (!first)
	    z_new_line ();

	while (count--) {

	    zbyte c;

	    LOW_BYTE (addr, c)
	    addr++;

	    print_char (c);

	}

	first = 0;

    }

}/* z_print_form */

/*
 * print_num
 *
 * Print a signed 16bit number.
 *
 */

void print_num (zword value)
{
    int i;

    /* Print sign */

    if ((short) value < 0) {
	print_char ('-');
	value = - (zword) value;
    }

    /* Print value */

    for (i = 10000; i != 0; i /= 10)
	if (value >= i || i == 1)
	    print_char ('0' + (value / i) % 10);

}/* print_num */

/*
 * z_print_num, print a signed number.
 *
 * 	zargs[0] = number to print
 *
 */

void z_print_num (void)
{

    print_num (zargs[0]);

}/* z_print_num */

/*
 * print_object
 *
 * Print an object description.
 *
 */

void print_object (zword object)
{
    zword addr = object_name (object);
    zword code = 0x94a5;
    zbyte length;

    LOW_BYTE (addr, length)
    addr++;

    if (length != 0)
	LOW_WORD (addr, code)

    if (code == 0x94a5) { 	/* encoded text 0x94a5 ==> empty string */

	print_string ("object#");	/* supply a generic name */
	print_num (object);		/* for anonymous objects */

    } else decode_text (LOW_STRING, addr);

}/* print_object */

/*
 * z_print_obj, print an object description.
 *
 * 	zargs[0] = number of object to be printed
 *
 */

void z_print_obj (void)
{

    print_object (zargs[0]);

}/* z_print_obj */

/*
 * z_print_paddr, print the string at the given packed address.
 *
 * 	zargs[0] = packed address of string to be printed
 *
 */

void z_print_paddr (void)
{

    decode_text (HIGH_STRING, zargs[0]);

}/* z_print_paddr */

/*
 * z_print_ret, print the string at PC, print newline then return true.
 *
 * 	no zargs used
 *
 */

void z_print_ret (void)
{

    decode_text (EMBEDDED_STRING, 0);
    new_line ();
    ret (1);

}/* z_print_ret */

/*
 * print_string
 *
 * Print a string of ASCII characters.
 *
 */

void print_string (const char *s)
{
    int c;

    while ((c = (unsigned char) *s++) != 0)

	if (c == '\n')
	    new_line ();
	else
	    print_char (c);

}/* print_string */

/*
 * lookup_text
 *
 * Scan a directory searching for the given word. The first argument
 * can be
 *
 * 0x00 - find the first word which is >= the given one
 * 0x05 - find the word which exactly matches the given one
 * 0x1f - find the last word which is <= the given one
 *
 * The return value is 0 if the search fails.
 *
 */

static zword lookup_text (char padding, zword dct)
{
    zword entry_addr;
    zword entry_count;
    zword entry;
    zword addr;
    zbyte entry_len;
    int sep_count;
    int entry_number;
    int sorted;
    int lower, upper;
    int i;

    encode_text (padding);

    LOW_BYTE (dct, sep_count)		/* skip word separators */
    dct += 1 + sep_count;
    LOW_BYTE (dct, entry_len)		/* get length of entries */
    dct += 1;
    LOW_WORD (dct, entry_count)		/* get number of entries */
    dct += 2;

    if ((short) entry_count < 0) {	/* bad luck, entries aren't sorted */

	entry_count = - (short) entry_count;
	sorted = 0;

    } else sorted = 1;			/* entries are sorted */

    lower = 0;
    upper = entry_count - 1;

    while (lower <= upper) {

	if (sorted)                             /* binary search */
	    entry_number = (lower + upper) / 2;
	else                                    /* linear search */
	    entry_number = lower;

	entry_addr = dct + entry_number * entry_len;

	/* Compare word to dictionary entry */

	addr = entry_addr;

	for (i = 0; i < (h_version <= V3 ? 2 : 3); i++) {
	    LOW_WORD (addr, entry)
	    if (encoded[i] != entry)
		goto continuing;
	    addr += 2;
	}

	return entry_addr;		/* exact match found, return now */

    continuing:

	if (sorted)				/* binary search */

	    if (encoded[i] > entry)
		lower = entry_number + 1;
	    else
		upper = entry_number - 1;

	else lower++;                           /* linear search */

    }

    /* No exact match has been found */

    if (padding == 0x05)
	return 0;

    entry_number = (padding == 0x00) ? lower : upper;

    if (entry_number == -1 || entry_number == entry_count)
	return 0;

    return dct + entry_number * entry_len;

}/* lookup_text */

/*
 * tokenise_text
 *
 * Translate a single word to a token and append it to the token
 * buffer. Every token consists of the address of the dictionary
 * entry, the length of the word and the offset of the word from
 * the start of the text buffer. Unknwon words cause empty slots
 * if the flag is set (such that the text can be scanned several
 * times with different dictionaries); otherwise they are zero.
 *
 */

static void tokenise_text (zword text, zword length, zword from, zword parse, zword dct, zword flag)
{
    zword addr;
    zbyte token_max, token_count;

    load_string (ZWORD (text + from), length);

    addr = lookup_text (0x05, dct);

    LOW_BYTE (parse, token_max)
    parse++;
    LOW_BYTE (parse, token_count)

    if (token_count < token_max) {	/* sufficient space left for token? */

	storeb (parse++, (zbyte) (token_count + 1));

	if (addr || !flag) {

	    parse += 4 * token_count;

	    storew (ZWORD (parse + 0), addr);
	    storeb (ZWORD (parse + 2), (zbyte) length);
	    storeb (ZWORD (parse + 3), (zbyte) from);

	}

    }

}/* tokenise_text */

/*
 * tokenise_line
 *
 * Split an input line into words and translate the words to tokens.
 *
 */

void tokenise_line (zword text, zword token, zword dct, zword flag)
{
    zword addr1;
    zword addr2;
    zbyte length;
    zbyte c;

    /* Use standard dictionary if the given dictionary is zero */

    if (!dct)
	dct = h_dictionary;

    /* Remove all tokens before inserting new ones */

    storeb (ZWORD (token + 1), 0);

    /* Move the first pointer across the text buffer searching for the
       beginning of a word. If this succeeds, store the position in a
       second pointer. Move the first pointer searching for the end of
       the word. When it is found, "tokenise" the word. Continue until
       the end of the buffer is reached. */

    addr1 = text;
    addr2 = 0;

    if (h_version >= V5) {
	addr1++;
	LOW_BYTE (addr1, length)
    }

    do {

	zword sep_addr;
	zbyte sep_count;
	zbyte separator;

	/* Fetch next character */

	addr1++;

	if (h_version >= V5 && addr1 == text + 2 + length)
	    c = 0;
	else
	    LOW_BYTE (addr1, c)

	/* Check for separator */

	sep_addr = dct;

	LOW_BYTE (sep_addr, sep_count)
	sep_addr++;

	do {

	    LOW_BYTE (sep_addr, separator)
	    sep_addr++;

	} while (c != separator && --sep_count != 0);

	/* This could be the start or the end of a word */

	if (!sep_count && c != ' ' && c != 0) {

	    if (!addr2)
		addr2 = addr1;

	} else if (addr2) {

	    tokenise_text ( text,
			    ZWORD (addr1 - addr2),
			    ZWORD (addr2 - text),
			    token,
			    dct, flag);

	    addr2 = 0;

	}

	/* Translate separator (which is a word in its own right) */

	if (sep_count)

	    tokenise_text ( text,
			    1,
			    ZWORD (addr1 - text),
			    token,
			    dct, flag);

    } while (c);

}/* tokenise_line */

/*
 * z_tokenise, make a lexical analysis of an ASCII string.
 *
 *	zargs[0] = address of string to analyze
 *	zargs[1] = address of token buffer
 *	zargs[2] = address of dictionary (optional)
 *	zargs[3] = set when unknown words cause empty slots (optional)
 *
 */

void z_tokenise (void)
{

    /* Supply default arguments */

    if (zargc < 3)
	zargs[2] = 0;
    if (zargc < 4)
	zargs[3] = 0;

    /* Call tokenise_line to do the real work */

    tokenise_line (zargs[0], zargs[1], zargs[2], zargs[3]);

}/* z_tokenise */

/*
 * completion
 *
 * Scan the vocabulary to complete the last word on the input line
 * (similar to "tcsh" under Unix). The return value is
 *
 *    2 ==> completion is impossible
 *    1 ==> completion is ambiguous
 *    0 ==> completion is successful
 *
 * The function also returns a string in its second argument. In case
 * of 2, the string is empty; in case of 1, the string is the longest
 * extension of the last word on the input line that is common to all
 * possible completions (for instance, if the last word on the input
 * is "fo" and its only possible completions are "follow" and "folly"
 * then the string is "ll"); in case of 0, the string is an extension
 * to the last word that results in the only possible completion.
 *
 */

int completion (const char *buffer, char *result)
{
    char *ptr;
    zword minaddr;
    zword maxaddr;
    int len;
    int i;
    char c;

    *result = 0;

    /* Copy last word to "decoded" string */

    len = 0;

    while ((c = *buffer++) != 0)

	if (c != ' ') {

	    if (len < 9)
		decoded[len++] = c;

	} else len = 0;

    decoded[len] = 0;

    /* Search the dictionary the first and last possible extension */

    minaddr = lookup_text (0x00, h_dictionary);
    maxaddr = lookup_text (0x1f, h_dictionary);

    if (!minaddr || !maxaddr || minaddr > maxaddr)
	return 2;

    /* Copy first extension to "result" string */

    decode_text (VOCABULARY, minaddr);

    for (i = len, ptr = result; (c = decoded[i]) != 0; i++, ptr++)
	*ptr = c;

    *ptr = 0;

    /* Merge second extension with "result" string */

    decode_text (VOCABULARY, maxaddr);

    for (i = len, ptr = result; (c = decoded[i]) != 0; i++, ptr++)
	if (*ptr != c)
	    break;

    *ptr = 0;

    /* Search was ambiguous or successful */

    return (minaddr == maxaddr) ? 0 : 1;

}/* completion */
