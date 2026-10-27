/*
 * main.c
 *
 * Frotz V2.22 main function
 *
 * This is an interpreter for Infocom V1 to V6 games. It also supports
 * the recently defined V7 and V8 games. Please report bugs to
 *
 *    jokisch@ls7.informatik.uni-dortmund.de
 *
 * Frotz is freeware. It may be used and distributed freely provided
 * no commercial profit is involved. (c) 1995-1997 Stefan Jokisch
 *
 */

#include "frotz.h"

#ifndef __MSDOS__
#define cdecl
#endif

#ifdef _WINDOWS
#include "windows.h"
#endif

/* Story file name and size */

char *story_name = 0;
long story_size = 0;

/* Auto-detection for a few buggy story files */

int story_id = 0;

/* Story file header data */

zbyte h_version = 0;
zbyte h_config = 0;
zword h_release = 0;
zword h_resident_size = 0;
zword h_start_pc = 0;
zword h_dictionary = 0;
zword h_objects = 0;
zword h_globals = 0;
zword h_dynamic_size = 0;
zword h_flags = 0;
zbyte h_serial[6] = { 0, 0, 0, 0, 0, 0 };
zword h_abbreviations = 0;
zword h_file_size = 0;
zword h_checksum = 0;
zbyte h_interpreter_number = 0;
zbyte h_interpreter_version = 0;
zbyte h_screen_rows = 0;
zbyte h_screen_cols = 0;
zword h_screen_width = 0;
zword h_screen_height = 0;
zbyte h_font_height = 1;
zbyte h_font_width = 1;
zword h_functions_offset = 0;
zword h_strings_offset = 0;
zbyte h_default_background = 0;
zbyte h_default_foreground = 0;
zword h_terminating_keys = 0;
zword h_line_width = 0;
zbyte h_standard_high = 0;
zbyte h_standard_low = 2;
zword h_alphabet = 0;
zword h_mouse_table = 0;
zbyte h_user_name[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Stack data */

zword stack[STACK_SIZE];
zword *sp = 0;
zword *fp = 0;

/* IO streams */

int ostream_screen = 1;
int ostream_script = 0;
int ostream_memory = 0;
int ostream_record = 0;

int istream_replay = 0;

/* Current and mouse window */

int cwin = 0;
int mwin = 0;

/* Mouse data */

int mouse_y = 0;
int mouse_x = 0;

/* Window attributes */

int enable_wrapping = 0;
int enable_scripting = 0;
int enable_scrolling = 0;
int enable_buffering = 0;

/* Flag used to fix a problem in The Lurking Horror */

int pause_flag = 0;

/* User options */

int option_attribute_assignment = 0;
int option_attribute_testing = 0;
int option_context_lines = 0;
int option_object_locating = 0;
int option_object_movement = 0;
int option_left_margin = 0;
int option_right_margin = 0;
int option_ignore_errors = 0;
int option_piracy = 0;
int option_undo_slots = MAX_UNDO_SLOTS;
int option_expand_abbreviations = 0;
int option_script_cols = 80;

/* Size of memory to reserve (in bytes) */

long reserve_mem = 0;

/*
 * runtime_error
 *
 * An error has occured. Ignore it or pass it to os_fatal.
 *
 */

void runtime_error (const char *s)
{

    flush_buffer ();

    if (!option_ignore_errors)
	os_fatal (s);

}/* runtime_error */

/*
 * z_piracy, branch if the story file is a legal copy.
 *
 *	no zargs used
 *
 */

void z_piracy (void)
{

    branch (!option_piracy);

}/* z_piracy */

/*
 * main
 *
 * Prepare and run the game.
 *
 */

#ifdef _WINDOWS
#ifdef _WIN32_WCE
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
int argc;
char *argv[1] = {"FrotzCE"};
#else
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
int argc;
char *argv[1] = {"Frotz32"};
#endif
#else
int cdecl main (int argc, char *argv[])
{
#endif

    os_process_arguments (argc, argv);

    init_memory ();

    os_init_screen ();

    init_undo ();

    z_restart ();

    interpret ();

    reset_memory ();

    os_reset_screen ();

    return 0;

}/* main */
