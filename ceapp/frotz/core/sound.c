/*
 * sound.c
 *
 * Sound effect function
 *
 */

#include "frotz.h"

static zword routine = 0;

/*
 * end_of_sound
 *
 * Call the Z-code routine which was given as the last parameter of
 * a sound_effect call. This function may be called from a hardware
 * interrupt (which requires extremely careful programming).
 *
 */

void end_of_sound (void)
{

    direct_call (routine);

}/* end_of_sound */

/*
 * z_sound_effect, load / play / stop / discard a sound effect.
 *
 *   	zargs[0] = number of bleep (1 or 2) or sample (>= 3)
 *	zargs[1] = operation to perform (samples only)
 *	zargs[2] = repeats (hi-byte) / volume (lo-byte) (play sample only)
 *	zargs[3] = end-of-sound routine (play sample only, optional)
 *
 * Note: Volumes range from 1 to 8, volume 255 is the default volume.
 *       255 repeats is infinite. There aren't repeats in V3.
 *
 */

void z_sound_effect (void)
{

    /* Supply default arguments */

    if (zargc < 4)
	zargs[3] = 0;

    /* Bleep or sample */

    if (zargs[0] >= 3) {

	routine = 0;	/* prevent any end-of-sound interrupts */

	/* The following is a workaround for The Lurking Horror. Close to
	   the end of the game a sequence of samples is played during one
	   turn. On slow Amigas this isn't a major problem, but on a fast
	   machine each sample is soon stopped by the next one. Therefore,
	   if no input took place since the last sample was started, wait
	   for the current sample to finish. */

	if (h_version == V3 && pause_flag)
	    os_wait_sample ();

	/* Load (#1), play (#2), stop (#3) or remove (#4) sample */

	switch (zargs[1]) {

	case 1:
	    os_prepare_sample (zargs[0]);
	    break;

	case 2:
	    os_start_sample (zargs[0], lo (zargs[2]), hi (zargs[2]));
	    routine = zargs[3];
	    pause_flag = 1;
	    break;

	case 3:
	    os_stop_sample ();
	    break;

	case 4:
	    os_finish_with_sample ();
	    break;

	}

    } else os_beep (zargs[0]);

}/* z_sound_effect */
