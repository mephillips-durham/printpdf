/* PrintPDF - main.c
 *
 * (c) Stephen Fryatt, 2007-2011
 */

/* ANSI C header files */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "flex.h"

/* OSLib header files */

#include "oslib/wimp.h"
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osspriteop.h"
#include "oslib/uri.h"
#include "oslib/hourglass.h"
#include "oslib/pdriver.h"
#include "oslib/help.h"

/* SF-Lib header files. */

#include "sflib/config.h"
#include "sflib/resources.h"
#include "sflib/heap.h"
#include "sflib/windows.h"
#include "sflib/icons.h"
#include "sflib/menus.h"
#include "sflib/transfer.h"
#include "sflib/url.h"
#include "sflib/msgs.h"
#include "sflib/debug.h"
#include "sflib/config.h"
#include "sflib/errors.h"
#include "sflib/string.h"
#include "sflib/colpick.h"
#include "sflib/event.h"

/* Application header files */

#include "main.h"

#include "bookmark.h"
#include "choices.h"
#include "convert.h"
#include "dataxfer.h"
#include "encrypt.h"
#include "iconbar.h"
#include "ihelp.h"
#include "menus.h"
#include "optimize.h"
#include "pdfmark.h"
#include "pmenu.h"
#include "popup.h"
#include "taskman.h"
#include "version.h"
#include "windows.h"

/* ------------------------------------------------------------------------------------------------------------------ */

static void	main_poll_loop(void);
static void	main_initialise(void);
static void	main_parse_command_line(int argc, char *argv[]);
static osbool	main_message_quit(wimp_message *message);
static osbool	main_message_prequit(wimp_message *message);

static void	mouse_click_handler(wimp_pointer *);
static void	key_press_handler(wimp_key *key);
static void	menu_selection_handler(wimp_selection *);


/*
 *Declare the global variables that are used.
 */

int			global_encryption_dialogue_location;
int			global_optimization_dialogue_location;
int			global_pdfmark_dialogue_location;
int			global_bookmark_dialogue_location;

/*
 * Cross file global variables
 */

wimp_t			main_task_handle;
int			main_quit_flag = FALSE;
osspriteop_area		*main_wimp_sprites;


/**
 * Main code entry point.
 */

int main(int argc, char *argv[])
{
	main_initialise();

	main_parse_command_line(argc, argv);

	main_poll_loop();

	terminate_bookmarks();
	msgs_close_file();
	wimp_close_down(main_task_handle);
	remove_all_remaining_conversions();

	return 0;
}


/**
 * Wimp Poll loop.
 */

static void main_poll_loop(void)
{
	os_t			poll_time;
	wimp_event_no		reason;
	wimp_block		blk;


	poll_time = os_read_monotonic_time();

	while (!main_quit_flag) {
		reason = wimp_poll_idle(0, &blk, poll_time, 0);

		/* Events are passed to Event Lib first; only if this fails
		 * to handle them do they get passed on to the internal
		 * inline handlers shown here.
		 */

		if (!event_process_event(reason, &blk, 0)) {
			switch (reason) {
			case wimp_NULL_REASON_CODE:
				popup_test_and_close(poll_time);
				check_for_ps_file();
				check_for_pending_files();
				poll_time += read_config_int("PollDelay");
				break;

			case wimp_OPEN_WINDOW_REQUEST:
				wimp_open_window(&(blk.open));
				break;

			case wimp_CLOSE_WINDOW_REQUEST:
				wimp_close_window(blk.close.w);
				break;

			case wimp_MOUSE_CLICK:
				mouse_click_handler(&(blk.pointer));
				break;

			case wimp_KEY_PRESSED:
				key_press_handler(&(blk.key));
				break;

			case wimp_MENU_SELECTION:
				menu_selection_handler(&(blk.selection));
				break;
			}
		}
	}
}


/**
 * Application initialisation.
 */

