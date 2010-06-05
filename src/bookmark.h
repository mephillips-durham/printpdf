/* PrintPDF - bookmark.h
 *
 * (c) Stephen Fryatt, 2010
 */

#ifndef _PRINTPDF_BOOKMARK
#define _PRINTPDF_BOOKARK

/* ==================================================================================================================
 * Static constants
 */

/* Optimization Window icons. */


#define MAX_BOOKMARK_LEN 64  /* The real maximum is 256, but Adobe recommend 32 max for practicality. */
#define BOOKMARK_TOOLBAR_HEIGHT 82
#define BOOKMARK_LINE_HEIGHT 60
#define BOOKMARK_ICON_HEIGHT 52
#define BOOKMARK_LINE_OFFSET 4
#define BOOKMARK_TOOLBAR_OFFSET 2

/* ==================================================================================================================
 * Data structures
 */

typedef struct bookmark_node
{
  char                 title[MAX_BOOKMARK_LEN];
  int                  destination;

  int                  expanded;

  struct bookmark_node *sub;
  struct bookmark_node *next;
}
bookmark_node;

typedef struct bookmark_redraw
{
  bookmark_node         *node;
  int                   indent;
  int                   selected;
}
bookmark_redraw;

typedef struct bookmark_block
{
  wimp_w                window;
  wimp_w                toolbar;

  bookmark_redraw       *redraw;
  int                   lines;

  bookmark_node         *root;

  struct bookmark_block *next;
}
bookmark_block;

typedef struct bookmark_params
{
  bookmark_block *bookmarks;
}
bookmark_params;

/* ==================================================================================================================
 * Function prototypes.
 */

/* Handle the PDFMark window and menu. */

void initialise_bookmarks (void);
void terminate_bookmarks(void);

void create_new_bookmark_window(wimp_pointer *pointer);
int close_bookmark_window(wimp_w window);
int redraw_bookmark_window(wimp_draw *redraw);

void initialise_bookmark_settings (bookmark_params *params);
void fill_bookmark_field (wimp_w window, wimp_i icon, bookmark_params *params);

#endif