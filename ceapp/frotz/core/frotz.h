/*
 * frotz.h
 *
 * Global declarations and definitions
 *
 */

typedef unsigned char zbyte;
typedef unsigned short zword;

#define ZBYTE(v) ((zbyte) (v))
#define ZWORD(v) ((zword) (v))

/*** Constants that may be set at compile time ***/

#ifndef MAX_UNDO_SLOTS
#define MAX_UNDO_SLOTS 5
#endif
#ifndef MAX_FILE_NAME
#define MAX_FILE_NAME 80
#endif
#ifndef TEXT_BUFFER_SIZE
#define TEXT_BUFFER_SIZE 200
#endif
#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE 200
#endif
#ifndef STACK_SIZE
#define STACK_SIZE 1024
#endif

#ifndef DEFAULT_SAVE_NAME
#define DEFAULT_SAVE_NAME "story.sav"
#endif
#ifndef DEFAULT_SCRIPT_NAME
#define DEFAULT_SCRIPT_NAME "story.scr"
#endif
#ifndef DEFAULT_COMMAND_NAME
#define DEFAULT_COMMAND_NAME "story.rec"
#endif
#ifndef DEFAULT_AUXILARY_NAME
#define DEFAULT_AUXILARY_NAME "story.aux"
#endif

/*** Story file header format ***/

#define H_VERSION 0
#define H_CONFIG 1
#define H_RELEASE 2
#define H_RESIDENT_SIZE 4
#define H_START_PC 6
#define H_DICTIONARY 8
#define H_OBJECTS 10
#define H_GLOBALS 12
#define H_DYNAMIC_SIZE 14
#define H_FLAGS 16
#define H_SERIAL 18
#define H_ABBREVIATIONS 24
#define H_FILE_SIZE 26
#define H_CHECKSUM 28
#define H_INTERPRETER_NUMBER 30
#define H_INTERPRETER_VERSION 31
#define H_SCREEN_ROWS 32
#define H_SCREEN_COLS 33
#define H_SCREEN_WIDTH 34
#define H_SCREEN_HEIGHT 36
#define H_FONT_HEIGHT 38 /* this is the font width in V5 */
#define H_FONT_WIDTH 39 /* this is the font height in V5 */
#define H_FUNCTIONS_OFFSET 40
#define H_STRINGS_OFFSET 42
#define H_DEFAULT_BACKGROUND 44
#define H_DEFAULT_FOREGROUND 45
#define H_TERMINATING_KEYS 46
#define H_LINE_WIDTH 48
#define H_STANDARD_HIGH 50
#define H_STANDARD_LOW 51
#define H_ALPHABET 52
#define H_MOUSE_TABLE 54
#define H_USER_NAME 56

/*** Various Z-machine constants ***/

#define V1 1
#define V2 2
#define V3 3
#define V4 4
#define V5 5
#define V6 6
#define V7 7
#define V8 8

#define CONFIG_BYTE_SWAPPED 0x01 /* Story file is byte swapped         - V3  */
#define CONFIG_COLOUR       0x01 /* Interpr supports colour            - V5+ */
#define CONFIG_TIME         0x02 /* Status line displays time          - V3  */
#define CONFIG_PICTURES	    0x02 /* Interpr supports pictures	       - V6  */
#define CONFIG_BOLDFACE     0x04 /* Interpr supports boldface style    - V4+ */
#define CONFIG_TANDY        0x08 /* Tandy licensed game                - V3  */
#define CONFIG_EMPHASIS     0x08 /* Interpr supports emphasis style    - V4+ */
#define CONFIG_NOSTATUSLINE 0x10 /* Interpr can't support status lines - V3  */
#define CONFIG_FIXED        0x10 /* Interpr supports fixed width style - V4+ */
#define CONFIG_SPLITSCREEN  0x20 /* Interpr supports split screen mode - V3  */
#define CONFIG_PROPORTIONAL 0x40 /* Interpr uses proportional font     - V3  */
#define CONFIG_TIMEDINPUT   0x80 /* Interpr supports timed input       - V4+ */

