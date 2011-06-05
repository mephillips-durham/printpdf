/* PrintPDF - encrypt.h
 *
 * (c) Stephen Fryatt, 2005-2011
 */

#ifndef PRINTPDF_ENCRYPT
#define PRINTPDF_ENCRYPT


#define MAX_PASSWORD 50


typedef struct encrypt_params {
	char		owner_password[MAX_PASSWORD];
	char		access_password[MAX_PASSWORD];

	osbool		allow_print;
	osbool		allow_full_print;
	osbool		allow_extraction;
	osbool		allow_full_extraction;
	osbool		allow_forms;
	osbool		allow_annotation;
	osbool		allow_modifications;
	osbool		allow_assembly;
} encrypt_params;


/**
 * Initialise the encryption dialogue.
 */

void encrypt_initialise(void);


/**
 * Initialise the values in an encryption settings structure.
 *
 * \param *params		The encryption params struct to be initialised.
 */

void encrypt_initialise_settings(encrypt_params *params);


/**
 * Set a callback handler to be called when the OK button of the
 * encryption dialogue is clicked.
 *
 * \param callback		The callback function to use, or NULL.
 */

void encrypt_set_dialogue_callback(void (*callback)(void));


/**
 * Open the encryption dialogue for the given parameter block.
 *
 * \param *params		The encryption parameter block to be used.
 * \param *extended_opts	TRUE if the extended options should be offered.
 * \param *pointer		The current pointer state.
 */

void encrypt_open_dialogue(encrypt_params *params, osbool extended_opts, wimp_pointer *pointer);


/**
 * Store the settings from the currently open encryption dialogue box in
 * an encryption parameter block.
 *
 * \param *params		The encryption parameter block to be used.
 */

void encrypt_process_dialogue(encrypt_params *params);


/**
 * Update the given text field icon with a status reflecting the settings
 * in the given encryption parameter block.
 *
 * \param window		The window containing the icon.
 * \param icon			The icon to update.
 * \param *params		The encryption parameter block to use.
 */

void encrypt_fill_field(wimp_w window, wimp_i icon, encrypt_params *params);


/**
 * Build up a text string in the supplied buffer containing the GS
 * parameters that reflect the contents of the given encryption
 * parameter block.
 *
 * \param *buffer		Buffer to hold the result.
 * \param len			The size of the buffer.
 * \param *params		The encryption parameter block to translate.
 * \param extended_opts		TRUE to use the extended range of options; else FALSE.
 */

void encryption_build_params(char *buffer, size_t len, encrypt_params *params, osbool extended_opts);

#endif

