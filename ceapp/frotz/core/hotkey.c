/*
 * hotkey.c
 *
 * Hot key functions
 *
 */

#include "frotz.h"

/*
 * hot_key_debugging
 *
 * ...allows user to toggle cheating options on/off.
 *
 */

static int hot_key_debugging (void)
{

    print_string ("Debugging options\n");

    option_attribute_assignment = read_yes_or_no ("Watch attribute assignment");
    option_attribute_testing = read_yes_or_no ("Watch attribute testing");
    option_object_movement = read_yes_or_no ("Watch object movement");
    option_object_locating = read_yes_or_no ("Watch object locating");

    return 0;

}/* hot_key_debugging */

/*
 * hot_key_help
 *
 * ...displays a list of all hot keys.
 *
 */

static int hot_key_help (void) {

    print_string ("Help\n");

    print_string ("\n\
Alt-D  debugging options\n\
Alt-H  help\n\
Alt-N  new game\n\
Alt-P  playback on\n\
Alt-R  recording on/off\n\
Alt-S  seed random numbers\n\
Alt-U  undo one turn\n\
Alt-X  exit game\n");

    return 0;

}/* hot_key_help */

/*
 * hot_key_playback
 *
 * ...allows user to turn playback on.
 *
 */

static int hot_key_playback (void)
{

    print_string ("Playback on\n");

    if (!istream_replay)
	replay_open ();

    return 0;

}/* hot_key_playback */

/*
 * hot_key_recording
 *
 * ...allows user to turn recording on/off.
 *
 */

static int hot_key_recording (void)
{

    if (istream_replay) {
	print_string ("Playback off\n");
	replay_close ();
    } else if (ostream_record) {
	print_string ("Recording off\n");
	record_close ();
    } else {
	print_string ("Recording on\n");
	record_open ();
    }

    return 0;

}/* hot_key_recording */

/*
 * hot_key_seed
 *
 * ...allows user to seed the random number seed.
 *
 */

static int hot_key_seed (void)
{

    print_string ("Seed random numbers\n");
    print_string ("Enter seed value (or return to randomize): ");

    seed_random (read_number ());

    return 0;

}/* hot_key_seed */

/*
 * hot_key_undo
 *
 * ...allows user to undo the previous turn.
 *
 */

static int hot_key_undo (void)
{

    print_string ("Undo one turn\n");

    if (restore_undo ()) {

	if (h_version >= V5) {		/* for V5+ games we must */
	    store (2);			/* store 2 (for success) */
	    return 1;			/* and abort the input   */
	}

	if (h_version <= V3) {		/* for V3- games we must */
	    z_show_status ();		/* draw the status line  */
	    return 0;			/* and continue input    */
	}

    } else print_string ("No more undo information available.\n");

    return 0;

}/* hot_key_undo */

/*
 * hot_key_restart
 *
 * ...allows user to start a new game.
 *
 */

static int hot_key_restart (void)
{

    print_string ("New game\n");

    if (read_yes_or_no ("Do you wish to restart")) {

	z_restart ();
	return 1;

    } else return 0;

}/* hot_key_restart */

/*
 * hot_key_quit
 *
 * ...allows user to exit the game.
 *
 */

static int hot_key_quit (void)
{

    print_string ("Exit game\n");

    if (read_yes_or_no ("Do you wish to quit")) {

	z_quit ();
	return 1;

    } else return 0;

}/* hot_key_quit */

/*
 * handle_hot_key
 *
 * Perform the action associated with a so-called hot key. Return
 * true to abort the current input action.
 *
 */

int handle_hot_key (int key)
{
    static int (*hot_keys[]) (void) = {
	hot_key_recording,
	hot_key_playback,
	hot_key_seed,
	hot_key_undo,
	hot_key_restart,
	hot_key_quit,
	hot_key_debugging,
	hot_key_help
    };

    if (cwin == 0) {

	print_string ("\nHot key -- ");

	if (hot_keys[key - HOT_KEY_MIN] ())
	    return 1;

	print_string ("\nContinue input...\n");

    }

    return 0;

}/* handle_hot_key */