#define SCRIPTING_FLAG	  0x0001 /* Outputting to transscription file  - V1+ */
#define FIXED_FONT_FLAG   0x0002 /* Use fixed width font               - V3+ */
#define REFRESH_FLAG 	  0x0004 /* Refresh the screen                 - V6  */
#define GRAPHICS_FLAG	  0x0008 /* Game wants to use graphics         - V5+ */
#define OLD_SOUND_FLAG	  0x0010 /* Game wants to use sound effects    - V3  */
#define UNDO_FLAG	  0x0010 /* Game wants to use UNDO feature     - V5+ */
#define MOUSE_FLAG	  0x0020 /* Game wants to use a mouse          - V5+ */
#define COLOUR_FLAG	  0x0040 /* Game wants to use colours          - V5+ */
#define SOUND_FLAG	  0x0080 /* Game wants to use sound effects    - V5+ */
#define MENU_FLAG	  0x0100 /* Game wants to use menus            - V6  */

#define INTERP_DEC_20 1
#define INTERP_APPLE_IIE 2
#define INTERP_MACINTOSH 3
#define INTERP_AMIGA 4
#define INTERP_ATARI_ST 5
#define INTERP_MSDOS 6
#define INTERP_CBM_128 7
#define INTERP_CBM_64 8
#define INTERP_APPLE_IIC 9
#define INTERP_APPLE_IIGS 10
#define INTERP_TANDY 11

#define BLACK_COLOUR 2
#define RED_COLOUR 3
#define GREEN_COLOUR 4
#define YELLOW_COLOUR 5
#define BLUE_COLOUR 6
#define MAGENTA_COLOUR 7
#define CYAN_COLOUR 8
#define WHITE_COLOUR 9
#define GREY_COLOUR 10       /* INTERP_MSDOS only */
#define LIGHTGREY_COLOUR 10  /* INTERP_AMIGA only */
#define MEDIUMGREY_COLOUR 11 /* INTERP_AMIGA only */
#define DARKGREY_COLOUR 12   /* INTERP_AMIGA only */

#define REVERSE_STYLE 1
#define BOLDFACE_STYLE 2
#define EMPHASIS_STYLE 4
#define FIXED_WIDTH_STYLE 8

#define TEXT_FONT 1
#define PICTURE_FONT 2
#define GRAPHICS_FONT 3
#define FIXED_WIDTH_FONT 4

/*** Other constants ***/

#define NEW_STYLE 1
#define NEW_FONT 2

#define FILE_RESTORE 0
#define FILE_SAVE 1
#define FILE_SCRIPT 2
#define FILE_PLAYBACK 3
#define FILE_RECORD 4
#define FILE_LOAD_AUX 5
#define FILE_SAVE_AUX 6

#define EURO_MIN 155
#define EURO_MAX 223

#define HOT_KEY_MIN 1000
#define HOT_KEY_RECORDING 1000
#define HOT_KEY_PLAYBACK 1001
#define HOT_KEY_SEED 1002
#define HOT_KEY_UNDO 1003
#define HOT_KEY_RESTART 1004
#define HOT_KEY_QUIT 1005
#define HOT_KEY_DEBUGGING 1006
#define HOT_KEY_HELP 1007
#define HOT_KEY_MAX 1007

#define BEYOND_ZORK 1
#define SHERLOCK 2

/*** Data access macros ***/

#define SET_BYTE(addr,v)  { zmp[addr] = (zbyte) v; }
#define LOW_BYTE(addr,v)  { v = zmp[addr]; }
#define CODE_BYTE(v)	  { v = *(pcp++); }

#if defined (AMIGA)

extern zbyte *pcp;
extern zbyte *zmp;

#define lo(v)	((zbyte *)&v)[1]
#define hi(v)	((zbyte *)&v)[0]