static void main_initialise(void)
{
	static char		task_name[255];
	char			resources[255], res_temp[255], filename[256];
	osspriteop_area		*sprites;

	wimp_MESSAGE_LIST(13)	message_list;
	wimp_version_no		wimp_version;


	hourglass_on();

	strcpy(resources, "<PrintPDF$Dir>.Resources");
	find_resource_path(resources, sizeof (resources));

	/* Load the messages file. */

	snprintf(res_temp, sizeof(res_temp), "%s.Messages", resources);
	msgs_init(res_temp);

	/* Initialise the error message system. */

	error_initialise("TaskName", "TaskSpr", NULL);

	/* Initialise with the Wimp. */

	message_list.messages[0]=message_URI_RETURN_RESULT;
	message_list.messages[1]=message_ANT_OPEN_URL;
	message_list.messages[2]=message_DATA_SAVE;
	message_list.messages[3]=message_DATA_SAVE_ACK;
	message_list.messages[4]=message_DATA_LOAD;
	message_list.messages[5]=message_DATA_OPEN;
	message_list.messages[6]=message_MENU_WARNING;
	message_list.messages[7]=message_MENUS_DELETED;
	message_list.messages[8]=message_HELP_REQUEST;
	message_list.messages[9]=message_TASK_INITIALISE;
	message_list.messages[10]=message_TASK_CLOSE_DOWN;
	message_list.messages[11]=message_PRE_QUIT;
	message_list.messages[12]=message_QUIT;

	msgs_lookup("TaskName", task_name, sizeof (task_name));
	main_task_handle = wimp_initialise(wimp_VERSION_RO3, task_name, (wimp_message_list *) &message_list, &wimp_version);

	/* Test to see if any other copies of PrintPDF are running, and set to quit if they are. */

	if (taskman_task_is_running(task_name, main_task_handle))
		main_quit_flag = TRUE;

	event_add_message_handler(message_QUIT, EVENT_MESSAGE_INCOMING, main_message_quit);
	event_add_message_handler(message_PRE_QUIT, EVENT_MESSAGE_INCOMING, main_message_prequit);

	/* Initialise the configuration. */

	initialise_configuration(task_name, "PrintPDF", "<PrintPDF$Dir>");

	init_config_str("FileQueue", "<Wimp$ScrapDir>.PrintPDF");
	init_config_str("ParamFile", "Pipe:$.PrintPDF");
	init_config_str("PDFMarkFile", "Pipe:$.PrintPDFMark");
	init_config_str("FileName", msgs_lookup ("FileName", filename, sizeof (filename)));
	init_config_int("PollDelay", 500);
	init_config_int("PopUpTime", 200);
	init_config_int("TaskMemory", 8192);
	init_config_int("PDFVersion", 0);
	init_config_int("Optimization", 0);
	init_config_opt("DownsampleMono", FALSE);
	init_config_int("DownsampleMonoType", 0);
	init_config_int("DownsampleMonoResolution", 300);
	init_config_int("DownsampleMonoThreshold", 15);
	init_config_int("DownsampleMonoDepth", -1);
	init_config_opt("DownsampleGrey", FALSE);
	init_config_int("DownsampleGreyType", 0);
	init_config_int("DownsampleGreyResolution", 72);
	init_config_int("DownsampleGreyThreshold", 15);
	init_config_int("DownsampleGreyDepth", -1);
	init_config_opt("DownsampleColour", FALSE);
	init_config_int("DownsampleColourType", 0);
	init_config_int("DownsampleColourResolution", 72);
	init_config_int("DownsampleColourThreshold", 15);
	init_config_int("DownsampleColourDepth", -1);
	init_config_opt("EncodeMono", TRUE);
	init_config_int("EncodeMonoType", 2);
	init_config_opt("EncodeGrey", TRUE);
	init_config_int("EncodeGreyType", 0);
	init_config_opt("EncodeColour", TRUE);
	init_config_int("EncodeColourType", 0);
	init_config_int("AutoPageRotation", 2);
	init_config_opt("CompressPages", TRUE);
	init_config_str("OwnerPasswd", "");
	init_config_str("UserPasswd", "");
	init_config_opt("AllowPrint", TRUE);
	init_config_opt("AllowFullPrint", TRUE);
	init_config_opt("AllowExtraction", TRUE);
	init_config_opt("AllowFullExtraction", TRUE);
	init_config_opt("AllowForms", TRUE);
	init_config_opt("AllowAnnotation", TRUE);
	init_config_opt("AllowModifications", TRUE);
	init_config_opt("AllowAssembly", TRUE);
	init_config_str("PDFMarkTitle", "");
	init_config_str("PDFMarkAuthor", "");
	init_config_str("PDFMarkSubject", "");
	init_config_str("PDFMarkKeywords", "");
	init_config_str("PDFMarkUserFile", "");
	init_config_opt("PreProcess", FALSE);
	init_config_opt("ResetParams", FALSE);
	init_config_opt("IconBarIcon", TRUE);
	init_config_opt("PopUpAfter", TRUE);

	load_configuration();

	/* Load the window templates. */

	sprites = load_user_sprite_area("<PrintPDF$Dir>.Sprites");

	main_wimp_sprites = sprites;

	snprintf(res_temp, sizeof(res_temp), "%s.Templates", resources);
	load_window_templates(res_temp, sprites);

	/* Load the menu structure. */

	snprintf(res_temp, sizeof(res_temp), "%s.Menus", resources);
	load_menu_definitions(res_temp);

	/* Initialise the postscript file queue and save box. */

	ihelp_initialise();
	taskman_initialise();
	popup_initialise();
	dataxfer_initialise();
	initialise_iconbar();
	initialise_conversion();
	initialise_bookmarks();

	url_initialise();

	/* Create an icon-bar icon. */

	set_iconbar_icon(read_config_opt("IconBarIcon") && (main_quit_flag == FALSE));

	hourglass_off();
}