#define SET_WORD(addr,v)  { zmp[addr] = hi (v); zmp[addr + 1] = lo (v); }
#define LOW_WORD(addr,v)  { hi (v) = zmp[addr]; lo (v) = zmp[addr+1]; }
#define HIGH_WORD(addr,v) { hi (v) = zmp[addr]; lo (v) = zmp[addr+1]; }
#define CODE_WORD(v)      { hi (v) = *pcp++; lo (v) = *pcp++; }
#define GET_PC(v)         { v = pcp - zmp; }
#define SET_PC(addr)      { pcp = zmp + v; }

#endif

#if defined (__MSDOS__)

extern zbyte far *pcp;
extern zbyte far *zmp;

#define lo(v)	((zbyte *)&v)[0]
#define hi(v)	((zbyte *)&v)[1]

#define SET_WORD(addr,v) asm {\
    les bx,zmp;\
    add bx,addr;\
    mov ax,v;\
    xchg al,ah;\
    mov es:[bx],ax }

#define LOW_WORD(addr,v) asm {\
    les bx,zmp;\
    add bx,addr;\
    mov ax,es:[bx];\
    xchg al,ah;\
    mov v,ax }

#define HIGH_WORD(addr,v) asm {\
    mov bx,word ptr zmp;\
    add bx,word ptr addr;\
    mov al,bh;\
    mov bh,0;\
    mov ah,0;\
    adc ah,byte ptr addr+2;\
    mov cl,4;\
    shl ax,cl;\
    add ax,word ptr zmp+2;\
    mov es,ax;\
    mov ax,es:[bx];\
    xchg al,ah;\
    mov v,ax }

#define CODE_WORD(v) asm {\
    les bx,pcp;\
    mov ax,es:[bx];\
    xchg al,ah;\
    mov v,ax;\
    add word ptr pcp,2 }

#define GET_PC(v) asm {\
    mov bx,word ptr pcp+2;\
    sub bx,word ptr zmp+2;\
    mov al,bh;\
    mov cl,4;\
    shl bx,cl;\
    shr al,cl;\
    add bx,word ptr pcp;\
    adc al,0;\
    sub bx,word ptr zmp;\
    sbb al,0;\
    mov ah,0;\
    mov word ptr v,bx;\
    mov word ptr v+2,ax }

#define SET_PC(v) asm {\
    mov bx,word ptr zmp;\
    add bx,word ptr v;\
    mov al,bh;\
    mov bh,0;\
    mov ah,0;\
    adc ah,byte ptr v+2;\
    mov cl,4;\
    shl ax,cl;\
    add ax,word ptr zmp+2;\
    mov word ptr pcp,bx;\
    mov word ptr pcp+2,ax }

#endif

#if !defined (AMIGA) && !defined (__MSDOS__)

extern zbyte *pcp;
extern zbyte *zmp;

#define lo(v)	(v & 0xff)
#define hi(v)	(v >> 8)

#define SET_WORD(addr,v)  { zmp[addr] = hi(v); zmp[addr+1] = lo(v); }
#define LOW_WORD(addr,v)  { v = ((zword) zmp[addr] << 8) | zmp[addr+1]; }
#define HIGH_WORD(addr,v) { v = ((zword) zmp[addr] << 8) | zmp[addr+1]; }
#define CODE_WORD(v)      { v = ((zword) pcp[0] << 8) | pcp[1]; pcp += 2; }
#define GET_PC(v)         { v = pcp - zmp; }
#define SET_PC(v)         { pcp = zmp + v; }

#endif

/*** Story file header data ***/

extern zbyte h_version;
extern zbyte h_config;
extern zword h_release;
extern zword h_resident_size;
extern zword h_start_pc;
extern zword h_dictionary;
extern zword h_objects;
extern zword h_globals;
extern zword h_dynamic_size;
extern zword h_flags;
extern zbyte h_serial[6];
extern zword h_abbreviations;
extern zword h_file_size;
extern zword h_checksum;
extern zbyte h_interpreter_number;
extern zbyte h_interpreter_version;
extern zbyte h_screen_rows;
extern zbyte h_screen_cols;
extern zword h_screen_width;
extern zword h_screen_height;
extern zbyte h_font_height;
extern zbyte h_font_width;
extern zword h_functions_offset;
extern zword h_strings_offset;
extern zbyte h_default_background;
extern zbyte h_default_foreground;
extern zword h_terminating_keys;
extern zword h_line_width;
extern zbyte h_standard_high;
extern zbyte h_standard_low;
extern zword h_alphabet;
extern zword h_mouse_table;
extern zbyte h_user_name[8];

/*** Various data ***/

extern char *story_name;
extern long story_size;

extern story_id;

extern zword stack[STACK_SIZE];
extern zword *sp;
extern zword *fp;

extern zword zargs[8];
extern zargc;

extern ostream_screen;
extern ostream_script;
extern ostream_memory;
extern ostream_record;

extern istream_replay;

extern cwin;
extern mwin;

extern mouse_x;
extern mouse_y;

extern enable_wrapping;
extern enable_scripting;
extern enable_scrolling;
extern enable_buffering;

extern pause_flag;

extern option_attribute_assignment;
extern option_attribute_testing;
extern option_object_locating;
extern option_object_movement;
extern option_context_lines;
extern option_left_margin;
extern option_right_margin;
extern option_ignore_errors;
extern option_piracy;
extern option_undo_slots;
extern option_expand_abbreviations;
extern option_script_cols;

extern long reserve_mem;

/*** Z-machine opcodes ***/