/**
 * Take the command line and parse it for useful arguments.
 */

static void main_parse_command_line(int argc, char *argv[])
{
	int	i;

	if (argc > 1) {
		for (i=1; i<argc; i++) {
			if (strcmp (argv[i], "-file") == 0 && i+1 < argc)
				load_bookmark_file(argv[i+1]);
		}
	}
}


/**
 * Handle incoming Message_Quit.
 */

static osbool main_message_quit(wimp_message *message)
{
	main_quit_flag = TRUE;

	return TRUE;
}


/**
 * Handle incoming Message_PreQuit.
 */

static osbool main_message_prequit(wimp_message *message)
{
	if (!bookmark_files_unsaved() && !pending_files_in_queue())
		return TRUE;

	message->your_ref = message->my_ref;
	wimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE, message, message->sender);

	return TRUE;
}










/* ==================================================================================================================
 * Mouse click handler
 *
 * NB: This only handles mouse events that are not processed by Event Lib callbacks!
 */

static void mouse_click_handler (wimp_pointer *pointer)
{
  extern global_windows windows;

  /* Save PDF Window. */

  if (pointer->w == windows.save_pdf)
  {
    switch ((int) pointer->i)
    {
      case SAVE_PDF_ICON_FILE:
        if (pointer->buttons == wimp_DRAG_SELECT)
        {
          start_save_window_drag(DRAG_SAVE_PDF);
        }
        break;

      case SAVE_PDF_ICON_OK:
        if (pointer->buttons == wimp_CLICK_SELECT)
        {
          immediate_window_save ();
        }
        break;

      case SAVE_PDF_ICON_CANCEL:
        if (pointer->buttons == wimp_CLICK_SELECT)
        {
          cancel_conversion ();
        }
        break;

      case SAVE_PDF_ICON_QUEUE:
        if (pointer->buttons == wimp_CLICK_SELECT)
        {
          conversion_dialogue_queue ();
          wimp_close_window (windows.save_pdf);
        }
        break;

      case SAVE_PDF_ICON_VERSION_MENU:
        open_convert_version_menu (pointer, windows.save_pdf, SAVE_PDF_ICON_VERSION_FIELD);
        break;

      case SAVE_PDF_ICON_OPT_MENU:
        global_optimization_dialogue_location = OPTIMIZATION_SAVE;
        open_convert_optimize_menu (pointer, windows.save_pdf, SAVE_PDF_ICON_OPT_FIELD);
        break;

      case SAVE_PDF_ICON_ENCRYPT_MENU:
        global_encryption_dialogue_location = ENCRYPTION_SAVE;
        open_convert_encrypt_dialogue (pointer);
        break;

      case SAVE_PDF_ICON_PDFMARK_MENU:
        global_pdfmark_dialogue_location = PDFMARK_SAVE;
        open_convert_pdfmark_dialogue (pointer);
        break;

      case SAVE_PDF_ICON_BOOKMARK_MENU:
        global_bookmark_dialogue_location = BOOKMARK_SAVE;
        open_convert_bookmark_menu(pointer, windows.save_pdf, SAVE_PDF_ICON_BOOKMARK_FIELD);
        break;
    }
  }

  /* Either of the Security Windows. */

  else if (pointer->w == windows.security2 || pointer->w == windows.security3)
  {
    switch ((int) pointer->i)
    {
      case ENCRYPT_ICON_CANCEL:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      case ENCRYPT_ICON_OK:
        switch (global_encryption_dialogue_location)
        {
          case ENCRYPTION_SAVE:
            process_convert_encrypt_dialogue ();
            break;

          case ENCRYPTION_CHOICE:
            process_choices_encrypt_dialogue ();
            break;
        }
        break;
    }
  }

  /* The PDFMark Windows. */

  else if (pointer->w == windows.pdfmark)
  {
    switch ((int) pointer->i)
    {
      case PDFMARK_ICON_CANCEL:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      case PDFMARK_ICON_OK:
        switch (global_pdfmark_dialogue_location)
        {
          case PDFMARK_SAVE:
            process_convert_pdfmark_dialogue ();
            break;

          case PDFMARK_CHOICE:
            process_choices_pdfmark_dialogue ();
            break;
        }
        break;
    }
  }

  /* The Optimization Window. */

  else if (pointer->w == windows.optimization)
  {
    switch ((int) pointer->i)
    {
      case OPTIMIZE_ICON_CANCEL:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      case OPTIMIZE_ICON_OK:
        switch (global_optimization_dialogue_location)
        {
          case OPTIMIZATION_SAVE:
            process_convert_optimize_dialogue ();
            break;

          case OPTIMIZATION_CHOICE:
            process_choices_optimize_dialogue ();
            break;
        }
        break;

      case OPTIMIZE_ICON_COLOUR_RESOLUTION_UP:
        update_resolution_icon(OPTIMIZE_ICON_COLOUR_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_COLOUR_RESOLUTION_DOWN:
        update_resolution_icon(OPTIMIZE_ICON_COLOUR_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_GREY_RESOLUTION_UP:
        update_resolution_icon(OPTIMIZE_ICON_GREY_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_GREY_RESOLUTION_DOWN:
        update_resolution_icon(OPTIMIZE_ICON_GREY_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_MONO_RESOLUTION_UP:
        update_resolution_icon(OPTIMIZE_ICON_MONO_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_MONO_RESOLUTION_DOWN:
        update_resolution_icon(OPTIMIZE_ICON_MONO_RESOLUTION, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_COLOUR_THRESHOLD_UP:
        update_threshold_icon(OPTIMIZE_ICON_COLOUR_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_COLOUR_THRESHOLD_DOWN:
        update_threshold_icon(OPTIMIZE_ICON_COLOUR_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_GREY_THRESHOLD_UP:
        update_threshold_icon(OPTIMIZE_ICON_GREY_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_GREY_THRESHOLD_DOWN:
        update_threshold_icon(OPTIMIZE_ICON_GREY_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_MONO_THRESHOLD_UP:
        update_threshold_icon(OPTIMIZE_ICON_MONO_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_MONO_THRESHOLD_DOWN:
        update_threshold_icon(OPTIMIZE_ICON_MONO_THRESHOLD, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_COLOUR_DEPTH_UP:
        update_depth_icon(OPTIMIZE_ICON_COLOUR_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_COLOUR_DEPTH_DOWN:
        update_depth_icon(OPTIMIZE_ICON_COLOUR_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_GREY_DEPTH_UP:
        update_depth_icon(OPTIMIZE_ICON_GREY_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_GREY_DEPTH_DOWN:
        update_depth_icon(OPTIMIZE_ICON_GREY_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_MONO_DEPTH_UP:
        update_depth_icon(OPTIMIZE_ICON_MONO_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? -1 : 1);
        break;

      case OPTIMIZE_ICON_MONO_DEPTH_DOWN:
        update_depth_icon(OPTIMIZE_ICON_MONO_DEPTH, (pointer->buttons == wimp_CLICK_ADJUST) ? 1 : -1);
        break;

      case OPTIMIZE_ICON_COLOUR_DOWNSAMPLE:
      case OPTIMIZE_ICON_GREY_DOWNSAMPLE:
      case OPTIMIZE_ICON_MONO_DOWNSAMPLE:
      case OPTIMIZE_ICON_COLOUR_ENCODE:
      case OPTIMIZE_ICON_GREY_ENCODE:
      case OPTIMIZE_ICON_MONO_ENCODE:
        shade_optimize_dialogue ();
        break;

      case OPTIMIZE_ICON_COLOUR_SUBSAMPLE:
      case OPTIMIZE_ICON_GREY_SUBSAMPLE:
      case OPTIMIZE_ICON_MONO_SUBSAMPLE:
      case OPTIMIZE_ICON_COLOUR_AVERAGE:
      case OPTIMIZE_ICON_GREY_AVERAGE:
      case OPTIMIZE_ICON_MONO_AVERAGE:
      case OPTIMIZE_ICON_COLOUR_DCT:
      case OPTIMIZE_ICON_GREY_DCT:
      case OPTIMIZE_ICON_MONO_CCITT:
      case OPTIMIZE_ICON_COLOUR_FLATE:
      case OPTIMIZE_ICON_GREY_FLATE:
      case OPTIMIZE_ICON_MONO_FLATE:
      case OPTIMIZE_ICON_MONO_RUNLENGTH:
      case OPTIMIZE_ICON_ROTATE_NONE:
      case OPTIMIZE_ICON_ROTATE_ALL:
      case OPTIMIZE_ICON_ROTATE_PAGE:
        if (pointer->buttons == wimp_CLICK_ADJUST)
        {
          set_icon_selected (pointer->w, pointer->i, TRUE);
        }
        break;
    }
  }

  /* Queue window. */

  else if (pointer->w == windows.queue)
  {
    switch ((int) pointer->i)
    {
      case QUEUE_ICON_CLOSE:
        close_queue_window ();
        break;

      case QUEUE_ICON_CREATE:
        close_queue_window ();
        start_held_conversion ();
        break;
    }
  }

  /* Choices Window. */

  else if (pointer->w == windows.choices)
  {
    switch ((int) pointer->i)
    {
      case CHOICE_ICON_APPLY:
        if (pointer->buttons == wimp_CLICK_SELECT || pointer->buttons == wimp_CLICK_ADJUST)
        {
          read_choices_window ();

          if (pointer->buttons == wimp_CLICK_SELECT)
          {
            close_choices_window ();
          }
        }
        break;

      case CHOICE_ICON_SAVE:
        if (pointer->buttons == wimp_CLICK_SELECT || pointer->buttons == wimp_CLICK_ADJUST)
        {
          read_choices_window ();
          save_configuration ();

          if (pointer->buttons == wimp_CLICK_SELECT)
          {
            close_choices_window ();
          }
        }
        break;

      case CHOICE_ICON_CANCEL:
        if (pointer->buttons == wimp_CLICK_SELECT)
        {
          close_choices_window ();
        }
        else if (pointer->buttons == wimp_CLICK_ADJUST)
        {
          set_choices_window ();
          redraw_choices_window ();
        }
        break;

      case CHOICE_ICON_VERSION_MENU:
        open_choices_version_menu (pointer, windows.choices, CHOICE_ICON_VERSION);
        break;

      case CHOICE_ICON_OPTIMIZE_MENU:
        global_optimization_dialogue_location = OPTIMIZATION_CHOICE;
        open_choices_optimize_menu (pointer, windows.choices, CHOICE_ICON_OPTIMIZE);
        break;

      case CHOICE_ICON_ENCRYPT_MENU:
        global_encryption_dialogue_location = ENCRYPTION_CHOICE;
        open_choices_encrypt_dialogue (pointer);
        break;

      case CHOICE_ICON_INFO_MENU:
        global_pdfmark_dialogue_location = PDFMARK_CHOICE;
        open_choices_pdfmark_dialogue (pointer);
        break;
    }
  }
}

/* ==================================================================================================================
 * Keypress handler
 *
 * NB: This only handles key events that are not processed by Event Lib callbacks!
 */

static void key_press_handler (wimp_key *key)
{
  extern global_windows windows;


  /* Save PDF window */

  if (key->w == windows.save_pdf)
  {
    switch (key->c)
    {
      case wimp_KEY_RETURN:
        immediate_window_save ();
        break;

      case wimp_KEY_ESCAPE:
        cancel_conversion ();
        break;

      default:
        wimp_process_key (key->c);
        break;
    }
  }

  /* The PDFMark Window. */

  else if (key->w == windows.pdfmark)
  {
    switch (key->c)
    {
      case wimp_KEY_RETURN:
        switch (global_pdfmark_dialogue_location)
        {
          case PDFMARK_SAVE:
            process_convert_pdfmark_dialogue ();
            break;

          case PDFMARK_CHOICE:
            process_choices_pdfmark_dialogue ();
            break;
        }
        break;

      case wimp_KEY_ESCAPE:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      default:
        wimp_process_key (key->c);
        break;
    }
  }

  /* Either of the Security Windows. */

  else if (key->w == windows.security2 || key->w == windows.security3)
  {
    if (key->i == ENCRYPT_ICON_OWNER_PW)
    {
      shade_encrypt_dialogue (0);
    }

    switch (key->c)
    {
      case wimp_KEY_RETURN:
        switch (global_encryption_dialogue_location)
        {
          case ENCRYPTION_SAVE:
            process_convert_encrypt_dialogue ();
            break;

          case ENCRYPTION_CHOICE:
            process_choices_encrypt_dialogue ();
            break;
        }
        break;

      case wimp_KEY_ESCAPE:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      default:
        wimp_process_key (key->c);
        break;
    }
  }

  /* The Optimization Window. */

  else if (key->w == windows.optimization)
  {
    switch (key->c)
    {
      case wimp_KEY_RETURN:
        switch (global_optimization_dialogue_location)
        {
          case OPTIMIZATION_SAVE:
            process_convert_optimize_dialogue ();
            break;

          case OPTIMIZATION_CHOICE:
            process_choices_optimize_dialogue ();
            break;
        }
        break;

      case wimp_KEY_ESCAPE:
        wimp_create_menu ((wimp_menu *) -1, 0, 0);
        break;

      default:
        wimp_process_key (key->c);
        break;
    }
  }


  /* Choices window */

  else if (key->w == windows.choices)
  {
    switch (key->c)
    {
      case wimp_KEY_RETURN:
        read_choices_window ();
        save_configuration ();
        close_choices_window ();
        break;

      case wimp_KEY_ESCAPE:
        close_choices_window ();
        break;

      default:
        wimp_process_key (key->c);
        break;
    }
  }
}

/* ==================================================================================================================
 * Menu selection handler
 */

static void menu_selection_handler (wimp_selection *selection)
{
	wimp_pointer		pointer;
	wimp_menu		*old_menu;

	extern global_menus	menus;


	/* Store the mouse status before decoding the menu. */

	wimp_get_pointer_info(&pointer);
	old_menu = menus.menu_up;

	/* Decode the individual menus. */

	if (menus.menu_up == menus.params)
		decode_param_menu(selection, &pointer);
	else if (menus.menu_up == menus.bookmarks_list)
		process_convert_bookmark_menu(selection);

	/* If Adjust was used, reopen the menu.  If the menu block has moved,
	 * the menu is just closed -- this might be because the block has
	 * had to be grown in malloc().
	 */

	if (pointer.buttons == wimp_CLICK_ADJUST && old_menu == menus.menu_up)
		wimp_create_menu(menus.menu_up, 0, 0);
}