/* math.... */ 	void 	z_add (void);
/* math.... */ 	void 	z_and (void);
/* math.... */ 	void 	z_art_shift (void);
/* screen.. */	void 	z_buffer_mode (void);
/* process. */	void 	z_call_n (void);
/* process. */	void 	z_call_s (void);
/* process. */	void 	z_catch (void);
/* process. */	void 	z_check_arg_count (void);
/* object.. */	void 	z_clear_attr (void);
/* table... */	void 	z_copy_table (void);
/* variable */	void 	z_dec (void);
/* variable */	void 	z_dec_chk (void);
/* math.... */ 	void 	z_div (void);
/* screen.. */	void 	z_draw_picture (void);
/* text.... */	void 	z_encode_text (void);
/* screen.. */	void 	z_erase_line (void);
/* screen.. */	void 	z_erase_picture (void);
/* screen.. */	void 	z_erase_window (void);
/* object.. */	void 	z_get_child (void);
/* screen.. */ 	void 	z_get_cursor (void);
/* object.. */	void 	z_get_next_prop (void);
/* object.. */	void 	z_get_parent (void);
/* object.. */	void 	z_get_prop (void);
/* object.. */	void 	z_get_prop_addr (void);
/* object.. */	void 	z_get_prop_len (void);
/* object.. */	void 	z_get_sibling (void);
/* screen.. */	void 	z_get_wind_prop (void);
/* variable */	void 	z_inc (void);
/* variable */	void 	z_inc_chk (void);
/* stream.. */	void 	z_input_stream (void);
/* object.. */	void 	z_insert_obj (void);
/* math.... */ 	void 	z_je (void);
/* math.... */ 	void 	z_jg (void);
/* object.. */	void 	z_jin (void);
/* math.... */ 	void 	z_jl (void);
/* process. */ 	void 	z_jump (void);
/* math.... */ 	void 	z_jz (void);
/* variable */	void 	z_load (void);
/* table... */	void 	z_loadb (void);
/* table... */	void 	z_loadw (void);
/* math.... */	void 	z_log_shift (void);
/* input... */	void 	z_make_menu (void);
/* math.... */ 	void 	z_mod (void);
/* screen.. */	void 	z_mouse_window (void);
/* screen.. */	void 	z_move_window (void);
/* math.... */ 	void 	z_mul (void);
/* text.... */	void 	z_new_line (void);
/* process. */	void 	z_nop (void);
/* math.... */ 	void 	z_not (void);
/* math.... */ 	void 	z_or (void);
/* stream.. */	void 	z_output_stream (void);
/* screen.. */	void 	z_picture_data (void);
/* screen.. */	void 	z_picture_table (void);
/* main.... */ 	void 	z_piracy (void);
/* variable */ 	void 	z_pop (void);
/* variable */	void 	z_pop_stack (void);
/* text.... */	void 	z_print (void);
/* text.... */	void 	z_print_addr (void);
/* text.... */	void 	z_print_char (void);
/* text.... */	void 	z_print_form (void);
/* text.... */	void 	z_print_num (void);
/* text.... */	void 	z_print_obj (void);
/* text.... */	void 	z_print_paddr (void);
/* text.... */	void 	z_print_ret (void);
/* screen.. */	void 	z_print_table (void);
/* variable */	void 	z_pull (void);
/* variable */ 	void 	z_push (void);
/* variable */ 	void 	z_push_stack (void);
/* object.. */	void 	z_put_prop (void);
/* screen.. */	void 	z_put_wind_prop (void);
/* process. */	void 	z_quit (void);
/* random.. */	void 	z_random (void);
/* input... */ 	void 	z_read (void);
/* input... */	void 	z_read_char (void);
/* input... */	void 	z_read_mouse (void);
/* object.. */	void 	z_remove_obj (void);
/* fastmem. */	void 	z_restart (void);
/* fastmem. */	void 	z_restore (void);
/* fastmem. */	void 	z_restore_undo (void);
/* process. */	void 	z_ret (void);
/* process. */	void 	z_ret_popped (void);
/* process. */	void 	z_rfalse (void);
/* process. */	void 	z_rtrue (void);
/* fastmem. */	void 	z_save (void);
/* fastmem. */	void 	z_save_undo (void);
/* table... */	void 	z_scan_table (void);
/* screen.. */	void 	z_scroll_window (void);
/* object.. */	void 	z_set_attr (void);
/* screen.. */	void 	z_set_font (void);
/* screen.. */	void 	z_set_colour (void);
/* screen.. */	void 	z_set_cursor (void);
/* screen.. */	void 	z_set_margins (void);
/* screen.. */	void 	z_set_window (void);
/* screen.. */	void 	z_set_text_style (void);
/* text.... */ 	void 	z_show_status (void);
/* sound... */	void 	z_sound_effect (void);
/* screen.. */	void 	z_split_window (void);
/* variable */	void 	z_store (void);
/* table... */	void 	z_storeb (void);
/* table... */	void 	z_storew (void);
/* math.... */ 	void 	z_sub (void);
/* math.... */  void 	z_test (void);
/* object.. */	void 	z_test_attr (void);
/* process. */	void 	z_throw (void);
/* text.... */	void 	z_tokenise (void);
/* fastmem. */	void 	z_verify (void);
/* screen.. */	void 	z_window_size (void);
/* screen.. */	void 	z_window_style (void);

/*** IO machinery ***/

/* files... */	int 	replay_read_key (void);
/* files... */	int 	replay_read_input (char *);
/* screen.. */	int 	console_read_key (int);
/* screen.. */	int 	console_read_input (int, char *, int, int);
/* stream.. */ 	int 	stream_read_key (zword, zword, int);
/* stream.. */ 	int 	stream_read_input (int, char *, zword, zword, int, int);

/* redirect */	void 	memory_open (zword, zword);
/* redirect */	void 	memory_close (void);
/* redirect */	void 	memory_word (const char *);
/* redirect */	void 	memory_new_line (void);
/* files... */ 	void 	record_open (void);
/* files... */ 	void 	record_close (void);
/* files... */ 	void 	record_write_key (int);
/* files... */ 	void 	record_write_input (const char *, int);
/* files... */ 	void 	replay_open (void);
/* files... */ 	void 	replay_close (void);
/* files... */	void 	script_open (void);
/* files... */	void 	script_close (void);
/* files... */ 	void 	script_word (const char *);
/* files... */	void 	script_new_line (void);
/* files... */ 	void 	script_write_input (const char *, int);
/* files... */ 	void 	script_erase_input (const char *);
/* files... */ 	void 	script_mssg_on (void);
/* files... */ 	void 	script_mssg_off (void);
/* screen.. */	void 	screen_word (const char *);
/* screen.. */	void 	screen_new_line (void);
/* screen.. */	void 	screen_write_input (const char *, int);
/* screen.. */	void 	screen_erase_input (const char *);
/* screen.. */	void 	screen_mssg_on (void);
/* screen.. */	void 	screen_mssg_off (void);
/* stream.. */	void 	stream_word (const char *);
/* stream.. */	void 	stream_new_line (void);
/* stream.. */	void 	stream_mssg_on (void);
/* stream.. */	void 	stream_mssg_off (void);

/*** Various global functions ***/

/* buffer.. */	void 	flush_buffer (void);
/* buffer.. */	void	new_line (void);
/* buffer.. */	void	print_char (int);
/* fastmem. */	void 	init_memory (void);
/* fastmem. */	void 	init_undo (void);
/* fastmem. */	void 	reset_memory (void);
/* fastmem. */	void 	restart_header (void);
/* fastmem. */	int  	save_undo (void);
/* fastmem. */	int  	restore_undo (void);
/* fastmem. */	void	storeb (zword, zbyte);
/* fastmem. */	void	storew (zword, zword);
/* hotkey.. */	int  	handle_hot_key (int);
/* input... */	int  	is_terminator (int);
/* input... */	int	read_number (void);
/* input... */	void	read_string (int, char *);
/* input... */	int	read_yes_or_no (const char *);
/* main.... */	void	runtime_error (const char *);
/* object.. */	zword	object_name (zword);
/* process. */  void	call (zword, int, zword *, int);
/* process. */	void	ret (zword);
/* process. */	void 	branch (int);
/* process. */	void 	store (zword);
/* process. */	void 	interpret (void);
/* process. */	int  	direct_call (zword);
/* random.. */	void 	seed_random (int);
/* screen.. */	int	get_max_width (int);
/* screen.. */	void 	resize_screen (void);
/* screen.. */	void 	restart_screen (void);
/* screen.. */	void 	refresh_text_style (void);
/* screen.. */	void	set_more_prompts (int);
/* screen.. */	void	split_window (zword);
/* screen.. */	int	validate_click (void);
/* sound... */	void	end_of_sound (void);
/* text.... */	int	completion (const char *, char *);
/* text.... */  void	print_num (zword);
/* text.... */	void	print_object (zword);
/* text.... */	void 	print_string (const char *);
/* text.... */	void	tokenise_line (zword, zword, zword, zword);

/*** Interface functions ***/

void 	os_beep (int);
int  	os_char_width (int);
void 	os_cursor_off (void);
void 	os_cursor_on (void);
void 	os_display_char (int);
void 	os_display_string (const char *);
void 	os_draw_picture (int, int, int);
void 	os_erase_area (int, int, int, int);
void 	os_fatal (const char *);
void 	os_finish_with_sample (void);
int  	os_font_data (int, int *, int *);
void 	os_init_screen (void);
void 	os_more_prompt (void);
int  	os_peek_colour (void);
int  	os_picture_data (int, int *, int *);
void 	os_prepare_sample (int);
void 	os_process_arguments (int, char *[]);
int	os_random_seed (void);
int  	os_read_file_name (char *, const char *, int);
int  	os_read_key (int);
int  	os_read_line (int, char *, int, int, int);
void 	os_reset_screen (void);
void 	os_scroll_area (int, int, int, int, int);
void 	os_set_colour (int, int);
void 	os_set_cursor (int, int);
void 	os_set_font (int);
void 	os_set_text_style (int);
void 	os_start_sample (int, int, int);
void 	os_stop_sample (void);
int  	os_string_width (const char *);
void 	os_wait_sample (void);
