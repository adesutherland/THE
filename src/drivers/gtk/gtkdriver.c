/* GTK is a projection of the real editor's driver surfaces. All editor
 * mutations are queued for the editor owner, including menu and tool actions. */
#include <gtk/gtk.h>
#include <string.h>

#include "the.h"
#include "proto.h"
#include "vars.h"
#include "driverlayout.h"
#include "driverwindow.h"
#include "headlessdriver.h"
#include "editorsource.h"
#include "editorclipboard.h"
#include "panelui.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "workbenchform.h"
#include "workbenchfilefields.h"
#include "tools.h"
#include "gtkinput.h"

#define GTK_TEXT_LIMIT (1024u * 1024u)
#define GTK_QUEUE_LIMIT 4096u
#define GTK_EDITOR_MIN_COLUMNS 80
#define GTK_CONSOLE_COLOURS (ATTR_CONSOLESTATUS - ATTR_CONSOLEBODY + 1)

typedef struct
{
   size_t line, offset;
} GtkDocumentCursor;

typedef struct
{
   TheDriverWindow *window;
   int visible;
} GtkSurface;

typedef struct
{
   TheInputEvent event;
   char *text;
   const char *cursor;
   TheWorkbenchFormRequest *form;
   TheWorkbenchFileFieldsRequest *filefields;
   char clipboard[16];
} GtkInput;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1], error[64];
   unsigned long generation;
   uint64_t revision, pending, receipt;
   int blocked;
} GtkFormSession;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   GtkWidget *entry;
   void *pane;
   int editable;
} GtkFormField;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   GtkWidget *entry;
   void *pane;
   int width;
   int caret, selected, selection_start, selection_end;
} GtkFileField;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   unsigned long generation;
   uint64_t revision, next_cursor;
   GtkWidget *page, *view, *scroller, *status;
   GdkFrameClock *scroll_clock;
   GtkAdjustment *scroll_adjustment;
   gulong scroll_handler;
   double scroll_value;
   struct { uint64_t cursor; int character; } records[THE_WORKBENCH_LOG_RECORD_MAX];
   size_t record_count;
   GtkDocumentCursor document_first, document_next, document_previous[32];
   size_t document_previous_count, document_lines;
   int document_more, document_request, document_budget, document_tail;
   GPtrArray *form_fields;
   unsigned long form_generation;
   GtkWidget *form_submit, *form_cancel, *form_return, *form_hide;
   int form_syncing;
   GPtrArray *filefields;
   uint64_t filefields_revision, filefields_pending, filefields_receipt;
   GtkWidget *filefields_reload, *filefields_focus;
   int filefields_syncing, filefields_rebuild;
   char filefields_error[96];
} GtkLogPane;

typedef struct
{
   GtkWidget *page, *view, *scroller, *latest, *tab;
   GString *text;
   unsigned version, job, unread;
   size_t bytes;
   uint64_t displayed, streams, history_epoch;
   int omitted;
   GdkFrameClock *scroll_clock;
   GtkAdjustment *scroll_adjustment;
   gulong scroll_handler;
   double scroll_value;
   int syncing, scroll_frames;
} GtkConsoleReader;

static struct
{
   GtkWidget *window, *split, *editor, *console, *output, *input, *state;
   GtkWidget *run, *stop, *eof, *send, *header, *first_control, *menu_focus;
   GtkWidget *build, *apply, *edit_profile, *apply_image;
   GtkWidget *task_row, *task_state, *file_title, *setup, *setup_tabs, *profile_summary, *tools_view;
   GtkWidget *settings, *settings_status, *expand_control;
   GtkWidget *tool_fields[5], *tool_role, *runner, *runner_arguments;
   int setup_visible, expanded, font_unsaved;
   int dragging, drag_started, drag_motion, drag_row, drag_col;
   char font_status[256];
   char *catalog_signature;
   GtkWidget *menu_button_origin;
   GtkWidget *build_result, *build_output, *build_state, *build_profile, *build_stop;
   GtkConsoleReader program_reader, build_reader;
   TheToolsFileSnapshot file_snapshot;
   unsigned result_serial;
   int build_hidden;
   GtkApplication *application;
   GtkIMContext *im;
   PangoFontDescription *font;
   GList *surfaces;
   GQueue inputs;
   size_t queued_bytes;
   int active, timeout, cell_width, cell_height, rows, cols;
   int cursor_visible, blink_visible;
   int split_positioned;
   GtkCssProvider *console_css;
   GtkCssProvider *filefields_css, *font_css;
   TheDriverAttr console_colours[GTK_CONSOLE_COLOURS];
   int console_colours_valid;
   guint tick, split_layout_idle;
   TheDriverWindow *input_window, *menu_popup;
   int menu_canceled, menu_entry_position, menu_selection_start, menu_selection_end;
   int menu_entry_selected;
   GMenu *menu_model;
   GtkWidget *native_menu;
   unsigned long menu_revision;
   GPtrArray *menu_actions;
   GtkWidget *auxiliary, *log_origin, *last_nonlog_focus;
   GPtrArray *logs;
   char focused_log[THE_WORKBENCH_ID_MAX + 1];
   unsigned long focused_log_generation, log_focus_serial;
   int syncing_logs, log_origin_program, log_entry_position;
   int log_selection_start, log_selection_end, log_entry_selected;
   GtkFormSession forms[THE_WORKBENCH_FORM_MAX];
   size_t form_count;
   uint64_t form_sequence;
   GtkFormSession *form_pending;
   uint64_t filefields_sequence, filefields_pending;
   ThePanelState *filefields_scratch;
} ui;

static TheDriverOps operations;
static void sync_menus(void);
static void sync_logs(void);
static GtkLogPane *log_for_widget(GtkWidget *widget);
static int queue_panel(GtkLogPane *pane, TheWorkbenchPanelOperation operation);
static void remember_log_origin(GtkWidget *focus);
static void cycle_log(GtkLogPane *pane, int reverse);
static gboolean document_key(GtkLogPane *pane, guint key);
static gboolean document_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data);
static GtkFormSession *form_session(const char *id);
static void form_receipt(void);
static void sync_form(GtkLogPane *pane, const TheWorkbenchPanel *panel);
static void create_form(GtkLogPane *pane, const TheWorkbenchPanel *panel, GtkWidget *controls);
static gboolean form_key(GtkLogPane *pane, GtkWidget *focus, GdkEventKey *key);
static void focus_form(GtkLogPane *pane);
static GtkLogPane *find_log(const char *id, unsigned long generation);
static void filefields_receipt(void);
static void sync_filefields(GtkLogPane *pane, const TheWorkbenchPanel *panel);
static GtkLogPane *create_filefields(const TheWorkbenchPanel *panel);
static gboolean filefields_key(GtkLogPane *pane, GtkWidget *focus, GdkEventKey *key);
static void focus_filefields(GtkLogPane *pane);
static void open_native_menu(GtkWidget *focus, GdkEventKey *trigger);
static void show_tool_page(GtkWidget *page);
static void show_setup(unsigned section);
static int clipboard_action(const char *name, GtkWidget *focus);
static void consume_clipboard(const char *name);
static void apply_font(const PangoFontDescription *font);
static void show_settings(void);
static void show_about(void);
static void focus_editor(void);

static void redraw(void)
{
   if (ui.editor)
      gtk_widget_queue_draw(ui.editor);
}

static void message(const char *text)
{
   GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ui.window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", text);
   gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);
}

static int queue_event(TheInputEvent event)
{
   GtkInput *input;
   if (g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT)
   {
      gtk_widget_error_bell(ui.editor);
      return 0;
   }
   input = g_new0(GtkInput, 1);
   input->event = event;
   g_queue_push_tail(&ui.inputs, input);
   return 1;
}

static void queue_text(const char *text)
{
   GtkInput *input;
   size_t length = text ? strlen(text) : 0;
   if (!length)
      return;
   if (length > GTK_TEXT_LIMIT || ui.queued_bytes > GTK_TEXT_LIMIT - length
   || g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT
   || !g_utf8_validate(text, length, NULL))
   {
      message("Text input must be valid UTF-8 and fit in the 1 MiB input queue.");
      return;
   }
   input = g_new0(GtkInput, 1);
   input->text = g_strdup(text);
   input->cursor = input->text;
   ui.queued_bytes += length;
   g_queue_push_tail(&ui.inputs, input);
}

static int pop_input(TheInputEvent *event)
{
   GtkInput *input = g_queue_peek_head(&ui.inputs);
   if (!input)
      return 0;
   form_receipt();
   filefields_receipt();
   input = g_queue_peek_head(&ui.inputs);
   if (!input) return 0;
   if (ui.filefields_pending) return 0;
   if (input->filefields)
   {
      GtkLogPane *pane = find_log(input->filefields->panel, input->filefields->panel_generation);
      const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(input->filefields->panel);
      int recovery = input->filefields->operation == THE_WORKBENCH_FILEFIELDS_RELOAD || input->filefields->operation == THE_WORKBENCH_FILEFIELDS_CANCEL;
      if (!pane || !state || (!recovery && pane->filefields_revision != state->draft_revision))
      {
         if (pane) strcpy(pane->filefields_error, "stale-revision; Reload or Cancel");
         g_queue_pop_head(&ui.inputs); g_free(input->filefields); g_free(input); return 0;
      }
      input->filefields->draft_revision = state->draft_revision;
      input->filefields->request_id = ++ui.filefields_sequence;
      if (!the_input_event_from_workbench_filefields(input->filefields, event))
      { g_queue_pop_head(&ui.inputs); g_free(input->filefields); g_free(input); return 0; }
      ui.filefields_pending = pane->filefields_pending = input->filefields->request_id;
      g_queue_pop_head(&ui.inputs); g_free(input->filefields); g_free(input); return 1;
   }
   if (input->form)
   {
      GtkFormSession *session = form_session(input->form->form);
      const TheWorkbenchFormState *state = the_workbench_form_state(input->form->form);
      if (ui.form_pending) return 0;
      if (!session || !state || session->blocked || session->generation != input->form->form_generation || session->revision != state->draft_revision)
      {
         if (session) { session->blocked = 1; strcpy(session->error, "stale-revision; refocus form"); }
         g_queue_pop_head(&ui.inputs); g_free(input->form); g_free(input); return 0;
      }
      input->form->draft_revision = session->revision;
      input->form->request_id = ++ui.form_sequence;
      if (!the_input_event_from_workbench_form(input->form, event))
      { g_queue_pop_head(&ui.inputs); g_free(input->form); g_free(input); return 0; }
      session->pending = input->form->request_id; ui.form_pending = session;
      g_queue_pop_head(&ui.inputs); g_free(input->form); g_free(input);
      return 1;
   }
   if (input->clipboard[0])
   {
      char name[16];
      strcpy(name, input->clipboard);
      g_queue_pop_head(&ui.inputs); g_free(input);
      consume_clipboard(name);
      /* The owner transaction has already rendered. Yield a handled empty
       * input for the normal refresh, without an intervening editor command
       * that would invalidate its immediate Undo group. */
      *event = the_input_event_none();
      return 1;
   }
   if (input->text)
   {
      if (!the_gtk_input_text(&input->cursor, event))
         *event = the_input_event_none();
      if (*input->cursor)
         return event->kind != THE_INPUT_NONE;
      ui.queued_bytes -= strlen(input->text);
      g_free(input->text);
   }
   else
      *event = input->event;
   g_queue_pop_head(&ui.inputs);
   g_free(input);
   if (ui.menu_popup && event->kind == THE_INPUT_KEY)
      ui.menu_canceled = event->key_code == 27;
   return event->kind != THE_INPUT_NONE;
}

static void queue_command(const char *command)
{
   TheInputEvent event;
   if (the_panel_active || in_macro)
   {
      gtk_widget_error_bell(ui.editor);
      return;
   }
   if (the_input_event_from_command(command, &event))
      queue_event(event);
   focus_editor();
}

static void command_action(GtkWidget *widget, gpointer command)
{
   (void)widget;
   queue_command(command);
}

static void tool_action(GtkWidget *widget, gpointer action)
{
   TheInputEvent event;
   if (the_panel_active || in_macro)
      return;
   if ((widget == ui.stop || widget == ui.eof) && !the_tool_program_console()->running)
      return;
   if (the_tools_prepare_input(action, "", "", 0, NULL, &event))
      queue_event(event);
   focus_editor();
}

static void current_file_action(GtkWidget *widget, gpointer data)
{
   static const char *const names[] = {"file-build", "file-run", "file-activate"};
   unsigned index = GPOINTER_TO_UINT(data);
   const TheToolsFileActionSnapshot *action;
   TheInputEvent event;
   char generation[32];
   (void)widget;
   if (the_panel_active || in_macro || index >= THE_TOOLS_FILE_ACTION_COUNT) return;
   action = ui.file_snapshot.actions + index;
   if (!action->enabled) return;
   g_snprintf(generation, sizeof(generation), "%lu", action->generation);
   /* Bind to what the user saw. The owner rejects changes queued ahead of
    * this request rather than silently running a newly selected source. */
   if (the_tools_prepare_lines_bound(names[index], action->tool, "", "",
       ui.file_snapshot.source_id, ui.file_snapshot.revision, generation, &event) && queue_event(event))
   {
      if (index != 1) ui.build_hidden = 0;
      show_tool_page(index == 1 ? ui.console : ui.build_result);
   }
}

static int queue_profile_image(const char *path)
{
   TheInputEvent event;
   if (!the_tools_prepare_input("apply-profile", "rxbin", path, 0, NULL, &event) || !queue_event(event)) return 0;
   ui.build_hidden = 0;
   show_tool_page(ui.build_result);
   return 1;
}

static void apply_image_clicked(GtkWidget *widget, gpointer data)
{
   GtkWidget *dialog;
   GtkFileFilter *filter;
   (void)widget; (void)data;
   if (the_panel_active || in_macro || !ui.file_snapshot.profile.apply_image.enabled) return;
   dialog = gtk_file_chooser_dialog_new("Apply compiled profile", GTK_WINDOW(ui.window),
      GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Apply", GTK_RESPONSE_ACCEPT, NULL);
   filter = gtk_file_filter_new();
   gtk_file_filter_set_name(filter, "Compiled profiles (*.profile.rxbin)");
   gtk_file_filter_add_pattern(filter, "*.profile.rxbin");
   gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
   if (CURRENT_FILE) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), (char *)CURRENT_FILE->fpath);
   if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
   {
      char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      if (!queue_profile_image(path)) message("The profile request could not be queued.");
      g_free(path);
   }
   gtk_widget_destroy(dialog);
}

static void file_action(GtkWidget *widget, gpointer name)
{
   const char *action = name;
   TheInputEvent event;
   (void)widget;
   if (the_panel_active || in_macro)
      return;
   if (!strcmp(action, "file.open") || !strcmp(action, "file.create"))
   {
      int creating = !strcmp(action, "file.create");
      GtkWidget *dialog = gtk_file_chooser_dialog_new(
         creating ? "New file" : "Open file", GTK_WINDOW(ui.window),
         creating ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
         "_Cancel", GTK_RESPONSE_CANCEL, creating ? "_Create" : "_Open",
         GTK_RESPONSE_ACCEPT, NULL);
      if (CURRENT_FILE)
         gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                              (char *)CURRENT_FILE->fpath);
      if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
      {
         char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
         if (the_input_event_from_action(action, path, &event))
            queue_event(event);
         else
            message("This path exceeds the editor limit or contains a quote or control character.");
         g_free(path);
      }
      gtk_widget_destroy(dialog);
   }
   else if (the_input_event_from_action(action, "", &event))
      queue_event(event);
   focus_editor();
}

/* Native fields retain GTK editing semantics. Editor transactions enter at
 * pop_input, on the editor owner, after a menu has released its grab. */
static int clipboard_action(const char *name, GtkWidget *focus)
{
   GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
   if (!focus) focus = ui.editor;
   if (GTK_IS_ENTRY(focus))
   {
      if (!strcmp(name, "edit.copy")) gtk_editable_copy_clipboard(GTK_EDITABLE(focus));
      else if (!strcmp(name, "edit.cut")) gtk_editable_cut_clipboard(GTK_EDITABLE(focus));
      else if (!strcmp(name, "edit.paste"))
      { if (focus == ui.input) g_signal_emit_by_name(focus, "paste-clipboard"); else gtk_editable_paste_clipboard(GTK_EDITABLE(focus)); }
      else if (!strcmp(name, "edit.select-all")) gtk_editable_select_region(GTK_EDITABLE(focus), 0, -1);
      else return 0;
   }
   else if (GTK_IS_TEXT_VIEW(focus))
   {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(focus));
      if (!strcmp(name, "edit.copy")) gtk_text_buffer_copy_clipboard(buffer, clipboard);
      else if (!strcmp(name, "edit.cut")) gtk_text_buffer_cut_clipboard(buffer, clipboard, gtk_text_view_get_editable(GTK_TEXT_VIEW(focus)));
      else if (!strcmp(name, "edit.paste")) gtk_text_buffer_paste_clipboard(buffer, clipboard, NULL, gtk_text_view_get_editable(GTK_TEXT_VIEW(focus)));
      else if (!strcmp(name, "edit.select-all"))
      { GtkTextIter first, last; gtk_text_buffer_get_bounds(buffer, &first, &last); gtk_text_buffer_select_range(buffer, &first, &last); }
      else return 0;
   }
   else if (focus == ui.editor)
   {
      GtkInput *input;
      if (the_panel_active || in_macro || g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT) return 0;
      input = g_new0(GtkInput, 1);
      g_strlcpy(input->clipboard, name, sizeof(input->clipboard));
      g_queue_push_tail(&ui.inputs, input);
   }
   else return 0; /* A control is never an implicit editor destination. */
   return 1;
}

static void clipboard_supply(GtkClipboard *clipboard, GtkSelectionData *selection, guint info, gpointer data)
{ (void)clipboard; (void)info; gtk_selection_data_set_text(selection, data, -1); }
static void clipboard_clear(GtkClipboard *clipboard, gpointer data)
{ (void)clipboard; g_free(data); }

static int clipboard_write(const unsigned char *bytes, size_t length)
{
   GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
   GtkTargetList *list = gtk_target_list_new(NULL, 0);
   GtkTargetEntry *targets;
   int count, accepted;
   char *copy = g_strndup((const char *)bytes, length);
   gtk_target_list_add_text_targets(list, 0);
   targets = gtk_target_table_new_from_list(list, &count);
   accepted = gtk_clipboard_set_with_data(clipboard, targets, count, clipboard_supply, clipboard_clear, copy);
   gtk_target_table_free(targets, count); gtk_target_list_unref(list);
   if (accepted) gtk_clipboard_set_can_store(clipboard, NULL, 0);
   else g_free(copy);
   return accepted;
}

static void consume_clipboard(const char *name)
{
   TheEditorClipboardCapture capture = {0};
   short rc;
   if (the_panel_active || in_macro) return;
   if (!strcmp(name, "edit.select-all")) rc = the_editor_clipboard_select_all();
   else
   {
      int copying = strcmp(name, "edit.paste") != 0;
      rc = the_editor_clipboard_capture(copying, &capture);
      if (!rc && copying)
      {
         if (!clipboard_write(capture.bytes, capture.length))
         { the_editor_clipboard_release(&capture); message("The system clipboard could not accept the selection. The file was unchanged."); return; }
         if (!strcmp(name, "edit.cut")) rc = the_editor_clipboard_cut(capture.token);
      }
      else if (!rc)
      {
         char *text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
         if (text) rc = the_editor_clipboard_paste(capture.token, (const unsigned char *)text, strlen(text));
         else { the_editor_clipboard_release(&capture); message("The clipboard has no available text. The file was unchanged."); return; }
         g_free(text);
      }
      the_editor_clipboard_release(&capture);
   }
   if (rc) message(the_editor_clipboard_error_message());
   redraw();
}

static void send_input(GtkWidget *widget, gpointer data)
{
   const char *text = gtk_entry_get_text(GTK_ENTRY(ui.input));
   TheInputEvent event;
   (void)widget; (void)data;
   if (the_panel_active || in_macro)
   {
      gtk_widget_error_bell(ui.input);
      return;
   }
   if (!the_tool_program_console()->running || the_tool_program_console()->eof) return;
   if (the_tools_prepare_input("input", "", text, 0, NULL, &event)
   && queue_event(event))
   {
      gtk_entry_set_text(GTK_ENTRY(ui.input), "");
      gtk_widget_grab_focus(ui.input);
   }
}

static void program_input_paste(GtkEntry *entry, gpointer data)
{
   char *text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
   int position;
   (void)data;
   g_signal_stop_emission_by_name(entry, "paste-clipboard");
   if (!text) return;
   if (strchr(text, '\n') || strchr(text, '\r'))
      message("Program input accepts one record at a time. Multiline paste was refused; the current draft is unchanged.");
   else if (gtk_editable_get_editable(GTK_EDITABLE(entry)))
   {
      gtk_editable_delete_selection(GTK_EDITABLE(entry));
      position = gtk_editable_get_position(GTK_EDITABLE(entry));
      gtk_editable_insert_text(GTK_EDITABLE(entry), text, -1, &position);
      gtk_editable_set_position(GTK_EDITABLE(entry), position);
   }
   g_free(text);
}

static void program_input_insert(GtkEditable *entry, const char *text, int length, int *position, gpointer data)
{
   (void)position; (void)data;
   if (memchr(text, '\n', length) || memchr(text, '\r', length))
   {
      g_signal_stop_emission_by_name(entry, "insert-text");
      message("Program input accepts one record at a time. Multiline paste was refused; the current draft is unchanged.");
   }
}

static gboolean close_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   (void)widget; (void)event; (void)data;
   /* CANCEL closes unchanged buffers and leaves changed buffers open. Its
    * save checks include the pending record; it never discards changed text. */
   queue_command("cancel");
   return TRUE;
}

static void im_commit(GtkIMContext *context, const char *text, gpointer data)
{
   (void)context; (void)data;
   queue_text(text);
}

static void remember_menu_focus(GtkWidget *focus)
{
   g_set_object(&ui.menu_focus, (focus ? focus : ui.editor));
   ui.menu_popup = NULL;
   ui.menu_canceled = 0;
   ui.menu_entry_selected = 0;
   if (ui.menu_focus && GTK_IS_ENTRY(ui.menu_focus))
   {
      ui.menu_entry_position = gtk_editable_get_position(GTK_EDITABLE(ui.menu_focus));
      ui.menu_entry_selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.menu_focus),
         &ui.menu_selection_start, &ui.menu_selection_end);
   }
}

static void finish_menu_focus(void)
{
   if (ui.menu_canceled && ui.menu_focus)
   {
      GtkWidget *focus = gtk_widget_is_drawable(ui.menu_focus)
                     && gtk_widget_is_sensitive(ui.menu_focus) ? ui.menu_focus : ui.editor;
      gtk_widget_grab_focus(focus);
      if (GTK_IS_ENTRY(focus))
      {
         if (ui.menu_entry_selected)
            gtk_editable_select_region(GTK_EDITABLE(focus),
               ui.menu_entry_position == ui.menu_selection_start
                  ? ui.menu_selection_end : ui.menu_selection_start,
               ui.menu_entry_position);
         else
            gtk_editable_set_position(GTK_EDITABLE(focus), ui.menu_entry_position);
      }
   }
   g_clear_object(&ui.menu_focus);
   ui.menu_popup = NULL;
   ui.menu_canceled = 0;
}

/* Only lower the stock shared-menu binding. Profiles retain authority to
 * redefine F10; modal READ and active macros always keep their own keys. */
static int shared_menu_key_binding(void)
{
   const char *definition = (const char *)get_key_definition(THE_KEY_F(10), THE_KEY_DEFINE_RAW, TRUE, FALSE);
   return definition && (!my_stricmp((DEFCHAR *)definition, "macro workbenchmenu")
      || !my_stricmp((DEFCHAR *)definition, "macro workbenchmenu_regina"));
}

static gboolean workbench_key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(ui.window));
   TheInputEvent event;
   (void)widget; (void)data;
   if (!the_panel_active && !in_macro && !ui.input_window)
   {
      guint lower = gdk_keyval_to_lower(key->keyval);
      const char *clip = lower == GDK_KEY_c ? "edit.copy" : lower == GDK_KEY_x ? "edit.cut"
                       : lower == GDK_KEY_v ? "edit.paste" : lower == GDK_KEY_a ? "edit.select-all" : NULL;
#ifdef GDK_WINDOWING_QUARTZ
      int shortcut = (key->state & GDK_META_MASK) != 0;
#else
      int shortcut = (key->state & GDK_CONTROL_MASK) != 0;
#endif
      if (shortcut && lower == GDK_KEY_z && (GTK_IS_ENTRY(focus) || GTK_IS_TEXT_VIEW(focus)))
      { gtk_widget_error_bell(focus); return TRUE; }
      if (shortcut && !(key->state & (GDK_MOD1_MASK | GDK_SHIFT_MASK)) && clip)
      {
         /* Native fields already own these bindings. File editing only lowers
          * an unassigned control key; an explicit profile binding wins. */
         if (focus != ui.editor || the_tools_frontend_key((unsigned char)g_ascii_toupper(lower)))
            if (clipboard_action(clip, focus)) return TRUE;
      }
      GtkLogPane *pane = log_for_widget(focus);
      if (pane)
      {
         const TheWorkbenchPanel *kind = the_workbench_panel_find(pane->id);
         if (kind && kind->kind == THE_WORKBENCH_PANEL_FIELDS && key->keyval != GDK_KEY_F10)
            return kind->document_id ? filefields_key(pane, focus, key) : form_key(pane, focus, key);
         if (key->keyval == GDK_KEY_Escape) return queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
         if (key->keyval == GDK_KEY_h || key->keyval == GDK_KEY_H ||
             ((key->state & GDK_CONTROL_MASK) && key->keyval == GDK_KEY_w))
            return queue_panel(pane, THE_WORKBENCH_PANEL_HIDE);
         if (key->keyval == GDK_KEY_Tab || key->keyval == GDK_KEY_ISO_Left_Tab)
         { cycle_log(pane, key->keyval == GDK_KEY_ISO_Left_Tab || (key->state & GDK_SHIFT_MASK)); return TRUE; }
         {
            const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
            if (panel && panel->kind == THE_WORKBENCH_PANEL_DOCUMENT && focus == pane->view &&
                key->keyval != GDK_KEY_F10 && key->keyval != GDK_KEY_F6)
            {
               if (((key->state & (GDK_CONTROL_MASK | GDK_META_MASK | GDK_SUPER_MASK)) &&
                     (key->keyval == GDK_KEY_c || key->keyval == GDK_KEY_C || key->keyval == GDK_KEY_a || key->keyval == GDK_KEY_A)) ||
                   ((key->state & GDK_SHIFT_MASK) && (key->keyval == GDK_KEY_Left || key->keyval == GDK_KEY_Right ||
                     key->keyval == GDK_KEY_Up || key->keyval == GDK_KEY_Down || key->keyval == GDK_KEY_Home || key->keyval == GDK_KEY_End)))
                  return FALSE; /* Native selection/copy remains read-only. */
               return document_key(pane, key->keyval);
            }
         }
      }
   }
   if (the_panel_active || in_macro || ui.input_window
   || key->state & (GDK_CONTROL_MASK | GDK_META_MASK | GDK_MOD1_MASK))
      return FALSE;
   if (key->keyval == GDK_KEY_ISO_Left_Tab
   || (key->keyval == GDK_KEY_Tab && key->state & GDK_SHIFT_MASK))
   {
      /* This moves physical widget focus only. The core retains the exact
       * file/command/prefix caret; ordinary Tab still reaches its profile. */
      if (focus == ui.editor && gtk_widget_get_visible(ui.auxiliary))
      {
         GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary)));
         if (page == ui.setup && gtk_widget_get_visible(ui.setup) && gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.setup_tabs)) == 2) gtk_widget_grab_focus(ui.build_output);
         else if (page == ui.setup && gtk_widget_get_visible(ui.setup)) gtk_widget_child_focus(ui.setup, GTK_DIR_TAB_FORWARD);
         else if (gtk_widget_get_visible(ui.console)) gtk_widget_grab_focus(gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output);
         else gtk_widget_grab_focus(ui.first_control);
      }
      else if (focus == ui.editor || (focus && gtk_widget_is_ancestor(focus, ui.auxiliary)))
         gtk_widget_grab_focus(ui.first_control);
      else
         focus_editor();
      return TRUE;
   }
   if (focus != ui.editor && key->keyval == GDK_KEY_Escape)
   {
      focus_editor();
      return TRUE;
   }
   if (key->keyval == GDK_KEY_F10 && shared_menu_key_binding())
   {
      open_native_menu(focus, key);
      return TRUE;
   }
   if ((key->keyval == GDK_KEY_F10 || (focus != ui.editor && key->keyval == GDK_KEY_F6))
   && the_gtk_input_key(key->keyval, key->state, &event) && queue_event(event))
   {
      focus_editor();
      return TRUE;
   }
   return FALSE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   TheInputEvent event;
   gunichar character;
   (void)widget; (void)data;
   if (workbench_key_pressed(widget, key, data))
      return TRUE;
   if (the_gtk_input_key(key->keyval, key->state, &event))
   {
      gtk_im_context_reset(ui.im);
      queue_event(event);
      return TRUE;
   }
   if (gtk_im_context_filter_keypress(ui.im, key))
      return TRUE;
   if (key->state & (GDK_CONTROL_MASK | GDK_META_MASK | GDK_MOD1_MASK))
      return FALSE;
   character = gdk_keyval_to_unicode(key->keyval);
   if (g_unichar_isprint(character)
   && the_input_event_from_text(character, &event))
   {
      queue_event(event);
      return TRUE;
   }
   return FALSE;
}

static gboolean key_released(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   (void)widget; (void)data;
   return gtk_im_context_filter_keypress(ui.im, key);
}

static void editor_realized(GtkWidget *widget, gpointer data)
{
   (void)data;
   gtk_im_context_set_client_window(ui.im, gtk_widget_get_window(widget));
}

static gboolean editor_focus(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
   (void)widget; (void)data;
   if (event->in)
      gtk_im_context_focus_in(ui.im);
   else
      gtk_im_context_focus_out(ui.im);
   redraw();
   return FALSE;
}

static void allocated(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
   (void)widget; (void)data;
   ui.rows = CLAMP(allocation->height / ui.cell_height, 12, 256);
   ui.cols = CLAMP(allocation->width / ui.cell_width, 40, THE_MAX_SCREEN_WIDTH);
}

static gboolean position_split(gpointer data)
{
   GtkAllocation allocation;
   (void)data;
   ui.split_layout_idle = 0;
   gtk_widget_get_allocation(ui.split, &allocation);
   gtk_paned_set_position(GTK_PANED(ui.split),
      gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == GTK_ORIENTATION_HORIZONTAL
         ? allocation.width * 2 / 3 : MAX(120, allocation.height / 2));
   return G_SOURCE_REMOVE;
}

static void split_allocated(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
   int console_width = 0;
   GtkOrientation orientation;
   /* Fit a useful editor grid beside the console using the current font and
    * theme's real widget minimums. A pixel-only breakpoint needlessly stacks
    * the console below the editor at the default desktop window width. */
   if (ui.auxiliary)
      gtk_widget_get_preferred_width(ui.auxiliary, &console_width, NULL);
   {
      GtkWidget *selected = ui.auxiliary ? gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary),
         gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) : NULL;
      GtkLogPane *pane = log_for_widget(selected);
      const TheWorkbenchPanel *panel = pane ? the_workbench_panel_find(pane->id) : NULL;
      if (panel) console_width = MAX(console_width, panel->mincols * ui.cell_width);
      orientation = allocation->width >= GTK_EDITOR_MIN_COLUMNS * ui.cell_width
                                         + console_width + ui.cell_width
                                    ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
      if (panel && panel->placement == THE_WORKBENCH_PLACE_BELOW) orientation = GTK_ORIENTATION_VERTICAL;
   }
   (void)data;
   /* Reset the divider only when changing layouts. Resizing within a layout
    * retains the user's divider adjustment and GtkPaned's resize behavior. */
   if (!ui.split_positioned
   || gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) != orientation)
   {
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget), orientation);
      /* GtkPaned must finish allocating the new axis before positioning its
       * divider; otherwise its old-axis resize adjustment moves it again. */
      if (!ui.split_layout_idle)
         ui.split_layout_idle = g_idle_add(position_split, NULL);
      ui.split_positioned = 1;
   }
}

static gboolean clicked(GtkWidget *widget, GdkEventButton *button, gpointer data)
{
   int screen_no, role;
   int row = (int)button->y / ui.cell_height;
   int col = (int)button->x / ui.cell_width;
   (void)data;
   if (button->button != 1 || the_panel_active || in_macro)
      return TRUE;
   if (!ui.drag_motion) ui.dragging = 0;
   for (screen_no = 0; screen_no < display_screens; screen_no++)
   {
      for (role = WINDOW_FILEAREA; role <= WINDOW_COMMAND; role++)
      {
         TheDriverWindow *win = driver_screen_role_window(screen_no, role);
         TheDriverWindowOrigin origin = operations.window_origin(win);
         TheDriverWindowSize size = operations.window_size(win);
         TheInputEvent event;
         int cell = col - origin.col;
         int local_row = row - origin.row;
         TheInputLogicalTargetKind kind;
         LINETYPE target_line = -1;
         int selectable = 0;
         if (!origin.valid || !size.valid || local_row < 0 || local_row >= size.rows
         || cell < 0 || cell >= size.cols)
            continue;
         kind = role == WINDOW_FILEAREA ? THE_INPUT_TARGET_FILEAREA
              : role == WINDOW_PREFIX ? THE_INPUT_TARGET_PREFIX
              : THE_INPUT_TARGET_COMMAND;
         if (role != WINDOW_COMMAND && screen[screen_no].sl)
         {
            SHOW_LINE *line = &screen[screen_no].sl[local_row];
            int enterable = role == WINDOW_FILEAREA ? line->main_enterable
                                                     : line->prefix_enterable;
            /* Mirror the core's cursorable-row contract. Display-only rows
             * must not pull keyboard focus away from Program input or other
             * controls when the core would reject their cursor target. */
            if (!enterable && line->line_type != LINE_TOF && line->line_type != LINE_EOF)
               return TRUE;
            if (role == WINDOW_FILEAREA)
            {
               target_line = line->line_number;
               selectable = line->line_type == LINE_LINE;
               const CHARTYPE *text = line->contents ? line->contents : rec;
               size_t length = line->contents ? line->length : rec_len;
               cell = driver_layout_logical_col_from_display(text, length,
                  (int)screen[screen_no].screen_view->verify_col - 1, cell,
                  TEXT_SNAP_BACKWARD);
            }
         }
         if (ui.drag_motion && !selectable) return TRUE;
         if (selectable)
         {
            if (!ui.drag_motion)
            {
               ui.drag_started = (button->state & GDK_SHIFT_MASK) != 0;
               queue_command(ui.drag_started ? "mark cua" : "reset block");
            }
            else if (!ui.drag_started) { queue_command("mark cua"); ui.drag_started = 1; }
         }
         if (the_input_event_from_logical_target(kind, target_line, local_row,
                                                    cell, screen_no, 0, &event)
         && queue_event(event))
         {
            if (selectable)
            {
               if (ui.drag_started) queue_command("mark cua");
               ui.dragging = 1; ui.drag_row = row; ui.drag_col = col;
            }
            gtk_widget_grab_focus(widget);
         }
         return TRUE;
      }
   }
   return TRUE;
}

static gboolean pointer_moved(GtkWidget *widget, GdkEventMotion *motion, gpointer data)
{
   GdkEventButton hit = {0};
   (void)data;
   if (!ui.dragging || !(motion->state & GDK_BUTTON1_MASK)) return FALSE;
   if ((int)motion->y / ui.cell_height == ui.drag_row && (int)motion->x / ui.cell_width == ui.drag_col) return TRUE;
   hit.button = 1; hit.x = motion->x; hit.y = motion->y; hit.state = motion->state;
   ui.drag_motion = 1; clicked(widget, &hit, NULL); ui.drag_motion = 0;
   return TRUE;
}
static gboolean pointer_released(GtkWidget *widget, GdkEventButton *button, gpointer data)
{ (void)widget; (void)data; if (button->button == 1) ui.dragging = 0; return FALSE; }

static gboolean scrolled(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
   (void)widget; (void)data;
   if (event->direction == GDK_SCROLL_UP || event->delta_y < 0)
      queue_command("backward 3");
   else if (event->direction == GDK_SCROLL_DOWN || event->delta_y > 0)
      queue_command("forward 3");
   return TRUE;
}

static unsigned logical_rgb(int logical, int foreground)
{
   static const unsigned palette[] = {
      0x1e1e1e, 0x569cd6, 0x6a9955, 0x4ec9b0,
      0xf48771, 0xc586c0, 0xdcdcaa, 0xd4d4d4
   };
   unsigned rgb = foreground ? 0xd4d4d4 : 0x1e1e1e;
   if (logical >= 0 && logical < 8)
      rgb = palette[logical];
   else if (the_render_color_is_rgb(logical))
      rgb = the_render_color_rgb24(logical);
   return rgb;
}

static void colour(cairo_t *cr, int logical, int foreground)
{
   unsigned rgb = logical_rgb(logical, foreground);
   cairo_set_source_rgb(cr, ((rgb >> 16) & 255) / 255.0,
                        ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0);
}

static void console_colours(TheDriverAttr attr, GdkRGBA *foreground, GdkRGBA *background)
{
   unsigned fg = logical_rgb(the_render_attr_fg(attr), 1);
   unsigned bg = logical_rgb(the_render_attr_bg(attr), 0);
   TheRenderStyle style = the_render_attr_style(attr);
   if (style & THE_STYLE_REVERSE)
   {
      unsigned swap = fg; fg = bg; bg = swap;
   }
   *foreground = (GdkRGBA){((fg >> 16) & 255) / 255.0,
                           ((fg >> 8) & 255) / 255.0, (fg & 255) / 255.0, 1};
   *background = (GdkRGBA){((bg >> 16) & 255) / 255.0,
                           ((bg >> 8) & 255) / 255.0, (bg & 255) / 255.0, 1};
   if (style & THE_STYLE_DIM)
   {
      foreground->red = foreground->red * .65 + background->red * .35;
      foreground->green = foreground->green * .65 + background->green * .35;
      foreground->blue = foreground->blue * .65 + background->blue * .35;
   }
}

static void console_css_rule(GString *css, const char *selector, TheDriverAttr attr,
                               int border)
{
   GdkRGBA fg, bg;
   char *foreground, *background;
   TheRenderStyle style = the_render_attr_style(attr);
   console_colours(attr, &fg, &bg);
   foreground = gdk_rgba_to_string(&fg);
   background = gdk_rgba_to_string(&bg);
   g_string_append_printf(css,
      "%s { color: %s; background-color: %s; background-image: none; transition: none;"
      "font-weight: %s; font-style: %s; text-decoration-line: %s%s%s;",
      selector, foreground, background, style & THE_STYLE_BOLD ? "bold" : "normal",
      style & THE_STYLE_ITALIC ? "italic" : "normal",
      style & THE_STYLE_UNDERLINE ? "underline " : "",
      style & THE_STYLE_STRIKEOUT ? "line-through " : "",
      style & (THE_STYLE_UNDERLINE | THE_STYLE_STRIKEOUT) ? "" : "none");
   if (border)
      g_string_append_printf(css, "border-color: %s;", foreground);
   g_string_append(css, "}\n");
   g_free(foreground);
   g_free(background);
}

static void sync_console_palette(void)
{
   TheDriverAttr colours[GTK_CONSOLE_COLOURS];
   GtkTextTag *error_tag;
   GdkRGBA fg, bg;
   TheRenderStyle style;
   GString *css;
   GError *error = NULL;
   int i;
   if (!ui.console_css || !CURRENT_VIEW || !CURRENT_FILE)
      return;
   for (i = 0; i < GTK_CONSOLE_COLOURS; i++)
      colours[i] = set_colour(CURRENT_FILE->attr + ATTR_CONSOLEBODY + i);
   if (ui.console_colours_valid
   && !memcmp(colours, ui.console_colours, sizeof(colours)))
      return;
   css = g_string_new(NULL);
#define CONSOLE_RULE(selector, role, border) \
   console_css_rule(css, selector, colours[(role) - ATTR_CONSOLEBODY], border)
   CONSOLE_RULE("#the-program-console, "
                "#the-console-output, #the-console-output text, "
                "#the-build-result, #the-build-output, #the-build-output text, "
                "#the-workbench-pane, #the-workbench-text, #the-workbench-text text",
                ATTR_CONSOLEBODY, 0);
   CONSOLE_RULE("#the-program-console .the-console-header, "
                "#the-program-console .the-console-header label, "
                "#the-build-result .the-console-header, #the-build-result .the-console-header label", ATTR_CONSOLEHEADER, 0);
   CONSOLE_RULE("#the-console-input-row, #the-console-input-row label, #the-console-input",
                ATTR_CONSOLEINPUT, 1);
   CONSOLE_RULE("#the-program-console .the-console-control, "
                "#the-program-console .the-console-control label, "
                "#the-build-result .the-console-control, #the-build-result .the-console-control label, "
                "#the-program-run, #the-program-run label, #the-file-build, #the-file-build label, "
                "#the-file-apply, #the-file-apply label", ATTR_CONSOLECONTROL, 1);
   CONSOLE_RULE("#the-console-output text selection, #the-build-output text selection, #the-console-input selection, #the-workbench-text text selection, "
                "#the-program-console .the-console-control:hover, "
                "#the-program-console .the-console-control:hover label, "
                "#the-program-console .the-console-control:focus, "
                "#the-program-console .the-console-control:focus label, "
                "#the-program-console .the-console-control:active, "
                "#the-program-console .the-console-control:active label, "
                "#the-build-result .the-console-control:hover, #the-build-result .the-console-control:hover label, "
                "#the-build-result .the-console-control:focus, #the-build-result .the-console-control:focus label, "
                "#the-build-result .the-console-control:active, #the-build-result .the-console-control:active label, "
                "#the-program-run:hover, #the-program-run:hover label, "
                "#the-program-run:focus, #the-program-run:focus label, "
                "#the-program-run:active, #the-program-run:active label, "
                "#the-file-build:hover, #the-file-build:hover label, "
                "#the-file-build:focus, #the-file-build:focus label, "
                "#the-file-build:active, #the-file-build:active label, "
                "#the-file-apply:hover, #the-file-apply:hover label, "
                "#the-file-apply:focus, #the-file-apply:focus label, "
                "#the-file-apply:active, #the-file-apply:active label", ATTR_CONSOLESELECTED, 1);
   CONSOLE_RULE("#the-program-console #the-console-status, #the-build-status, #the-build-profile, #the-workbench-status", ATTR_CONSOLESTATUS, 0);
   CONSOLE_RULE("#the-editor-console-split > separator", ATTR_CONSOLEDIVIDER, 1);
   g_string_append(css,
      "#the-editor-console-split > separator { border-style: solid; border-width: 1px; }\n"
      "#the-file-build:disabled, #the-program-run:disabled, #the-file-apply:disabled { opacity: 0.45; }\n");
#undef CONSOLE_RULE
   if (!gtk_css_provider_load_from_data(ui.console_css, css->str, css->len, &error))
   {
      g_warning("GTK console palette: %s", error->message);
      g_clear_error(&error);
      g_string_free(css, TRUE);
      return;
   }
   g_string_free(css, TRUE);
   style = the_render_attr_style(colours[ATTR_CONSOLEERROR - ATTR_CONSOLEBODY]);
   console_colours(colours[ATTR_CONSOLEERROR - ATTR_CONSOLEBODY], &fg, &bg);
   for (i = 0; i < 2; i++)
   {
      error_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(
         gtk_text_view_get_buffer(GTK_TEXT_VIEW(i ? ui.build_output : ui.output))), "stderr");
      g_object_set(error_tag, "foreground-rgba", &fg, "background-rgba", &bg,
         "weight", style & THE_STYLE_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
         "style", style & THE_STYLE_ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
         "underline", style & THE_STYLE_UNDERLINE ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
         "strikethrough", (style & THE_STYLE_STRIKEOUT) != 0, NULL);
   }
   memcpy(ui.console_colours, colours, sizeof(colours));
   ui.console_colours_valid = 1;
}

static void draw_surface(cairo_t *cr, PangoLayout *layout, GtkSurface *surface)
{
   TheDriverWindowOrigin origin = operations.window_origin(surface->window);
   TheDriverWindowSize size = operations.window_size(surface->window);
   int row, col;
   if (!surface->visible || !origin.valid || !size.valid)
      return;
   cairo_save(cr);
   cairo_rectangle(cr, origin.col * ui.cell_width, origin.row * ui.cell_height,
                       size.cols * ui.cell_width, size.rows * ui.cell_height);
   cairo_clip(cr);
   /* Backgrounds are separate from glyphs: continuation cells of a wide
    * grapheme must not paint over its glyph after Pango has drawn it. */
   for (row = 0; row < size.rows; row++)
      for (col = 0; col < size.cols; col++)
      {
         TheRenderCell cell;
         TheRenderStyle style;
         int fg, bg;
         headless_driver_render_cell_at(surface->window, row, col, &cell);
         style = the_render_attr_style(cell.attr);
         fg = the_render_attr_fg(cell.attr); bg = the_render_attr_bg(cell.attr);
         colour(cr, style & THE_STYLE_REVERSE ? fg : bg,
                     (style & THE_STYLE_REVERSE) != 0);
         cairo_rectangle(cr, (origin.col + col) * ui.cell_width,
            (origin.row + row) * ui.cell_height, ui.cell_width, ui.cell_height);
         cairo_fill(cr);
      }
   for (row = 0; row < size.rows; row++)
      for (col = 0; col < size.cols; col++)
      {
         TheRenderCell cell;
         TheRenderStyle style;
         char text[THE_RENDER_MAX_UTF8_BYTES + 1];
         size_t length = 0;
         int x = (origin.col + col) * ui.cell_width;
         int y = (origin.row + row) * ui.cell_height;
         int width, height;
         if (!headless_driver_render_cell_at(surface->window, row, col, &cell))
            continue;
         style = the_render_attr_style(cell.attr);
         if (cell.flags & THE_RENDER_CLUSTER_HAS_UTF8)
         {
            length = MIN(cell.utf8_length, sizeof(text) - 1);
            memcpy(text, cell.utf8, length);
         }
         else if (cell.flags & THE_RENDER_CLUSTER_HAS_FALLBACK)
         {
            length = MIN(cell.fallback_length, sizeof(text) - 1);
            memcpy(text, cell.fallback_utf8, length);
         }
         else if (cell.codepoint_count && g_unichar_validate(cell.codepoints[0]))
            length = g_unichar_to_utf8(cell.codepoints[0], text);
         text[length] = 0;
         if (!length || !g_utf8_validate(text, length, NULL))
            continue;
         colour(cr, style & THE_STYLE_REVERSE ? the_render_attr_bg(cell.attr)
                                               : the_render_attr_fg(cell.attr), 1);
         pango_font_description_set_weight(ui.font,
            style & THE_STYLE_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
         pango_font_description_set_style(ui.font,
            style & THE_STYLE_ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
         pango_layout_set_font_description(layout, ui.font);
         pango_layout_set_text(layout, text, length);
         pango_layout_get_pixel_size(layout, &width, &height);
         cairo_save(cr);
         cairo_rectangle(cr, x, y, MAX(1, cell.advance_width) * ui.cell_width,
                                                    ui.cell_height);
         cairo_clip(cr);
         cairo_move_to(cr, x, y + MAX(0, (ui.cell_height - height) / 2));
         pango_cairo_show_layout(cr, layout);
         if (style & THE_STYLE_UNDERLINE)
         {
            cairo_rectangle(cr, x, y + ui.cell_height - 2,
               MAX(1, cell.advance_width) * ui.cell_width, 1);
            cairo_fill(cr);
         }
         cairo_restore(cr);
      }
   cairo_restore(cr);
}

static CursorShape cursor_shape(void)
{
   return INSERTMODEx ? cursorstyle_insert_shape : cursorstyle_over_shape;
}

static CursorBlink cursor_blink(void)
{
   return INSERTMODEx ? cursorstyle_insert_blink : cursorstyle_over_blink;
}

static gboolean draw_editor(GtkWidget *widget, cairo_t *cr, gpointer data)
{
   GList *item;
   PangoLayout *layout = gtk_widget_create_pango_layout(widget, "");
   TheDriverWindow *win = ui.input_window ? ui.input_window : driver_current_window();
   TheDriverWindowCursor cursor = operations.capture_window_cursor(win);
   TheDriverWindowOrigin origin = operations.window_origin(win);
   (void)data;
   colour(cr, THE_COLOR_UNSPECIFIED, 0);
   cairo_paint(cr);
   for (item = ui.surfaces; item; item = item->next)
      draw_surface(cr, layout, item->data);
   if (ui.cursor_visible && gtk_widget_has_focus(widget) && cursor.valid && origin.valid
   && (cursor_blink() != CURSOR_BLINK || ui.blink_visible))
   {
      int x = (origin.col + cursor.col) * ui.cell_width;
      int y = (origin.row + cursor.row) * ui.cell_height;
      GdkRectangle area = {x, y, ui.cell_width, ui.cell_height};
      gtk_im_context_set_cursor_location(ui.im, &area);
      cairo_set_source_rgba(cr, 0.95, 0.96, 1.0, 0.85);
      switch (cursor_shape())
      {
         case CURSOR_IBEAM: cairo_rectangle(cr, x, y, 2, ui.cell_height); break;
         case CURSOR_UNDERLINE:
            cairo_rectangle(cr, x, y + ui.cell_height - 2, ui.cell_width, 2); break;
         default:
            cairo_set_source_rgba(cr, 0.95, 0.96, 1.0, 0.35);
            cairo_rectangle(cr, x, y, ui.cell_width, ui.cell_height); break;
      }
      cairo_fill(cr);
   }
   pango_font_description_set_weight(ui.font, PANGO_WEIGHT_NORMAL);
   pango_font_description_set_style(ui.font, PANGO_STYLE_NORMAL);
   g_object_unref(layout);
   return FALSE;
}

static int reader_selected_page(const GtkConsoleReader *reader)
{
   GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary),
      gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary)));
   return reader == &ui.build_reader ? page == ui.setup && ui.setup_tabs
      && gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.setup_tabs)) == 2 : page == reader->page;
}

static int reader_at_tail(const GtkConsoleReader *reader)
{
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(reader->scroller));
   return gtk_adjustment_get_value(adjustment) >= gtk_adjustment_get_upper(adjustment)
      - gtk_adjustment_get_page_size(adjustment) - 2;
}

static void reader_unread(GtkConsoleReader *reader)
{
   char label[64];
   const char *title = reader == &ui.program_reader ? "Program" : "Build result";
   if (reader->unread) g_snprintf(label, sizeof(label), "Latest (%u new lines)", reader->unread);
   else g_strlcpy(label, "Latest", sizeof(label));
   gtk_button_set_label(GTK_BUTTON(reader->latest), label);
   if (reader->unread) g_snprintf(label, sizeof(label), "%s (%u unread)", title, reader->unread);
   else g_strlcpy(label, title, sizeof(label));
   gtk_label_set_text(GTK_LABEL(reader->tab), label);
}

static void cancel_reader_scroll(GtkConsoleReader *reader)
{
   if (reader->scroll_clock && reader->scroll_handler)
      g_signal_handler_disconnect(reader->scroll_clock, reader->scroll_handler);
   reader->scroll_handler = 0; reader->scroll_frames = 0;
   g_clear_object(&reader->scroll_clock); g_clear_object(&reader->scroll_adjustment);
}

static void reader_scrolled(GtkAdjustment *adjustment, gpointer data)
{
   GtkConsoleReader *reader = data;
   (void)adjustment;
   if (!reader->syncing && !reader->scroll_handler && reader_selected_page(reader) && reader_at_tail(reader)
   && !gtk_text_buffer_get_has_selection(gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader->view))))
   { reader->unread = 0; reader_unread(reader); }
}

static void reader_scroll_after_paint(GdkFrameClock *clock, gpointer data)
{
   GtkConsoleReader *reader = data;
   (void)clock;
   reader->syncing++;
   gtk_adjustment_set_value(reader->scroll_adjustment, reader->scroll_value);
   /* TextView can validate its prefix anchor after this first paint and
    * adjust the viewport again. Restore once more on the following frame,
    * after that native layout has settled, then release the restoration. */
   if (--reader->scroll_frames <= 0) cancel_reader_scroll(reader);
   else gtk_widget_queue_draw(reader->view);
   reader->syncing--;
}

static gboolean reader_manual_input(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   (void)widget; (void)event;
   /* A user gesture always takes precedence over a pending background
    * output restore, including scrollbar drags and native reader keys. */
   cancel_reader_scroll(data);
   return FALSE;
}

static void bind_reader_input(GtkConsoleReader *reader)
{
   GtkWidget *scrollbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(reader->scroller));
   g_signal_connect(reader->scroller, "scroll-event", G_CALLBACK(reader_manual_input), reader);
   g_signal_connect(reader->view, "button-press-event", G_CALLBACK(reader_manual_input), reader);
   g_signal_connect(reader->view, "key-press-event", G_CALLBACK(reader_manual_input), reader);
   g_signal_connect(scrollbar, "button-press-event", G_CALLBACK(reader_manual_input), reader);
   g_signal_connect(scrollbar, "key-press-event", G_CALLBACK(reader_manual_input), reader);
}

static void reader_scroll(GtkConsoleReader *reader, double value)
{
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(reader->scroller));
   cancel_reader_scroll(reader); reader->scroll_value = value;
   reader->scroll_clock = gtk_widget_get_frame_clock(reader->view);
   if (reader->scroll_clock)
   {
      g_object_ref(reader->scroll_clock); reader->scroll_adjustment = g_object_ref(adjustment);
      reader->scroll_frames = 2;
      reader->scroll_handler = g_signal_connect(reader->scroll_clock, "after-paint", G_CALLBACK(reader_scroll_after_paint), reader);
      gtk_widget_queue_draw(reader->view);
   }
   else gtk_adjustment_set_value(adjustment, value);
}

static void reader_latest(GtkWidget *widget, gpointer data)
{
   GtkConsoleReader *reader = data;
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader->view));
   GtkTextIter end;
   (void)widget;
   gtk_text_buffer_get_end_iter(buffer, &end);
   gtk_text_buffer_place_cursor(buffer, &end);
   reader->unread = 0; reader_unread(reader);
   reader_scroll(reader, G_MAXDOUBLE);
}

/* The host bounds each output lane. Find the retained suffix in linear time
 * so eviction preserves a surviving selection and its viewport like LOG. */
static size_t reader_overlap(const char *old, size_t old_length, const char *text, size_t length)
{
   size_t *prefix, i, matched = 0;
   if (!old_length || !length) return 0;
   if (length >= old_length && !memcmp(old, text, old_length)) return old_length;
   prefix = g_new(size_t, length);
   prefix[0] = 0;
   for (i = 1; i < length; i++)
   {
      size_t at = prefix[i - 1];
      while (at && text[i] != text[at]) at = prefix[at - 1];
      prefix[i] = at + (text[i] == text[at]);
   }
   for (i = 0; i < old_length; i++)
   {
      while (matched && (matched == length || old[i] != text[matched])) matched = prefix[matched - 1];
      if (old[i] == text[matched]) matched++;
   }
   g_free(prefix);
   /* TextView offsets are characters; do not retain a partial UTF-8 prefix. */
   while (matched && ((unsigned char)text[matched] & 0xc0) == 0x80) matched--;
   return matched;
}

static void sync_reader(GtkConsoleReader *reader, const TheToolConsole *console)
{
   GtkTextBuffer *buffer;
   GtkTextIter first, end, anchor, insert;
   GtkAdjustment *adjustment;
   GString *text;
   size_t i, overlap, removed, skip, added, maximum, offset = 0;
   uint64_t progress;
   uint64_t streams = UINT64_C(1469598103934665603);
   unsigned lines = 0;
   int same, cleared, selected, follow, anchor_offset, insert_offset, removed_characters = 0;
   double scroll, deleted_y = 0;
   if (reader->version == console->version) return;
   reader->version = console->version;
   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader->view));
   adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(reader->scroller));
   text = g_string_new(NULL);
   for (i = 0; i < console->count; i++)
   {
      char *valid = g_utf8_make_valid(console->chunks[i].text, console->chunks[i].length);
      g_string_append(text, valid); g_free(valid);
      streams = (streams ^ (unsigned)console->chunks[i].fd) * UINT64_C(1099511628211);
      streams = (streams ^ console->chunks[i].length) * UINT64_C(1099511628211);
   }
   same = reader->job == console->id;
   reader->job = console->id;
   cleared = same && (console->history_epoch != reader->history_epoch || console->displayed < reader->displayed);
   progress = same && !cleared ? console->displayed - reader->displayed : console->displayed;
   if (same && !cleared && reader->text && text->len == reader->text->len && !memcmp(text->str, reader->text->str, text->len)
   && streams == reader->streams && console->displayed == reader->displayed && console->bytes == reader->bytes && console->omitted == reader->omitted)
   { g_string_free(text, TRUE); return; }
   selected = gtk_text_buffer_get_has_selection(buffer);
   gtk_text_buffer_get_iter_at_mark(buffer, &anchor, gtk_text_buffer_get_selection_bound(buffer));
   gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
   anchor_offset = gtk_text_iter_get_offset(&anchor); insert_offset = gtk_text_iter_get_offset(&insert);
   scroll = gtk_adjustment_get_value(adjustment);
   follow = !same || cleared || (!selected && (reader->scroll_handler ? reader->scroll_value == G_MAXDOUBLE : reader_at_tail(reader)));
   maximum = same && !cleared && reader->text ? reader->text->len : 0;
   /* Cumulative displayed bytes disambiguate identical retained lines after the
    * bounded host evicts a prefix. Only use byte offsets when the displayed
    * UTF-8 has exactly the retained host length. */
   if (maximum && console->omitted && progress && reader->bytes == maximum && console->bytes == text->len
   && progress > console->bytes - MIN(console->bytes, reader->bytes))
   {
      size_t evicted = progress - (console->bytes - MIN(console->bytes, reader->bytes));
      if (reader->bytes > console->bytes) evicted += reader->bytes - console->bytes;
      maximum -= MIN(maximum, evicted);
   }
   overlap = maximum ? reader_overlap(reader->text->str + reader->text->len - maximum, maximum, text->str, text->len) : 0;
   removed = same && reader->text ? reader->text->len - overlap : 0;
   if (!same || cleared || !text->len) reader->unread = 0;
   added = text->len - overlap;
   for (i = overlap; i < text->len; i++) if (text->str[i] == '\n') lines++;
   if (!added && text->len && (progress || streams != reader->streams || console->omitted != reader->omitted))
   {
      size_t start = progress ? text->len - MIN(progress, text->len) : 0;
      for (i = start; i < text->len; i++) if (text->str[i] == '\n') lines++;
      if (!lines) lines = 1;
   }
   if (added && !lines) lines = 1;
   if (follow && reader_selected_page(reader)) reader->unread = 0;
   else if (lines) reader->unread = lines > G_MAXUINT - reader->unread ? G_MAXUINT : reader->unread + lines;
   cancel_reader_scroll(reader); reader->syncing++;
   if (same && overlap)
   {
      if (removed)
      {
         GdkRectangle location;
         gtk_text_buffer_get_start_iter(buffer, &first);
         removed_characters = (int)g_utf8_strlen(reader->text->str, removed);
         gtk_text_buffer_get_iter_at_offset(buffer, &end, removed_characters);
         gtk_text_view_get_iter_location(GTK_TEXT_VIEW(reader->view), &end, &location); deleted_y = location.y;
         gtk_text_buffer_delete(buffer, &first, &end);
      }
   }
   else
   {
      removed_characters = same && reader->text ? (int)g_utf8_strlen(reader->text->str, -1) : 0;
      gtk_text_buffer_set_text(buffer, "", 0);
   }
   skip = overlap;
   for (i = 0; i < console->count; i++)
   {
      char *valid = g_utf8_make_valid(console->chunks[i].text, console->chunks[i].length);
      size_t length = strlen(valid);
      if (skip < length)
      {
         gtk_text_buffer_get_end_iter(buffer, &end);
         gtk_text_buffer_insert_with_tags_by_name(buffer, &end, valid + skip, length - skip,
            console->chunks[i].fd == 2 ? "stderr" : "stdout", NULL);
         skip = 0;
      }
      else skip -= length;
      g_free(valid);
   }
   gtk_text_buffer_get_bounds(buffer, &first, &end);
   gtk_text_buffer_remove_tag_by_name(buffer, "stdout", &first, &end);
   gtk_text_buffer_remove_tag_by_name(buffer, "stderr", &first, &end);
   for (i = 0; i < console->count; i++)
   {
      char *valid = g_utf8_make_valid(console->chunks[i].text, console->chunks[i].length);
      size_t count = g_utf8_strlen(valid, -1);
      gtk_text_buffer_get_iter_at_offset(buffer, &first, offset);
      gtk_text_buffer_get_iter_at_offset(buffer, &end, offset + count);
      gtk_text_buffer_apply_tag_by_name(buffer, console->chunks[i].fd == 2 ? "stderr" : "stdout", &first, &end);
      offset += count; g_free(valid);
   }
   if (same && !cleared && selected)
   {
      int count = gtk_text_buffer_get_char_count(buffer);
      gtk_text_buffer_get_iter_at_offset(buffer, &anchor, CLAMP(anchor_offset - removed_characters, 0, count));
      gtk_text_buffer_get_iter_at_offset(buffer, &insert, CLAMP(insert_offset - removed_characters, 0, count));
      gtk_text_buffer_select_range(buffer, &insert, &anchor);
   }
   if (reader->text) g_string_free(reader->text, TRUE);
   reader->text = text;
   reader->streams = streams; reader->displayed = console->displayed;
   reader->history_epoch = console->history_epoch;
   reader->bytes = console->bytes; reader->omitted = console->omitted;
   reader_unread(reader);
   reader_scroll(reader, follow ? G_MAXDOUBLE : MAX(0, scroll - deleted_y));
   reader->syncing--;
}

static void show_tool_page(GtkWidget *page)
{
   if (page == ui.build_result) { show_setup(2); return; }
   gtk_widget_show(page); gtk_widget_show(ui.auxiliary);
   /* Explicit task navigation selects its requested page without clearing reader state. */
   ui.syncing_logs++;
   gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), page));
   ui.syncing_logs--;
}

static void hide_tool_page(GtkWidget *widget, gpointer data)
{
   GtkWidget *page = data;
   (void)widget;
   if (page == ui.console) the_tool_program_console_visibility(0, 0);
   else { ui.build_hidden = 1; ui.setup_visible = 0; page = ui.setup; }
   gtk_widget_hide(page);
   focus_editor();
}

static void sync_tool_catalog(void)
{
   GString *signature = g_string_new(NULL);
   size_t i;
   for (i = 0; i < the_tool_count(); i++)
   {
      const TheToolDefinition *tool = the_tool_at(i);
      g_string_append_printf(signature, "%s|%s|%s|%s|%s|%d;", tool->id, tool->label, tool->role, tool->extensions, tool->image, tool->available);
   }
   if (!g_strcmp0(ui.catalog_signature, signature->str)) { g_string_free(signature, TRUE); return; }
   g_free(ui.catalog_signature); ui.catalog_signature = g_string_free(signature, FALSE);
   if (ui.tools_view)
   {
      GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ui.tools_view)));
      const char *selected = gtk_combo_box_get_active_id(GTK_COMBO_BOX(ui.runner));
      char *selected_id = g_strdup(selected);
      gtk_list_store_clear(store); gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(ui.runner));
      for (i = 0; i < the_tool_count(); i++)
      {
         const TheToolDefinition *tool = the_tool_at(i);
         const TheWorkbenchTool *definition = the_workbench_tool_find(tool->id);
         GtkTreeIter row;
         char *details = g_strdup_printf("%s%s%s", definition ? definition->owner : "Profile-defined / unknown", *tool->image ? " · " : "", tool->image);
         gtk_list_store_append(store, &row);
         gtk_list_store_set(store, &row, 0, *tool->extensions ? tool->extensions : "—", 1, tool->label, 2, tool->role, 3, tool->available ? "Available" : "Unavailable", 4, details, -1);
         g_free(details);
         if (!strcmp(tool->role, "runner") && tool->available) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui.runner), tool->id, tool->label);
      }
      if (!selected_id || !gtk_combo_box_set_active_id(GTK_COMBO_BOX(ui.runner), selected_id)) gtk_combo_box_set_active(GTK_COMBO_BOX(ui.runner), 0);
      g_free(selected_id);
   }
}

static void sync_file_tools(void)
{
   GtkWidget *buttons[] = {ui.build, ui.run, ui.apply};
   TheToolsFileSnapshot *snapshot = &ui.file_snapshot;
   char summary[1200], *name, *active_name;
   const char *prerequisite = NULL;
   size_t i;
   int relevant = 0;
   if (!the_tools_file_snapshot(snapshot)) return;
   for (i = 0; i < THE_TOOLS_FILE_ACTION_COUNT; i++)
   {
      const TheToolsFileActionSnapshot *action = snapshot->actions + i;
      int visible = *action->tool != 0;
      gtk_button_set_label(GTK_BUTTON(buttons[i]), action->label);
      gtk_widget_set_visible(buttons[i], visible);
      relevant += visible;
      gtk_widget_set_sensitive(buttons[i], action->enabled);
      gtk_widget_set_tooltip_text(buttons[i], action->enabled
         ? i == 1 ? "Run the current unsaved file snapshot. Output and stdin are in Program console."
           : i == 2 ? "Apply the current built profile and retain its startup selection."
           : "Build the current unsaved file snapshot. Details are in Tools & Profiles / Build results."
         : action->reason);
      if (visible && !action->enabled && !prerequisite) prerequisite = action->reason;
   }
   gtk_widget_set_visible(ui.task_row, relevant != 0);
   gtk_label_set_text(GTK_LABEL(ui.task_state), prerequisite ? prerequisite : snapshot->role);
   name = g_path_get_basename(snapshot->path);
   g_snprintf(summary, sizeof(summary), "%s%s", name, the_editor_buffer_dirty(CURRENT_FILE) ? " · Modified" : " · Saved");
   gtk_label_set_text(GTK_LABEL(ui.file_title), summary); gtk_widget_set_tooltip_text(ui.file_title, snapshot->path); g_free(name);
   gtk_widget_set_sensitive(ui.apply_image, snapshot->profile.apply_image.enabled);
   gtk_widget_set_tooltip_text(ui.apply_image, snapshot->profile.apply_image.enabled
      ? "Choose a .profile.rxbin file to apply and retain an immutable startup copy."
      : snapshot->profile.apply_image.reason);
   if (*snapshot->profile.active.source_path || *snapshot->profile.active.image)
   {
      active_name = g_path_get_basename(*snapshot->profile.active.source_path
         ? snapshot->profile.active.source_path : snapshot->profile.active.image);
      g_snprintf(summary, sizeof(summary), "Active profile: %s\nSelection kind: %s\n%s\n%s",
         active_name, *snapshot->profile.active.kind ? snapshot->profile.active.kind : "Unknown",
         snapshot->profile.active.startup_saved ? "Startup selection saved for future sessions." : "Startup selection is not confirmed saved.",
         !*snapshot->profile.active.source_path ? "No editable source is attached to this compiled profile." : "Edit defaults, then Build and Apply in the editor.");
      g_free(active_name);
   }
   else g_snprintf(summary, sizeof(summary), "No user profile applied.\nEdit profile defaults to create or open your user profile source, then Build and Apply in the editor.");
   gtk_label_set_text(GTK_LABEL(ui.profile_summary), summary);
   gtk_widget_set_tooltip_text(ui.edit_profile, !*snapshot->profile.active.source_path && *snapshot->profile.active.image
      ? "Open or create a user profile source; the imported binary has no source to edit." : "Open your editable profile source for future defaults.");
   {
      char *details = g_strdup_printf("Editable profile source: %s\nApplied source: %s\nApplied compiled profile: %s\nLegacy registry entries: %lu (provenance unknown/preserved)", snapshot->profile.source_path,
         *snapshot->profile.active.source_path ? snapshot->profile.active.source_path : "None",
         *snapshot->profile.active.image ? snapshot->profile.active.image : "None", (unsigned long)snapshot->profile.legacy_entries);
      gtk_widget_set_tooltip_text(ui.profile_summary, details); g_free(details);
   }
   if (*snapshot->profile.build.image)
   {
      const char *role = !strcmp(snapshot->profile.build.role, "profile-source") ? "Profile"
         : !strcmp(snapshot->profile.build.role, "macro-source") ? "Macro"
         : !strcmp(snapshot->profile.build.role, "crexx-source") ? "Program" : snapshot->profile.build.role;
      char *details;
      g_snprintf(summary, sizeof(summary), "Latest build · %s: %s%s%.180s", role, snapshot->profile.build.state,
         *snapshot->profile.build.name ? " · " : "", snapshot->profile.build.name);
      details = g_strdup_printf("Source: %s\nCompiled artifact: %s", snapshot->profile.build.source_path, snapshot->profile.build.image);
      gtk_widget_set_tooltip_text(ui.build_profile, details); g_free(details);
   }
   else { g_strlcpy(summary, "Latest build: None", sizeof(summary)); gtk_widget_set_tooltip_text(ui.build_profile, NULL); }
   gtk_label_set_text(GTK_LABEL(ui.build_profile), summary);
   if (snapshot->result.serial != ui.result_serial)
   {
      ui.result_serial = snapshot->result.serial;
      if (snapshot->result.serial) gtk_label_set_text(GTK_LABEL(ui.build_state), snapshot->result.message);
   }
   sync_tool_catalog();
}

static void sync_console(void)
{
   const TheToolConsole *program = the_tool_program_console();
   const TheToolConsole *build = the_tool_build_console();
   unsigned result_serial = ui.result_serial;
   if (!ui.console) return;
   sync_console_palette();
   sync_file_tools();
   gtk_widget_set_visible(ui.console, program->visible);
   gtk_label_set_text(GTK_LABEL(ui.state), program->state[0] ? program->state :
      the_tools_backend_available() ? "Ready" : "Native RXBIN tool bundle is not configured");
   gtk_widget_set_sensitive(ui.stop, program->running);
   gtk_widget_set_sensitive(ui.eof, program->running && !program->eof);
   gtk_widget_set_sensitive(ui.input, program->running && !program->eof);
   gtk_widget_set_sensitive(ui.send, program->running && !program->eof);
   gtk_widget_set_sensitive(ui.build_stop, build->running);
   if (build->running || (build->id && build->version != ui.build_reader.version && result_serial == ui.result_serial))
      gtk_label_set_text(GTK_LABEL(ui.build_state), build->state);
   if (build->id != ui.build_reader.job) ui.build_hidden = 0;
   gtk_widget_set_visible(ui.setup, ui.setup_visible || (!ui.build_hidden && (build->id || ui.result_serial)));
   sync_reader(&ui.program_reader, program);
   sync_reader(&ui.build_reader, build);
}

static GtkLogPane *find_log(const char *id, unsigned long generation)
{
   guint i;
   if (!ui.logs) return NULL;
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      if (!strcmp(pane->id, id) && pane->generation == generation) return pane;
   }
   return NULL;
}

static GtkLogPane *log_for_widget(GtkWidget *widget)
{
   guint i;
   if (!widget || !ui.logs) return NULL;
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      if (widget == pane->page || gtk_widget_is_ancestor(widget, pane->page)) return pane;
   }
   return NULL;
}

static int queue_panel(GtkLogPane *pane, TheWorkbenchPanelOperation operation)
{
   TheInputEvent event;
   if (the_panel_active || in_macro || !pane) return 0;
   if (operation == THE_WORKBENCH_PANEL_FOCUS && !ui.focused_log[0] && !ui.log_origin)
      remember_log_origin(gtk_window_get_focus(GTK_WINDOW(ui.window)));
   if (operation == THE_WORKBENCH_PANEL_RETURN && !ui.focused_log[0])
   {
      GtkWidget *target = ui.last_nonlog_focus;
      gtk_widget_grab_focus(target && gtk_widget_is_drawable(target) && gtk_widget_is_sensitive(target) ? target : ui.editor);
      return 1;
   }
   return the_input_event_from_workbench_panel(pane->id, pane->generation, operation, &event) && queue_event(event);
}

static void remember_log_origin(GtkWidget *focus)
{
   if (!focus || focus == ui.auxiliary || log_for_widget(focus))
      focus = ui.last_nonlog_focus ? ui.last_nonlog_focus : ui.editor;
   g_clear_object(&ui.log_origin);
   ui.log_origin = g_object_ref(focus ? focus : ui.editor);
   ui.log_origin_program = the_tool_program_console()->focused && focus &&
      (focus == ui.console || gtk_widget_is_ancestor(focus, ui.console));
   ui.log_entry_selected = 0;
   if (ui.log_origin == ui.input)
   {
      ui.log_entry_position = gtk_editable_get_position(GTK_EDITABLE(ui.input));
      ui.log_entry_selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input),
         &ui.log_selection_start, &ui.log_selection_end);
   }
}

static void restore_log_origin(void)
{
   GtkWidget *focus = ui.log_origin;
   if (focus && (focus == ui.console || gtk_widget_is_ancestor(focus, ui.console)))
      gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), 0);
   else if (focus && (focus == ui.build_result || gtk_widget_is_ancestor(focus, ui.build_result)))
      show_setup(2);
   if (!focus || !gtk_widget_is_drawable(focus) || !gtk_widget_is_sensitive(focus)) focus = ui.editor;
   if (ui.log_origin_program && the_tool_program_console()->visible)
      the_tool_program_console_visibility(1, 1);
   gtk_widget_grab_focus(focus);
   if (focus == ui.input)
   {
      if (ui.log_entry_selected)
         gtk_editable_select_region(GTK_EDITABLE(ui.input),
            ui.log_entry_position == ui.log_selection_start ? ui.log_selection_end : ui.log_selection_start,
            ui.log_entry_position);
      else gtk_editable_set_position(GTK_EDITABLE(ui.input), ui.log_entry_position);
   }
   g_clear_object(&ui.log_origin);
}

static void select_program_from_log(GtkLogPane *pane)
{
   remember_log_origin(gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output);
   ui.log_origin_program = the_tool_program_console()->visible;
   queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
}

static void cycle_log(GtkLogPane *pane, int reverse)
{
   int page = gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), pane->page);
   int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ui.auxiliary)), tries;
   for (tries = 0; tries < n; tries++)
   {
      GtkWidget *next;
      GtkLogPane *log;
      page = (page + n + (reverse ? -1 : 1)) % n;
      next = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), page);
      if (!gtk_widget_get_visible(next)) continue;
      if (next == ui.console) { select_program_from_log(pane); return; }
      if (next == ui.setup)
      { remember_log_origin(ui.build_output); ui.log_origin_program = 0; queue_panel(pane, THE_WORKBENCH_PANEL_RETURN); return; }
      log = log_for_widget(next);
      if (log) { queue_panel(log, THE_WORKBENCH_PANEL_FOCUS); return; }
   }
}

static void auxiliary_switched(GtkNotebook *notebook, GtkWidget *page, guint number, gpointer data)
{
   GtkLogPane *pane;
   (void)notebook; (void)number; (void)data;
   if (ui.syncing_logs || the_panel_active || in_macro) return;
   if (page == ui.console || (page == ui.setup && ui.setup_tabs && gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.setup_tabs)) == 2))
   {
      GtkConsoleReader *reader = page == ui.console ? &ui.program_reader : &ui.build_reader;
      if (reader->scroller && reader->view && reader_at_tail(reader) && !gtk_text_buffer_get_has_selection(gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader->view))))
      { reader->unread = 0; reader_unread(reader); }
   }
   pane = log_for_widget(page);
   if (pane) queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS);
   else if (page == ui.console && ui.focused_log[0])
      select_program_from_log(find_log(ui.focused_log, ui.focused_log_generation));
   else if (page == ui.setup && ui.focused_log[0])
   {
      GtkLogPane *from = find_log(ui.focused_log, ui.focused_log_generation);
      remember_log_origin(ui.build_output); ui.log_origin_program = 0;
      queue_panel(from, THE_WORKBENCH_PANEL_RETURN);
   }
}

static void remember_nonlog_focus(GtkWindow *window, GtkWidget *focus, gpointer data)
{
   GtkLogPane *pane = log_for_widget(focus);
   (void)window; (void)data;
   /* A deliberate focus target must be visible in its notebook. Background
    * output never reaches this route and must not select another page. */
   if (ui.console && focus && (focus == ui.console || gtk_widget_is_ancestor(focus, ui.console))
   && gtk_widget_get_visible(ui.console) && gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary)) != 0)
      gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), 0);
   /* set-focus also runs when an inactive toplevel cannot deliver native
    * focus-in-event. Remember the actual field before returning to Program. */
   if (pane && pane->filefields && !ui.syncing_logs) pane->filefields_focus = focus;
   if (!focus || focus == ui.auxiliary || pane) return;
   g_clear_object(&ui.last_nonlog_focus); ui.last_nonlog_focus = g_object_ref(focus);
}

static gboolean log_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
   (void)widget;
   if (event->button == 1) queue_panel(data, THE_WORKBENCH_PANEL_FOCUS);
   return FALSE;
}

static void log_hide_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_panel(data, THE_WORKBENCH_PANEL_HIDE); }
static void log_return_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_panel(data, THE_WORKBENCH_PANEL_RETURN); }

static GtkFormSession *form_session(const char *id)
{
   size_t i, available = THE_WORKBENCH_FORM_MAX;
   const TheWorkbenchFormState *state = the_workbench_form_state(id);
   if (!state) return NULL;
   for (i = 0; i < ui.form_count; i++)
   {
      if (!strcmp(ui.forms[i].id, id)) return ui.forms + i;
      if (ui.forms + i != ui.form_pending && !the_workbench_form_state(ui.forms[i].id)) available = i;
   }
   if (available < THE_WORKBENCH_FORM_MAX) { i = available; memset(ui.forms + i, 0, sizeof(ui.forms[i])); }
   else { if (ui.form_count == THE_WORKBENCH_FORM_MAX) return NULL; i = ui.form_count++; }
   strcpy(ui.forms[i].id, id); ui.forms[i].generation = state->generation;
   ui.forms[i].revision = state->draft_revision;
   return ui.forms + i;
}

static void discard_form_inputs(const char *form)
{
   GList *item = ui.inputs.head;
   while (item)
   {
      GList *next = item->next;
      GtkInput *input = item->data;
      if (input->form && !strcmp(input->form->form, form))
      { g_queue_delete_link(&ui.inputs, item); g_free(input->form); g_free(input); }
      item = next;
   }
}

static void form_receipt(void)
{
   GtkFormSession *session = ui.form_pending;
   const TheWorkbenchFormReceipt *receipt = the_workbench_form_receipt();
   if (!session || !receipt || receipt->request_id != session->pending || receipt->serial == session->receipt) return;
   session->receipt = receipt->serial; session->pending = 0; ui.form_pending = NULL;
   if (!receipt->rc) { session->revision = receipt->draft_revision; session->error[0] = 0; }
   else
   {
      g_strlcpy(session->error, receipt->code, sizeof(session->error));
      session->blocked = !strcmp(receipt->code, "stale-revision") || !strcmp(receipt->code, "stale-generation");
      discard_form_inputs(session->id);
   }
}

static int form_inputs_pending(const char *id)
{
   GList *item;
   if (ui.form_pending && !strcmp(ui.form_pending->id, id)) return 1;
   for (item = ui.inputs.head; item; item = item->next)
   {
      GtkInput *input = item->data;
      if (input->form && !strcmp(input->form->form, id)) return 1;
   }
   return 0;
}

static void queue_form(GtkLogPane *pane, TheWorkbenchFormOperation operation, const char *field, const char *value)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
   GtkFormSession *session;
   GtkInput *input;
   size_t length = value ? strlen(value) : 0;
   if (pane->form_syncing || in_macro || the_panel_active || !panel || panel->generation != pane->generation) return;
   session = form_session(panel->form);
   if (!session || session->blocked || length > THE_WORKBENCH_FIELD_VALUE_MAX || g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT)
   {
      if (session && length > THE_WORKBENCH_FIELD_VALUE_MAX) strcpy(session->error, "limit-exceeded");
      gtk_widget_error_bell(pane->page); sync_form(pane, panel); return;
   }
   input = g_new0(GtkInput, 1); input->form = g_new0(TheWorkbenchFormRequest, 1);
   input->form->operation = operation; input->form->form_generation = session->generation;
   input->form->panel_generation = pane->generation;
   strcpy(input->form->panel, pane->id); strcpy(input->form->form, panel->form);
   if (field) strcpy(input->form->field, field);
   if (length) memcpy(input->form->value, value, length);
   input->form->length = length;
   g_queue_push_tail(&ui.inputs, input);
}

static void form_changed(GtkEditable *entry, gpointer data)
{
   GtkFormField *field = data;
   queue_form(field->pane, THE_WORKBENCH_FORM_SET, field->id, gtk_entry_get_text(GTK_ENTRY(entry)));
}
static void form_submit_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_form(data, THE_WORKBENCH_FORM_SUBMIT, NULL, NULL); }
static void form_cancel_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_form(data, THE_WORKBENCH_FORM_CANCEL, NULL, NULL); }

static void create_form(GtkLogPane *pane, const TheWorkbenchPanel *panel, GtkWidget *controls)
{
   size_t i;
   pane->form_fields = g_ptr_array_new_with_free_func(g_free);
   pane->form_generation = panel->form_generation;
   pane->form_submit = gtk_button_new_with_label("Submit"); pane->form_cancel = gtk_button_new_with_label("Cancel");
   g_signal_connect(pane->form_submit, "clicked", G_CALLBACK(form_submit_clicked), pane);
   g_signal_connect(pane->form_cancel, "clicked", G_CALLBACK(form_cancel_clicked), pane);
   gtk_box_pack_start(GTK_BOX(controls), pane->form_submit, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(controls), pane->form_cancel, FALSE, FALSE, 0);
   gtk_box_reorder_child(GTK_BOX(controls), pane->form_submit, 1);
   gtk_box_reorder_child(GTK_BOX(controls), pane->form_cancel, 2);
   pane->view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
   for (i = 0; i < the_workbench_field_count(); i++)
   {
      const TheWorkbenchField *definition = the_workbench_field_at(i);
      const TheWorkbenchFieldState *state;
      GtkFormField *field;
      GtkWidget *row, *label;
      char caption[THE_WORKBENCH_LABEL_MAX + 32];
      if (strcmp(definition->form, panel->form)) continue;
      state = the_workbench_field_state(panel->form, definition->id);
      field = g_new0(GtkFormField, 1); field->pane = pane; strcpy(field->id, definition->id);
      field->editable = definition->access == THE_WORKBENCH_FIELD_EDIT;
      row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
      g_snprintf(caption, sizeof(caption), "%s%s", definition->label, field->editable ? "" : " [Read-only]");
      label = gtk_label_new(caption); gtk_label_set_xalign(GTK_LABEL(label), 0); gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
      gtk_widget_set_name(label, "the-workbench-status");
      field->entry = gtk_entry_new(); gtk_entry_set_width_chars(GTK_ENTRY(field->entry), 12);
      gtk_style_context_add_class(gtk_widget_get_style_context(field->entry), "the-fixed-field");
      gtk_entry_set_text(GTK_ENTRY(field->entry), state ? state->value : definition->value);
      gtk_editable_set_editable(GTK_EDITABLE(field->entry), field->editable);
      gtk_widget_set_tooltip_text(field->entry, field->editable ? "Form draft. Tab visits fields and controls; Enter submits; Esc returns without cancelling." : "Read-only field");
      gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(row), field->entry, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(pane->view), row, FALSE, FALSE, 0);
      g_signal_connect(field->entry, "button-press-event", G_CALLBACK(log_button_pressed), pane);
      if (field->editable) g_signal_connect(field->entry, "changed", G_CALLBACK(form_changed), field);
      g_signal_connect(field->entry, "activate", G_CALLBACK(form_submit_clicked), pane);
      g_ptr_array_add(pane->form_fields, field);
   }
}

static void sync_form(GtkLogPane *pane, const TheWorkbenchPanel *panel)
{
   const TheWorkbenchFormState *state = the_workbench_form_state(panel->form);
   GtkFormSession *session = form_session(panel->form);
   guint i;
   char status[192];
   if (!state || !session) { gtk_label_set_text(GTK_LABEL(pane->status), "Form unavailable"); return; }
   if (!session->pending && state->draft_revision != session->revision)
   { session->blocked = 1; strcpy(session->error, "stale-revision; refocus form"); }
   pane->form_syncing++;
   if (!form_inputs_pending(panel->form))
      for (i = 0; i < pane->form_fields->len; i++)
      {
         GtkFormField *field = g_ptr_array_index(pane->form_fields, i);
         const TheWorkbenchFieldState *value = the_workbench_field_state(panel->form, field->id);
         if (value && strcmp(value->value, gtk_entry_get_text(GTK_ENTRY(field->entry))))
         {
            int at = gtk_editable_get_position(GTK_EDITABLE(field->entry));
            gtk_entry_set_text(GTK_ENTRY(field->entry), value->value);
            gtk_editable_set_position(GTK_EDITABLE(field->entry), MIN(at, (int)g_utf8_strlen(value->value, value->length)));
         }
      }
   pane->form_syncing--;
   if (session->error[0]) g_snprintf(status, sizeof(status), "Form | %s", session->error);
   else g_snprintf(status, sizeof(status), "%s | Draft %llu | Result %llu", state->dirty ? "Draft" : state->submitted ? "Submitted" : "Ready",
      (unsigned long long)state->draft_revision, (unsigned long long)state->result_revision);
   gtk_label_set_text(GTK_LABEL(pane->status), status);
}

static void focus_form(GtkLogPane *pane)
{
   guint i;
   for (i = 0; i < pane->form_fields->len; i++)
   {
      GtkFormField *field = g_ptr_array_index(pane->form_fields, i);
      if (field->editable) { gtk_widget_grab_focus(field->entry); return; }
   }
   gtk_widget_grab_focus(pane->form_submit);
}

static gboolean form_key(GtkLogPane *pane, GtkWidget *focus, GdkEventKey *key)
{
   GtkWidget *targets[THE_WORKBENCH_FIELDS_PER_FORM_MAX + 4];
   size_t count = 0, i, current = 0;
   if (key->keyval == GDK_KEY_Escape) return queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
   if (key->keyval == GDK_KEY_Tab || key->keyval == GDK_KEY_ISO_Left_Tab)
   {
      int reverse = key->keyval == GDK_KEY_ISO_Left_Tab || (key->state & GDK_SHIFT_MASK);
      if (key->state & GDK_CONTROL_MASK) { cycle_log(pane, reverse); return TRUE; }
      for (i = 0; i < pane->form_fields->len; i++)
      {
         GtkFormField *field = g_ptr_array_index(pane->form_fields, i);
         if (field->editable) targets[count++] = field->entry;
      }
      targets[count++] = pane->form_submit; targets[count++] = pane->form_cancel; targets[count++] = pane->form_hide; targets[count++] = pane->form_return;
      for (i = 0; i < count; i++) if (targets[i] == focus) { current = i; break; }
      gtk_widget_grab_focus(targets[(current + count + (reverse ? -1 : 1)) % count]); return TRUE;
   }
   if ((key->state & GDK_CONTROL_MASK) && (key->keyval == GDK_KEY_Return || key->keyval == GDK_KEY_KP_Enter))
   { queue_form(pane, THE_WORKBENCH_FORM_SUBMIT, NULL, NULL); return TRUE; }
   return FALSE; /* GtkEntry owns typing/selection; it never targets the editor. */
}

static void discard_filefields_inputs(const char *panel)
{
   GList *item = ui.inputs.head;
   while (item)
   {
      GList *next = item->next;
      GtkInput *input = item->data;
      if (input->filefields && !strcmp(input->filefields->panel, panel))
      { g_queue_delete_link(&ui.inputs, item); g_free(input->filefields); g_free(input); }
      item = next;
   }
}

static int filefields_inputs_pending(GtkLogPane *pane)
{
   GList *item;
   if (pane->filefields_pending) return 1;
   for (item = ui.inputs.head; item; item = item->next)
   {
      GtkInput *input = item->data;
      if (input->filefields && !strcmp(input->filefields->panel, pane->id)) return 1;
   }
   return 0;
}

static void filefields_receipt(void)
{
   const TheWorkbenchFileFieldsReceipt *receipt = the_workbench_filefields_receipt();
   GtkLogPane *pane;
   if (!ui.filefields_pending || !receipt || receipt->request_id != ui.filefields_pending) return;
   ui.filefields_pending = 0;
   pane = find_log(receipt->panel, receipt->panel_generation);
   if (!pane || receipt->serial == pane->filefields_receipt) return;
   pane->filefields_receipt = receipt->serial; pane->filefields_pending = 0;
   if (!receipt->rc)
   {
      pane->filefields_revision = receipt->draft_revision; pane->filefields_error[0] = 0;
      if (receipt->operation == THE_WORKBENCH_FILEFIELDS_RELOAD) pane->filefields_rebuild = 1;
   }
   else
   {
      g_snprintf(pane->filefields_error, sizeof(pane->filefields_error), "%s; Reload or Cancel", receipt->code);
      discard_filefields_inputs(pane->id);
   }
}

static int queue_filefields(GtkLogPane *pane, TheWorkbenchFileFieldsOperation operation, const char *field, const char *value)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
   GtkInput *input;
   size_t length = value ? strlen(value) : 0;
   if (pane->filefields_syncing || in_macro || the_panel_active || !panel || panel->generation != pane->generation) return 0;
   if (length > THE_WORKBENCH_FIELD_VALUE_MAX || g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT)
   { strcpy(pane->filefields_error, "limit-exceeded"); gtk_widget_error_bell(pane->page); return 0; }
   input = g_new0(GtkInput, 1); input->filefields = g_new0(TheWorkbenchFileFieldsRequest, 1);
   input->filefields->operation = operation; input->filefields->panel_generation = pane->generation;
   strcpy(input->filefields->panel, pane->id);
   if (field) strcpy(input->filefields->field, field);
   if (length) memcpy(input->filefields->value, value, length);
   input->filefields->length = length;
   g_queue_push_tail(&ui.inputs, input); return 1;
}

static void filefields_apply_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_filefields(data, THE_WORKBENCH_FILEFIELDS_APPLY, NULL, NULL); }
static void filefields_cancel_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_filefields(data, THE_WORKBENCH_FILEFIELDS_CANCEL, NULL, NULL); }
static void filefields_reload_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_filefields(data, THE_WORKBENCH_FILEFIELDS_RELOAD, NULL, NULL); }

static gboolean filefields_focused(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
   GtkLogPane *pane = data;
   if (event->in && !ui.syncing_logs && gtk_window_get_focus(GTK_WINDOW(ui.window)) == widget)
      pane->filefields_focus = widget;
   return FALSE;
}

static void filefield_cursor_changed(GObject *object, GParamSpec *property, gpointer data)
{
   GtkFileField *field = data;
   GtkLogPane *pane = field->pane;
   (void)object; (void)property;
   /* GtkNotebook may select an Entry while changing pages. Save only the
    * active user's caret, before RETURN, rather than that automatic focus. */
   if (ui.syncing_logs || strcmp(ui.focused_log, pane->id)
   || gtk_window_get_focus(GTK_WINDOW(ui.window)) != field->entry) return;
   field->caret = gtk_editable_get_position(GTK_EDITABLE(field->entry));
   field->selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(field->entry), &field->selection_start, &field->selection_end);
}

/* Native entry edits are staged through the same fixed-cell panel operations
 * as READ. Only the normalized SET request can change the C-owned draft. */
static ThePanelState *filefields_scratch(GtkFileField *field, int character)
{
   const char *value = gtk_entry_get_text(GTK_ENTRY(field->entry));
   const char *at = g_utf8_offset_to_pointer(value, CLAMP(character, 0, (int)g_utf8_strlen(value, -1)));
   ThePanelState *scratch;
   if (!ui.filefields_scratch) ui.filefields_scratch = g_new0(ThePanelState, 1);
   scratch = ui.filefields_scratch;
   scratch->rows = 1; scratch->cols = field->width; scratch->count = 0;
   scratch->active = -1; scratch->done = scratch->cancelled = 0;
   if (!panel_ui_add(scratch, 0, 0, field->width, 1, 'F', 1, value, strlen(value), 0)) return NULL;
   scratch->cursor = textpos_from_byte((const CHARTYPE *)value, strlen(value), (size_t)(at - value)).cell_column;
   return scratch;
}

static void filefields_commit_entry(GtkFileField *field, ThePanelState *scratch)
{
   GtkLogPane *pane = field->pane;
   const char *value = scratch->fields[0].text;
   size_t length = strlen(value);
   TextPos at = textpos_from_cell((const CHARTYPE *)value, length, scratch->cursor, TEXT_SNAP_BACKWARD);
   if (!queue_filefields(pane, THE_WORKBENCH_FILEFIELDS_SET, field->id, value)) return;
   pane->filefields_syncing++;
   gtk_entry_set_text(GTK_ENTRY(field->entry), value);
   gtk_editable_set_position(GTK_EDITABLE(field->entry), (int)g_utf8_pointer_to_offset(value, value + at.byte_offset));
   pane->filefields_syncing--;
}

static void filefields_insert(GtkEditable *entry, const char *text, int length, int *position, gpointer data)
{
   GtkFileField *field = data;
   GtkLogPane *pane = field->pane;
   ThePanelState *scratch;
   const char *cursor, *end;
   if (pane->filefields_syncing) return;
   g_signal_stop_emission_by_name(entry, "insert-text");
   if (length < 0) length = (int)strlen(text);
   if (!g_utf8_validate(text, length, NULL)) { gtk_widget_error_bell(field->entry); return; }
   scratch = filefields_scratch(field, *position);
   if (!scratch) return;
   for (cursor = text, end = text + length; cursor < end; cursor = g_utf8_next_char(cursor))
   {
      TheInputEvent input;
      gunichar codepoint = g_utf8_get_char(cursor);
      if (codepoint < 32 || codepoint == 127 || !the_input_event_from_text(codepoint, &input) || !panel_ui_event(scratch, &input))
      { gtk_widget_error_bell(field->entry); return; }
   }
   filefields_commit_entry(field, scratch);
   *position = gtk_editable_get_position(entry);
}

static void filefields_delete(GtkEditable *entry, int start, int end, gpointer data)
{
   GtkFileField *field = data;
   GtkLogPane *pane = field->pane;
   ThePanelState *scratch;
   TheInputEvent input = the_input_event_none();
   const char *value, *last;
   int count;
   if (pane->filefields_syncing) return;
   g_signal_stop_emission_by_name(entry, "delete-text");
   value = gtk_entry_get_text(GTK_ENTRY(entry));
   if (end < 0) end = (int)g_utf8_strlen(value, -1);
   scratch = filefields_scratch(field, start);
   if (!scratch) return;
   last = g_utf8_offset_to_pointer(value, CLAMP(end, 0, (int)g_utf8_strlen(value, -1)));
   count = textpos_from_byte((const CHARTYPE *)value, strlen(value), (size_t)(last - value)).cell_column - scratch->cursor;
   input.kind = THE_INPUT_KEY; input.key_code = THE_KEY_DC;
   while (count > 0)
   {
      TextPos at = textpos_from_cell((const CHARTYPE *)scratch->fields[0].text, strlen(scratch->fields[0].text), scratch->cursor, TEXT_SNAP_BACKWARD);
      TextPos next = textpos_next_cluster((const CHARTYPE *)scratch->fields[0].text, strlen(scratch->fields[0].text), at);
      int width = next.cell_column - at.cell_column;
      if (width <= 0 || !panel_ui_event(scratch, &input)) break;
      count -= width;
   }
   filefields_commit_entry(field, scratch);
}

static void populate_filefields(GtkLogPane *pane)
{
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(pane->id);
   GList *children = gtk_container_get_children(GTK_CONTAINER(pane->view)), *item;
   size_t i;
   int columns = 1;
   pane->filefields_focus = NULL;
   for (item = children; item; item = item->next) gtk_widget_destroy(item->data);
   g_list_free(children); g_ptr_array_set_size(pane->filefields, 0);
   if (!state) return;
   for (i = 0; i < state->spans; i++)
   {
      const TheWorkbenchFileFieldSpan *span = the_workbench_filefields_span(pane->id, i);
      GtkWidget *widget;
      char description[192];
      if (!span) continue;
      columns = MAX(columns, span->cell + span->maxcells);
      if (span->editable)
      {
         GtkFileField *field = g_new0(GtkFileField, 1);
         strcpy(field->id, span->id); field->pane = pane; field->width = span->maxcells;
         widget = field->entry = gtk_entry_new();
         gtk_entry_set_has_frame(GTK_ENTRY(widget), FALSE);
         gtk_entry_set_width_chars(GTK_ENTRY(widget), 1); gtk_entry_set_max_width_chars(GTK_ENTRY(widget), 1);
         gtk_entry_set_overwrite_mode(GTK_ENTRY(widget), TRUE);
         gtk_entry_set_text(GTK_ENTRY(widget), span->value);
         gtk_style_context_add_class(gtk_widget_get_style_context(widget), "the-filefield");
         g_signal_connect(widget, "insert-text", G_CALLBACK(filefields_insert), field);
         g_signal_connect(widget, "delete-text", G_CALLBACK(filefields_delete), field);
         g_signal_connect(widget, "focus-in-event", G_CALLBACK(filefields_focused), pane);
         g_signal_connect(widget, "notify::cursor-position", G_CALLBACK(filefield_cursor_changed), field);
         g_signal_connect(widget, "notify::selection-bound", G_CALLBACK(filefield_cursor_changed), field);
         g_signal_connect(widget, "button-press-event", G_CALLBACK(log_button_pressed), pane);
         g_ptr_array_add(pane->filefields, field);
      }
      else
      {
         widget = gtk_label_new(span->value); gtk_label_set_xalign(GTK_LABEL(widget), 0);
         gtk_label_set_yalign(GTK_LABEL(widget), 0.5);
      }
      gtk_style_context_add_class(gtk_widget_get_style_context(widget), "the-filefield-cell");
      gtk_style_context_add_provider(gtk_widget_get_style_context(widget), GTK_STYLE_PROVIDER(ui.filefields_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_snprintf(description, sizeof(description), "%s text, line %lu, cell %d, width %d%s", span->editable ? "Editable fixed-cell" : "Protected",
         (unsigned long)span->line, span->cell + 1, span->maxcells, span->editable ? ". Tab visits fields and buttons; Esc returns, retaining the draft." : "");
      gtk_widget_set_tooltip_text(widget, description);
      atk_object_set_name(gtk_widget_get_accessible(widget), description);
      gtk_widget_set_size_request(widget, MAX(1, span->maxcells * ui.cell_width), ui.cell_height);
      g_object_set_data(G_OBJECT(widget), "the-cell", GINT_TO_POINTER(span->cell));
      g_object_set_data(G_OBJECT(widget), "the-line", GINT_TO_POINTER((int)span->line - 1));
      g_object_set_data(G_OBJECT(widget), "the-width", GINT_TO_POINTER(span->maxcells));
      gtk_fixed_put(GTK_FIXED(pane->view), widget, span->cell * ui.cell_width, ((int)span->line - 1) * ui.cell_height);
   }
   gtk_widget_set_size_request(pane->view, columns * ui.cell_width, MAX(1, (int)state->records) * ui.cell_height);
   gtk_widget_show_all(pane->view);
}

static GtkLogPane *create_filefields(const TheWorkbenchPanel *panel)
{
   GtkLogPane *pane = g_new0(GtkLogPane, 1);
   GtkWidget *buttons, *controls;
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(panel->id);
   strcpy(pane->id, panel->id); pane->generation = panel->generation;
   pane->filefields_revision = state ? state->draft_revision : 0;
   pane->filefields = g_ptr_array_new_with_free_func(g_free);
   pane->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4); gtk_container_set_border_width(GTK_CONTAINER(pane->page), 6);
   gtk_widget_set_name(pane->page, "the-workbench-pane");
   controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
   gtk_box_pack_start(GTK_BOX(pane->page), controls, FALSE, FALSE, 0);
   pane->status = gtk_label_new("File fields"); gtk_widget_set_name(pane->status, "the-workbench-status");
   gtk_label_set_xalign(GTK_LABEL(pane->status), 0); gtk_label_set_line_wrap(GTK_LABEL(pane->status), TRUE);
   gtk_box_pack_start(GTK_BOX(controls), pane->status, FALSE, FALSE, 0);
   buttons = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(buttons), GTK_SELECTION_NONE);
   gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(buttons), 1); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(buttons), 5);
   gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(buttons), FALSE);
   pane->form_submit = gtk_button_new_with_label("Apply"); pane->form_cancel = gtk_button_new_with_label("Cancel");
   pane->filefields_reload = gtk_button_new_with_label("Reload"); pane->form_hide = gtk_button_new_with_label("Hide"); pane->form_return = gtk_button_new_with_label("Return");
   {
      GtkWidget *controls[] = { pane->form_submit, pane->form_cancel, pane->filefields_reload, pane->form_hide, pane->form_return };
      GCallback callbacks[] = { G_CALLBACK(filefields_apply_clicked), G_CALLBACK(filefields_cancel_clicked), G_CALLBACK(filefields_reload_clicked), G_CALLBACK(log_hide_clicked), G_CALLBACK(log_return_clicked) };
      size_t i;
      for (i = 0; i < 5; i++) { gtk_container_add(GTK_CONTAINER(buttons), controls[i]); g_signal_connect(controls[i], "clicked", callbacks[i], pane); g_signal_connect(controls[i], "focus-in-event", G_CALLBACK(filefields_focused), pane); }
   }
   gtk_box_pack_start(GTK_BOX(controls), buttons, FALSE, FALSE, 0);
   pane->scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane->scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_can_focus(pane->scroller, TRUE);
   gtk_widget_set_tooltip_text(pane->scroller, "Fixed-cell CTLCHAR file fields. Scroll both axes; protected text cannot be edited. Tab visits fields, Apply, Cancel, Reload, Hide, Return and this viewport. Ctrl+Tab visits panes; Esc returns without cancelling. Apply changes the editor buffer; Save remains explicit.");
   g_signal_connect(pane->scroller, "focus-in-event", G_CALLBACK(filefields_focused), pane);
   pane->view = gtk_fixed_new(); gtk_container_add(GTK_CONTAINER(pane->scroller), pane->view);
   gtk_box_pack_start(GTK_BOX(pane->page), pane->scroller, TRUE, TRUE, 0);
   populate_filefields(pane);
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), pane->page, gtk_label_new(panel->title));
   gtk_widget_show_all(pane->page); g_ptr_array_add(ui.logs, pane); return pane;
}

static void sync_filefields(GtkLogPane *pane, const TheWorkbenchPanel *panel)
{
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(panel->id);
   char status[256];
   size_t i, j;
   if (!state) { gtk_label_set_text(GTK_LABEL(pane->status), "File fields unavailable"); return; }
   if (pane->filefields_rebuild)
   { pane->filefields_rebuild = 0; populate_filefields(pane); if (!strcmp(ui.focused_log, pane->id)) focus_filefields(pane); }
   if (!filefields_inputs_pending(pane))
   {
      if (state->draft_revision != pane->filefields_revision) strcpy(pane->filefields_error, "stale-revision; Reload or Cancel");
      pane->filefields_syncing++;
      for (i = 0; i < pane->filefields->len; i++)
      {
         GtkFileField *field = g_ptr_array_index(pane->filefields, i);
         for (j = 0; j < state->spans; j++)
         {
            const TheWorkbenchFileFieldSpan *span = the_workbench_filefields_span(panel->id, j);
            if (!span || strcmp(span->id, field->id)) continue;
            if (strcmp(span->value, gtk_entry_get_text(GTK_ENTRY(field->entry))))
            {
               int at = gtk_editable_get_position(GTK_EDITABLE(field->entry));
               gtk_entry_set_text(GTK_ENTRY(field->entry), span->value);
               gtk_editable_set_position(GTK_EDITABLE(field->entry), MIN(at, (int)g_utf8_strlen(span->value, -1)));
            }
            gtk_editable_set_editable(GTK_EDITABLE(field->entry), state->active);
            break;
         }
      }
      pane->filefields_syncing--;
   }
   if (pane->filefields_error[0]) g_snprintf(status, sizeof(status), "File fields | %s", pane->filefields_error);
   else g_snprintf(status, sizeof(status), "File fields | %s | Tab fields/buttons; Esc Return | Apply changes buffer; Save explicitly",
      !state->active ? "Cancelled; Reload to edit" : state->dirty ? "Draft" : "Ready");
   gtk_label_set_text(GTK_LABEL(pane->status), status);
   gtk_widget_set_sensitive(pane->form_submit, state->active);
}

static void focus_filefields(GtkLogPane *pane)
{
   GtkWidget *target = pane->filefields_focus;
   guint i;
   if (!target || !gtk_widget_is_drawable(target) || !gtk_widget_is_sensitive(target))
      target = pane->filefields->len && gtk_widget_is_drawable(pane->scroller)
         ? ((GtkFileField *)g_ptr_array_index(pane->filefields, 0))->entry : pane->filefields_reload;
   /* Notebook page changes can focus a container while sync_logs runs. Only
    * this chosen target becomes the retained field/control for a return. */
   if (GTK_IS_ENTRY(target))
   {
      gtk_entry_grab_focus_without_selecting(GTK_ENTRY(target));
      for (i = 0; i < pane->filefields->len; i++)
      {
         GtkFileField *field = g_ptr_array_index(pane->filefields, i);
         if (field->entry != target) continue;
         if (field->selected)
            gtk_editable_select_region(GTK_EDITABLE(target), field->caret == field->selection_start ? field->selection_end : field->selection_start, field->caret);
         else gtk_editable_set_position(GTK_EDITABLE(target), field->caret);
         break;
      }
   }
   else gtk_widget_grab_focus(target);
   pane->filefields_focus = target;
}

static gboolean filefields_key(GtkLogPane *pane, GtkWidget *focus, GdkEventKey *key)
{
   GtkWidget *targets[THE_WORKBENCH_FILEFIELDS_SPANS_MAX + 6];
   size_t count = 0, i, current = 0;
   if (key->keyval == GDK_KEY_Escape) return queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
   if (key->keyval == GDK_KEY_Tab || key->keyval == GDK_KEY_ISO_Left_Tab)
   {
      int reverse = key->keyval == GDK_KEY_ISO_Left_Tab || (key->state & GDK_SHIFT_MASK);
      if (key->state & GDK_CONTROL_MASK) { cycle_log(pane, reverse); return TRUE; }
      if (gtk_widget_get_visible(pane->scroller))
         for (i = 0; i < pane->filefields->len; i++) targets[count++] = ((GtkFileField *)g_ptr_array_index(pane->filefields, i))->entry;
      if (gtk_widget_get_sensitive(pane->form_submit)) targets[count++] = pane->form_submit;
      targets[count++] = pane->form_cancel; targets[count++] = pane->filefields_reload;
      targets[count++] = pane->form_hide; targets[count++] = pane->form_return;
      if (gtk_widget_get_visible(pane->scroller)) targets[count++] = pane->scroller;
      for (i = 0; i < count; i++) if (targets[i] == focus) { current = i; break; }
      gtk_widget_grab_focus(targets[(current + count + (reverse ? -1 : 1)) % count]); return TRUE;
   }
   if ((key->state & GDK_CONTROL_MASK) && (key->keyval == GDK_KEY_Return || key->keyval == GDK_KEY_KP_Enter))
   { queue_filefields(pane, THE_WORKBENCH_FILEFIELDS_APPLY, NULL, NULL); return TRUE; }
   if (key->keyval == GDK_KEY_Page_Up || key->keyval == GDK_KEY_Page_Down)
   {
      GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
      double at = gtk_adjustment_get_value(adjustment), step = gtk_adjustment_get_page_increment(adjustment);
      gtk_adjustment_set_value(adjustment, at + (key->keyval == GDK_KEY_Page_Up ? -step : step)); return TRUE;
   }
   return FALSE;
}

static void cancel_log_scroll(GtkLogPane *pane)
{
   if (pane->scroll_clock && pane->scroll_handler)
      g_signal_handler_disconnect(pane->scroll_clock, pane->scroll_handler);
   pane->scroll_handler = 0;
   g_clear_object(&pane->scroll_clock); g_clear_object(&pane->scroll_adjustment);
}

static void log_scroll_after_paint(GdkFrameClock *clock, gpointer data)
{
   GtkLogPane *pane = data;
   (void)clock;
   gtk_adjustment_set_value(pane->scroll_adjustment, pane->scroll_value);
   cancel_log_scroll(pane);
}

static void free_log_pane(gpointer data)
{
   GtkLogPane *pane = data;
   if (pane->form_fields) g_ptr_array_free(pane->form_fields, TRUE);
   if (pane->filefields) g_ptr_array_free(pane->filefields, TRUE);
   cancel_log_scroll(pane); g_free(pane);
}

static GtkLogPane *create_log(const TheWorkbenchPanel *panel)
{
   if (panel->kind == THE_WORKBENCH_PANEL_FIELDS && panel->document_id) return create_filefields(panel);
   GtkLogPane *pane = g_new0(GtkLogPane, 1);
   GtkWidget *controls, *hide, *back;
   GtkTextBuffer *buffer;
   strcpy(pane->id, panel->id); pane->generation = panel->generation;
   pane->revision = UINT64_MAX;
   pane->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   gtk_widget_set_name(pane->page, "the-workbench-pane");
   gtk_container_set_border_width(GTK_CONTAINER(pane->page), 6);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   pane->status = gtk_label_new(panel->kind == THE_WORKBENCH_PANEL_FIELDS ? "Form" : panel->kind == THE_WORKBENCH_PANEL_DOCUMENT ? "Read-only document" : "Read-only log");
   gtk_widget_set_name(pane->status, "the-workbench-status");
   gtk_label_set_xalign(GTK_LABEL(pane->status), 0);
   gtk_label_set_ellipsize(GTK_LABEL(pane->status), PANGO_ELLIPSIZE_END);
   gtk_box_pack_start(GTK_BOX(controls), pane->status, TRUE, TRUE, 0);
   hide = gtk_button_new_with_label("Hide"); back = gtk_button_new_with_label("Return");
   pane->form_return = back; pane->form_hide = hide;
   g_signal_connect(hide, "clicked", G_CALLBACK(log_hide_clicked), pane);
   g_signal_connect(back, "clicked", G_CALLBACK(log_return_clicked), pane);
   gtk_box_pack_start(GTK_BOX(controls), hide, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(controls), back, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(pane->page), controls, FALSE, FALSE, 0);
   pane->scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane->scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   if (panel->kind == THE_WORKBENCH_PANEL_FIELDS) create_form(pane, panel, controls);
   else
   {
   pane->view = gtk_text_view_new();
   gtk_widget_set_name(pane->view, "the-workbench-text");
   g_signal_connect(pane->view, "button-press-event", G_CALLBACK(log_button_pressed), pane);
   g_signal_connect(pane->view, "scroll-event", G_CALLBACK(document_scroll), pane);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(pane->view), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(pane->view), FALSE);
   gtk_text_view_set_monospace(GTK_TEXT_VIEW(pane->view), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pane->view), GTK_WRAP_CHAR);
   gtk_widget_set_tooltip_text(pane->view, panel->kind == THE_WORKBENCH_PANEL_DOCUMENT
      ? "Read-only document. Arrows and Page Up/Down scroll; Home/End visits the beginning/end; Tab visits panes; H hides; Esc returns."
      : "Read-only log. Page Up/Down and Home/End scroll; Tab visits panes; H hides; Esc returns.");
   }
   gtk_container_add(GTK_CONTAINER(pane->scroller), pane->view);
   gtk_box_pack_start(GTK_BOX(pane->page), pane->scroller, TRUE, TRUE, 0);
   if (panel->kind != THE_WORKBENCH_PANEL_FIELDS)
   {
      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
      gtk_text_buffer_create_tag(buffer, "stdout", NULL);
      gtk_text_buffer_create_tag(buffer, "stderr", "foreground", "tomato", NULL);
   }
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), pane->page, gtk_label_new(panel->title));
   gtk_widget_show_all(pane->page);
   g_ptr_array_add(ui.logs, pane);
   return pane;
}

static void append_log_text(GString *text, const char *bytes, size_t length)
{
   size_t i;
   for (i = 0; i < length; i++)
   {
      unsigned char c = (unsigned char)bytes[i];
      if ((c < 32 && c != '\n') || c == 127) g_string_append_printf(text, "\\x%02X", c);
      else if (c == 0xc2 && i + 1 < length &&
               (unsigned char)bytes[i + 1] >= 0x80 && (unsigned char)bytes[i + 1] <= 0x9f)
         g_string_append_printf(text, "\\u00%02X", (unsigned char)bytes[++i]);
      else g_string_append_c(text, bytes[i]);
   }
}

static void sync_log_text(GtkLogPane *pane, const TheWorkbenchPanel *panel)
{
   const TheWorkbenchLogInfo *info = the_workbench_log_find(panel->model);
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
   GtkTextIter insert, anchor, end, first;
   TheWorkbenchLogPage page;
   GString *text;
   uint64_t cursor = 0;
   size_t offset = 0, i, new_count = 0;
   int selected, insert_offset, anchor_offset, removed = 0;
   int follow, incremental = 0;
   double scroll, deleted_y = 0;
   char status[160];
   if (!info || info->generation != panel->model_generation || pane->revision == info->revision) return;
   pane->revision = info->revision;
   selected = gtk_text_buffer_get_has_selection(buffer);
   gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
   gtk_text_buffer_get_iter_at_mark(buffer, &anchor, gtk_text_buffer_get_selection_bound(buffer));
   insert_offset = gtk_text_iter_get_offset(&insert); anchor_offset = gtk_text_iter_get_offset(&anchor);
   scroll = gtk_adjustment_get_value(adjustment);
   follow = !selected && scroll >= gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment) - 2;
   if (pane->record_count && info->first_cursor != pane->records[0].cursor)
   {
      GdkRectangle location;
      removed = gtk_text_buffer_get_char_count(buffer);
      for (i = 0; i < pane->record_count; i++)
         if (pane->records[i].cursor == info->first_cursor) { removed = pane->records[i].character; break; }
      gtk_text_buffer_get_iter_at_offset(buffer, &end, removed);
      gtk_text_view_get_iter_location(GTK_TEXT_VIEW(pane->view), &end, &location); deleted_y = location.y;
   }
   cancel_log_scroll(pane);
   if (pane->record_count && info->records)
   {
      for (i = 0; i < pane->record_count; i++)
         if (pane->records[i].cursor == info->first_cursor)
         {
            size_t retained = pane->record_count - i, j;
            incremental = 1;
            if (removed)
            {
               gtk_text_buffer_get_start_iter(buffer, &first);
               gtk_text_buffer_get_iter_at_offset(buffer, &end, removed);
               gtk_text_buffer_delete(buffer, &first, &end);
            }
            memmove(pane->records, pane->records + i, retained * sizeof(pane->records[0]));
            for (j = 0; j < retained; j++) pane->records[j].character -= removed;
            new_count = retained; cursor = pane->next_cursor;
            break;
         }
   }
   if (!incremental) gtk_text_buffer_set_text(buffer, "", 0);
   text = g_string_new(NULL);
   do
   {
      if (the_workbench_log_page(panel->model, panel->model_generation, cursor, offset,
          THE_WORKBENCH_LOG_PAGE_MAX, THE_WORKBENCH_LOG_TEXT_MAX, &page) != THE_WORKBENCH_LOG_OK) break;
      for (i = 0; i < page.count; i++)
      {
         const TheWorkbenchLogSlice *slice = page.slices + i;
         if (new_count < THE_WORKBENCH_LOG_RECORD_MAX && (!new_count || pane->records[new_count - 1].cursor != slice->cursor))
         {
            pane->records[new_count].cursor = slice->cursor;
            pane->records[new_count++].character = gtk_text_buffer_get_char_count(buffer);
         }
         g_string_truncate(text, 0); append_log_text(text, slice->text, slice->length);
         gtk_text_buffer_get_end_iter(buffer, &end);
         gtk_text_buffer_insert_with_tags_by_name(buffer, &end, text->str, text->len,
                                                 slice->stream == 2 ? "stderr" : "stdout", NULL);
      }
      cursor = page.next_cursor; offset = page.next_offset;
   } while (page.more);
   g_string_free(text, TRUE); pane->record_count = new_count; pane->next_cursor = info->next_cursor;
   if (selected && !incremental)
   {
      int count = gtk_text_buffer_get_char_count(buffer);
      gtk_text_buffer_get_iter_at_offset(buffer, &insert, CLAMP(insert_offset - removed, 0, count));
      gtk_text_buffer_get_iter_at_offset(buffer, &anchor, CLAMP(anchor_offset - removed, 0, count));
      gtk_text_buffer_select_range(buffer, &insert, &anchor);
   }
   if (follow)
   {
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pane->view), &end, 0, FALSE, 0, 1);
   }
   else if (incremental && !removed) gtk_adjustment_set_value(adjustment, scroll);
   else
   {
      /* TextView validates layout and may scroll its insertion mark later.
       * Restore the user's viewport after that frame without moving focus. */
      pane->scroll_value = MAX(0, scroll - deleted_y);
      pane->scroll_clock = gtk_widget_get_frame_clock(pane->view);
      if (pane->scroll_clock)
      {
         g_object_ref(pane->scroll_clock);
         pane->scroll_adjustment = g_object_ref(adjustment);
         pane->scroll_handler = g_signal_connect(pane->scroll_clock, "after-paint", G_CALLBACK(log_scroll_after_paint), pane);
         gtk_widget_queue_draw(pane->view);
      }
      else gtk_adjustment_set_value(adjustment, pane->scroll_value);
   }
   if (info->omitted_bytes) g_snprintf(status, sizeof(status), "History omitted: %llu bytes", (unsigned long long)info->omitted_bytes);
   else g_snprintf(status, sizeof(status), "Read-only log");
   gtk_label_set_text(GTK_LABEL(pane->status), status);
}

static void append_document_text(GString *text, const unsigned char *bytes, size_t length)
{
   size_t i = 0;
   while (i < length)
   {
      gunichar cp = g_utf8_get_char_validated((const char *)bytes + i, length - i);
      size_t n = cp == (gunichar)-1 || cp == (gunichar)-2 ? 1
         : (size_t)(g_utf8_next_char((const char *)bytes + i) - ((const char *)bytes + i));
      if (cp == (gunichar)-1 || cp == (gunichar)-2 || cp < 32 || cp == 127)
         g_string_append_printf(text, "\\x%02X", bytes[i]);
      else if (cp >= 128 && cp <= 159) g_string_append_printf(text, "\\u%04X", cp);
      else g_string_append_len(text, (const char *)bytes + i, n);
      i += n;
   }
}

static void sync_document_text(GtkLogPane *pane, const TheWorkbenchPanel *panel)
{
   TheWorkbenchDocumentInfo info;
   TheWorkbenchDocumentPage page;
   TheWorkbenchDocumentResult result = the_workbench_document_info(panel->document_id, &info);
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
   GtkAllocation allocation;
   GtkDocumentCursor cursor;
   GtkTextIter first, last;
   GString *text;
   size_t raw = 0, lines = 0, i;
   int budget, request = pane->document_request, a = 0, b = 0;
   gboolean selection;
   double scroll;
   char status[192];
   if (result != THE_WORKBENCH_DOCUMENT_OK || !info.available)
   {
      g_snprintf(status, sizeof(status), "Read-only document | %s", info.reason ? info.reason : the_workbench_document_result_name(result));
      if (result == THE_WORKBENCH_DOCUMENT_MISSING) gtk_text_buffer_set_text(buffer, "", 0);
      gtk_label_set_text(GTK_LABEL(pane->status), status);
      return;
   }
   pane->document_lines = info.lines;
   gtk_widget_get_allocation(pane->view, &allocation);
   budget = CLAMP(allocation.height / MAX(1, ui.cell_height), 1, 64);
   if (!pane->document_first.line) pane->document_first.line = 1;
   if (pane->document_first.line > MAX((size_t)1, info.lines))
   { pane->document_first.line = MAX((size_t)1, info.lines); pane->document_first.offset = 0; }
   if (pane->revision != info.revision || request || pane->document_budget != budget)
   {
      if (request) pane->document_tail = request == 2;
      if (request == -2) { pane->document_first.line = 1; pane->document_first.offset = 0; pane->document_previous_count = 0; }
      else if (request == -1)
      {
         if (pane->document_previous_count) pane->document_first = pane->document_previous[--pane->document_previous_count];
         else { pane->document_first.line = pane->document_first.line > (size_t)budget ? pane->document_first.line - budget : 1; pane->document_first.offset = 0; }
      }
      else if (request == 1 && pane->document_more)
      {
         if (pane->document_previous_count == G_N_ELEMENTS(pane->document_previous))
         { memmove(pane->document_previous, pane->document_previous + 1, 31 * sizeof(pane->document_previous[0])); pane->document_previous_count--; }
         pane->document_previous[pane->document_previous_count++] = pane->document_first;
         pane->document_first = pane->document_next;
      }
      else if (request == 2)
      {
         pane->document_first.line = MAX((size_t)1, info.lines); pane->document_first.offset = 0;
         if (the_workbench_document_read(panel->document_id, info.revision, pane->document_first.line, 0, 1,
             THE_WORKBENCH_DOCUMENT_BYTES_MAX, &page) == THE_WORKBENCH_DOCUMENT_OK && page.count)
            pane->document_first.offset = page.slices[0].original_length > THE_WORKBENCH_DOCUMENT_BYTES_MAX
               ? page.slices[0].original_length - THE_WORKBENCH_DOCUMENT_BYTES_MAX : 0;
      }
      pane->document_request = 0; cursor = pane->document_first;
      text = g_string_sized_new(THE_WORKBENCH_DOCUMENT_BYTES_MAX);
      do
      {
         unsigned retry = 0;
         do {
            result = the_workbench_document_read(panel->document_id, info.revision, cursor.line, cursor.offset,
               MIN((size_t)THE_WORKBENCH_DOCUMENT_LINES_MAX, (size_t)budget - lines),
               THE_WORKBENCH_DOCUMENT_BYTES_MAX - raw, &page);
            if (result != THE_WORKBENCH_DOCUMENT_INVALID || !cursor.offset || request != 2 || retry++ == 3) break;
            cursor.offset++; pane->document_first = cursor;
         } while (1);
         if (result == THE_WORKBENCH_DOCUMENT_INVALID && cursor.offset && pane->revision != info.revision)
         {
            cursor.offset = 0; pane->document_first = cursor;
            result = the_workbench_document_read(panel->document_id, info.revision, cursor.line, 0,
               MIN((size_t)THE_WORKBENCH_DOCUMENT_LINES_MAX, (size_t)budget - lines), THE_WORKBENCH_DOCUMENT_BYTES_MAX - raw, &page);
         }
         if (result != THE_WORKBENCH_DOCUMENT_OK) break;
         for (i = 0; i < page.count; i++)
         {
            const TheWorkbenchDocumentSlice *slice = page.slices + i;
            append_document_text(text, page.bytes + slice->data_offset, slice->length);
            if (slice->offset + slice->length == slice->original_length) g_string_append_c(text, '\n');
         }
         raw += page.length; lines += page.count;
         cursor.line = page.next_line; cursor.offset = page.next_offset; pane->document_more = page.more;
      } while (page.more && lines < (size_t)budget && raw < THE_WORKBENCH_DOCUMENT_BYTES_MAX);
      if (result == THE_WORKBENCH_DOCUMENT_OK || (raw && result == THE_WORKBENCH_DOCUMENT_LIMIT))
      {
         selection = !request && gtk_text_buffer_get_selection_bounds(buffer, &first, &last);
         if (selection) { a = gtk_text_iter_get_offset(&first); b = gtk_text_iter_get_offset(&last); }
         scroll = request ? 0 : gtk_adjustment_get_value(adjustment);
         gtk_text_buffer_set_text(buffer, text->str, text->len);
         if (selection)
         {
            int count = gtk_text_buffer_get_char_count(buffer);
            gtk_text_buffer_get_iter_at_offset(buffer, &first, MIN(a, count)); gtk_text_buffer_get_iter_at_offset(buffer, &last, MIN(b, count));
            gtk_text_buffer_select_range(buffer, &last, &first);
         }
         cancel_log_scroll(pane); pane->scroll_value = pane->document_tail ? G_MAXDOUBLE : scroll;
         pane->scroll_clock = gtk_widget_get_frame_clock(pane->view);
         if (pane->scroll_clock)
         {
            g_object_ref(pane->scroll_clock); pane->scroll_adjustment = g_object_ref(adjustment);
            pane->scroll_handler = g_signal_connect(pane->scroll_clock, "after-paint", G_CALLBACK(log_scroll_after_paint), pane);
            gtk_widget_queue_draw(pane->view);
         }
         pane->document_next = cursor; pane->revision = info.revision; pane->document_budget = budget;
      }
      g_string_free(text, TRUE);
   }
   if (result != THE_WORKBENCH_DOCUMENT_OK && result != THE_WORKBENCH_DOCUMENT_LIMIT)
      g_snprintf(status, sizeof(status), "Read-only document | %s", the_workbench_document_result_name(result));
   else g_snprintf(status, sizeof(status), "Read-only document | Line %lu/%lu%s%s", (unsigned long)pane->document_first.line,
      (unsigned long)info.lines, pane->document_first.offset ? " (continued)" : "", info.dirty ? " | Modified" : "");
   gtk_label_set_text(GTK_LABEL(pane->status), status);
}

static gboolean document_key(GtkLogPane *pane, guint key)
{
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
   double at = gtk_adjustment_get_value(adjustment);
   double bottom = MAX(0, gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));
   double step = key == GDK_KEY_Up || key == GDK_KEY_Down ? ui.cell_height : gtk_adjustment_get_page_size(adjustment);
   if (key == GDK_KEY_Up || key == GDK_KEY_Down || key == GDK_KEY_Page_Up || key == GDK_KEY_Page_Down || key == GDK_KEY_Home)
      pane->document_tail = 0;
   if (key == GDK_KEY_Home) pane->document_request = -2;
   else if (key == GDK_KEY_End) pane->document_request = 2;
   else if (key == GDK_KEY_Down || key == GDK_KEY_Page_Down)
   { if (at < bottom - 1) gtk_adjustment_set_value(adjustment, MIN(bottom, at + step)); else if (pane->document_more) pane->document_request = 1; }
   else if (key == GDK_KEY_Up || key == GDK_KEY_Page_Up)
   { if (at > 1) gtk_adjustment_set_value(adjustment, MAX(0, at - step)); else pane->document_request = -1; }
   /* Every other edit/typing key stays inside this read-only surface. */
   if (pane->document_request) sync_logs();
   return TRUE;
}

static gboolean document_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
   GtkLogPane *pane = data;
   const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
   double dx = 0, dy = 0;
   (void)widget;
   if (!panel || panel->kind != THE_WORKBENCH_PANEL_DOCUMENT) return FALSE;
   if (event->direction == GDK_SCROLL_UP) return document_key(pane, GDK_KEY_Up);
   if (event->direction == GDK_SCROLL_DOWN) return document_key(pane, GDK_KEY_Down);
   if (gdk_event_get_scroll_deltas((GdkEvent *)event, &dx, &dy) && dy)
      return document_key(pane, dy < 0 ? GDK_KEY_Up : GDK_KEY_Down);
   return TRUE;
}

static const char *log_error_name(TheWorkbenchLogResult result)
{
   switch (result)
   {
      case THE_WORKBENCH_LOG_INVALID: return "invalid-output";
      case THE_WORKBENCH_LOG_MISSING: return "missing-model";
      case THE_WORKBENCH_LOG_STALE: return "stale-model";
      case THE_WORKBENCH_LOG_LIMIT: return "limit-exceeded";
      case THE_WORKBENCH_LOG_NOMEM: return "out-of-memory";
      default: return "ok";
   }
}

static void sync_logs(void)
{
   const TheWorkbenchFocusRequest *focus;
   guint i;
   int visible = 0;
   if (!ui.auxiliary || !ui.logs) return;
   form_receipt();
   filefields_receipt();
   ui.syncing_logs++;
   for (i = 0; i < ui.logs->len; )
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
      if (!panel || panel->generation != pane->generation ||
          (pane->form_fields && panel->form_generation != pane->form_generation))
      { gtk_widget_destroy(pane->page); g_ptr_array_remove_index(ui.logs, i); continue; }
      i++;
   }
   for (i = 0; i < the_workbench_panel_count(); i++)
   {
      const TheWorkbenchPanel *panel = the_workbench_panel_at(i);
      const TheWorkbenchPanelState *state = the_workbench_panel_state(panel->id);
      GtkLogPane *pane;
      if (panel->kind != THE_WORKBENCH_PANEL_LOG && panel->kind != THE_WORKBENCH_PANEL_DOCUMENT && panel->kind != THE_WORKBENCH_PANEL_FIELDS) continue;
      pane = find_log(panel->id, panel->generation);
      if (!pane) pane = create_log(panel);
      gtk_widget_set_visible(pane->page, state && state->visible);
      if (state && state->visible)
      {
         const TheWorkbenchModelState *model = the_workbench_model_state(panel->model);
         const TheWorkbenchLogInfo *info = the_workbench_log_find(panel->model);
         char status[160];
         visible++;
         if (panel->kind == THE_WORKBENCH_PANEL_FIELDS) { if (panel->document_id) sync_filefields(pane, panel); else sync_form(pane, panel); continue; }
         if (panel->kind == THE_WORKBENCH_PANEL_DOCUMENT) { sync_document_text(pane, panel); continue; }
         sync_log_text(pane, panel);
         if (model && model->last_error != THE_WORKBENCH_LOG_OK)
            g_snprintf(status, sizeof(status), "Log output error: %s", log_error_name(model->last_error));
         else if (info && info->omitted_bytes)
            g_snprintf(status, sizeof(status), "History omitted: %llu bytes", (unsigned long long)info->omitted_bytes);
         else g_strlcpy(status, "Read-only log", sizeof(status));
         if (strcmp(gtk_label_get_text(GTK_LABEL(pane->status)), status)) gtk_label_set_text(GTK_LABEL(pane->status), status);
      }
   }
   gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ui.auxiliary), visible > 0 || gtk_widget_get_visible(ui.setup));
   gtk_widget_set_visible(ui.auxiliary, visible || the_tool_program_console()->visible || gtk_widget_get_visible(ui.setup));
   focus = the_workbench_focus_request();
   if (focus && focus->serial != ui.log_focus_serial)
   {
      GtkLogPane *pane = find_log(focus->panel, focus->generation);
      ui.log_focus_serial = focus->serial;
      if (pane && gtk_widget_get_visible(pane->page))
      {
         GtkWidget *prior_focus = gtk_window_get_focus(GTK_WINDOW(ui.window));
         if (!ui.focused_log[0] && !ui.log_origin) remember_log_origin(gtk_window_get_focus(GTK_WINDOW(ui.window)));
         strcpy(ui.focused_log, pane->id); ui.focused_log_generation = pane->generation;
         if (the_tool_program_console()->focused) the_tool_program_console_visibility(the_tool_program_console()->visible, 0);
         gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), pane->page));
         if (pane->filefields) focus_filefields(pane);
         else if (pane->form_fields)
         {
            const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
            const TheWorkbenchFormState *state = the_workbench_form_state(panel->form);
            GtkFormSession *session = form_session(panel->form);
            if (state && session && !session->pending &&
                (session->blocked || session->generation != state->generation || session->revision != state->draft_revision))
            {
               /* An initial FOCUS may already have valid typing queued behind
                * it. Discard only requests from an explicitly stale session. */
               discard_form_inputs(panel->form); session->generation = state->generation; session->revision = state->draft_revision;
               session->blocked = 0; session->error[0] = 0;
            }
            if (prior_focus && GTK_IS_ENTRY(prior_focus) && gtk_widget_is_ancestor(prior_focus, pane->page))
               gtk_widget_grab_focus(prior_focus);
            else focus_form(pane);
         }
         else gtk_widget_grab_focus(pane->view);
      }
      else if (!focus->panel[0] && ui.focused_log[0])
      { ui.focused_log[0] = '\0'; restore_log_origin(); }
   }
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      int shown = gtk_widget_get_visible(pane->page);
      int selected_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) == pane->page;
      TheWorkbenchPlacement placement = !shown ? THE_WORKBENCH_PLACE_NONE : !selected_page ? THE_WORKBENCH_PLACE_TAB
         : gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == GTK_ORIENTATION_HORIZONTAL
            ? THE_WORKBENCH_PLACE_RIGHT : THE_WORKBENCH_PLACE_BELOW;
      const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
      GtkAllocation allocation;
      int fits = 1, allowed = 0, controls_height = 0;
      size_t fallback;
      gtk_widget_get_allocation(pane->page, &allocation);
      /* Wrapped status/form controls need the selected pane's real width.
       * The width-independent minimum can assume a single FlowBox column
       * and incorrectly collapse fields that fit in the allocated pane. */
      gtk_widget_get_preferred_height_for_width(gtk_widget_get_parent(pane->status),
         MAX(1, allocation.width - 12), &controls_height, NULL);
      if (shown && selected_page && panel && allocation.width > 1 && allocation.height > 1)
         fits = allocation.width - 12 >= panel->mincols * ui.cell_width &&
            allocation.height - 16 - controls_height >= panel->minrows * ui.cell_height;
      if (panel && shown && selected_page)
      {
         if (panel->placement == placement || panel->placement == THE_WORKBENCH_PLACE_TAB) allowed = 1;
         for (fallback = 0; fallback < panel->fallback_count; fallback++)
            if (panel->fallbacks[fallback] == placement) allowed = 1;
         if (!allowed)
            for (fallback = 0; fallback < panel->fallback_count; fallback++)
               if (panel->fallbacks[fallback] == THE_WORKBENCH_PLACE_TAB) { placement = THE_WORKBENCH_PLACE_TAB; allowed = 1; break; }
         if (panel->placement == THE_WORKBENCH_PLACE_TAB) placement = THE_WORKBENCH_PLACE_TAB;
      }
      else allowed = 1;
      if (!fits || !allowed) placement = THE_WORKBENCH_PLACE_NONE;
      if (shown && selected_page)
      {
         gtk_widget_set_visible(pane->scroller, fits && allowed);
         if (!fits || !allowed) gtk_label_set_text(GTK_LABEL(pane->status),
            panel && panel->kind == THE_WORKBENCH_PANEL_FIELDS ? "Fields collapsed: insufficient space; Return and recovery controls remain available" : panel && panel->kind == THE_WORKBENCH_PANEL_DOCUMENT
               ? "Document collapsed: insufficient space" : "Log collapsed: insufficient space");
      }
      the_workbench_panel_effective(pane->id, pane->generation, placement,
                                   !shown ? "hidden" : !fits ? "insufficient-space" : !allowed ? "placement-unavailable" : !selected_page ? "auxiliary-tab" : "");
   }
   ui.syncing_logs--;
}

static gboolean blink(gpointer data)
{
   (void)data;
   ui.blink_visible = !ui.blink_visible;
   if (cursor_blink() == CURSOR_BLINK)
      redraw();
   return G_SOURCE_CONTINUE;
}

static GtkWidget *button(GtkWidget *box, const char *label,
                          GCallback callback, gconstpointer argument)
{
   GtkWidget *widget = gtk_button_new_with_label(label);
   gtk_style_context_add_class(gtk_widget_get_style_context(widget), "the-console-control");
   gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0);
   g_signal_connect(widget, "clicked", callback, (gpointer)argument);
   return widget;
}

/* Fonts are renderer preferences, deliberately independent of Rexx profiles. */
static int font_available(const PangoFontDescription *font)
{
   PangoFontFamily **families;
   const char *wanted = pango_font_description_get_family(font);
   int count, i, valid = 0;
   double points = (double)pango_font_description_get_size(font) / PANGO_SCALE;
   if (!wanted || points < 6 || points > 48 || pango_font_description_get_size_is_absolute(font)) return 0;
   if (!g_ascii_strcasecmp(wanted, "Monospace")) return 1;
   pango_context_list_families(gtk_widget_get_pango_context(ui.editor), &families, &count);
   for (i = 0; i < count; i++)
      if (pango_font_family_is_monospace(families[i]) && !g_ascii_strcasecmp(wanted, pango_font_family_get_name(families[i]))) valid = 1;
   g_free(families);
   return valid;
}

static void measure_font(void)
{
   PangoLayout *layout;
   GtkAllocation allocation;
   int width, height;
   guint i;
   if (!ui.font || !ui.editor) return;
   pango_font_description_set_weight(ui.font, PANGO_WEIGHT_NORMAL);
   pango_font_description_set_style(ui.font, PANGO_STYLE_NORMAL);
   layout = gtk_widget_create_pango_layout(ui.editor, "M");
   pango_layout_set_font_description(layout, ui.font);
   pango_layout_get_pixel_size(layout, &width, &height);
   g_object_unref(layout);
   if (ui.cell_width == MAX(1, width) && ui.cell_height == MAX(1, height + 2))
   { redraw(); return; }
   ui.cell_width = MAX(1, width); ui.cell_height = MAX(1, height + 2);
   gtk_widget_set_size_request(ui.editor, 40 * ui.cell_width, 12 * ui.cell_height);
   gtk_widget_get_allocation(ui.editor, &allocation);
   if (allocation.width > 1 && allocation.height > 1) allocated(ui.editor, &allocation, NULL);
   if (ui.logs) for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      if (pane->filefields && pane->view)
      {
         GList *children = gtk_container_get_children(GTK_CONTAINER(pane->view)), *item;
         int columns = 0, rows = 0;
         for (item = children; item; item = item->next)
         {
            GtkWidget *field = item->data;
            int cell = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(field), "the-cell"));
            int line = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(field), "the-line"));
            int cells = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(field), "the-width"));
            gtk_widget_set_size_request(field, MAX(1, cells * ui.cell_width), ui.cell_height);
            gtk_fixed_move(GTK_FIXED(pane->view), field, cell * ui.cell_width, line * ui.cell_height);
            columns = MAX(columns, cell + cells); rows = MAX(rows, line + 1);
         }
         g_list_free(children);
         gtk_widget_set_size_request(pane->view, columns * ui.cell_width, rows * ui.cell_height);
      }
   }
   ui.split_positioned = 0;
   if (ui.split) gtk_widget_queue_resize(ui.split);
   redraw(); /* read_input emits the ordinary owner resize for the new grid. */
}

static void font_style_changed(GtkWidget *widget, gpointer data)
{ (void)widget; (void)data; measure_font(); }

static void apply_font(const PangoFontDescription *font)
{
   char *family, *css;
   PangoFontDescription *copy = pango_font_description_copy(font);
   if (ui.font) pango_font_description_free(ui.font);
   ui.font = copy;
   if (!ui.font_css)
   {
      ui.font_css = gtk_css_provider_new();
      gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(ui.window), GTK_STYLE_PROVIDER(ui.font_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);
   }
   if (!ui.filefields_css) ui.filefields_css = gtk_css_provider_new();
   family = g_strescape(pango_font_description_get_family(ui.font), NULL);
   css = g_strdup_printf("#the-console-output, #the-build-output, #the-workbench-text, #the-console-input, .the-fixed-field, .the-filefield-cell { font-family: \"%s\"; font-size: %.2fpt; } entry.the-filefield { padding: 0; border: 0; min-width: 0; min-height: 0; border-radius: 0; }", family, (double)pango_font_description_get_size(ui.font) / PANGO_SCALE);
   gtk_css_provider_load_from_data(ui.font_css, css, -1, NULL);
   gtk_css_provider_load_from_data(ui.filefields_css, css, -1, NULL);
   g_free(css); g_free(family);
   measure_font();
}

static char *font_preferences_path(void)
{ return g_build_filename(g_get_user_config_dir(), "crxthe", "gtk-preferences.ini", NULL); }

static int save_font_preferences(void)
{
   GKeyFile *settings = g_key_file_new();
   GError *error = NULL;
   char *path = font_preferences_path(), *directory = g_path_get_dirname(path);
   char *font = pango_font_description_to_string(ui.font), *bytes;
   gsize length;
   int ok;
   g_key_file_set_integer(settings, "GTK", "version", 1);
   g_key_file_set_string(settings, "GTK", "font", font);
   bytes = g_key_file_to_data(settings, &length, NULL);
   ok = !g_mkdir_with_parents(directory, 0700) && g_file_set_contents(path, bytes, length, &error);
   ui.font_unsaved = !ok;
   g_snprintf(ui.font_status, sizeof(ui.font_status), "%s", ok ? "Appearance saved on this device." : "Appearance applies to this session only; preferences could not be saved.");
   g_clear_error(&error); g_free(bytes); g_free(font); g_free(directory); g_free(path); g_key_file_unref(settings);
   return ok;
}

static void restore_font_preferences(void)
{
   GKeyFile *settings = g_key_file_new();
   char *path = font_preferences_path();
   PangoFontDescription *font = pango_font_description_from_string("Monospace 12");
   if (g_key_file_load_from_file(settings, path, G_KEY_FILE_NONE, NULL))
   {
      char *value = g_key_file_get_string(settings, "GTK", "font", NULL);
      PangoFontDescription *saved = value ? pango_font_description_from_string(value) : NULL;
      if (g_key_file_get_integer(settings, "GTK", "version", NULL) == 1 && saved && font_available(saved))
      { pango_font_description_free(font); font = saved; saved = NULL; }
      else g_strlcpy(ui.font_status, "Saved appearance is unavailable; using Monospace 12 for this session.", sizeof(ui.font_status));
      if (saved) pango_font_description_free(saved);
      g_free(value);
   }
   apply_font(font); pango_font_description_free(font);
   g_key_file_unref(settings); g_free(path);
}

static gboolean monospace_filter(const PangoFontFamily *family, const PangoFontFace *face, gpointer data)
{ (void)face; (void)data; return pango_font_family_is_monospace((PangoFontFamily *)family); }

static GtkWidget *wrapped_label(GtkWidget *box, const char *text)
{
   GtkWidget *label = gtk_label_new(text);
   gtk_label_set_xalign(GTK_LABEL(label), 0); gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
   return label;
}

static void show_setup(unsigned section)
{
   if (!ui.setup) return;
   ui.setup_visible = 1;
   gtk_widget_show(ui.setup); gtk_widget_show(ui.auxiliary);
   ui.syncing_logs++;
   gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), ui.setup));
   gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.setup_tabs), section);
   ui.syncing_logs--;
   if (ui.focused_log[0]) queue_panel(find_log(ui.focused_log, ui.focused_log_generation), THE_WORKBENCH_PANEL_RETURN);
}

static void setup_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; show_setup(GPOINTER_TO_UINT(data)); }
static void program_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; (void)data; the_tool_program_console_visibility(1, 0); show_tool_page(ui.console); }
static void focus_editor(void)
{
   if (ui.expanded)
   {
      ui.expanded = 0;
      gtk_widget_show(ui.editor);
      if (ui.expand_control) gtk_button_set_label(GTK_BUTTON(ui.expand_control), "Expand pane");
      ui.split_positioned = 0;
      gtk_widget_queue_resize(ui.split);
   }
   gtk_widget_grab_focus(ui.editor);
}
static void return_editor(GtkWidget *widget, gpointer data)
{ (void)widget; (void)data; focus_editor(); }
static void expand_auxiliary(GtkWidget *widget, gpointer data)
{
   (void)data;
   if (ui.expanded) focus_editor();
   else
   {
      ui.expanded = 1;
      gtk_widget_hide(ui.editor);
      gtk_button_set_label(GTK_BUTTON(widget), "Restore panes");
   }
}

static void settings_setup_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; gtk_dialog_response(GTK_DIALOG(data), 2); }
static void settings_profile_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; gtk_dialog_response(GTK_DIALOG(data), 3); }

static void show_settings(void)
{
   GtkWidget *dialog, *tabs, *appearance, *editor, *tools, *chooser, *number, *wrap, *prefix, *insert, *tab_size, *status;
   TheToolsFileSnapshot snapshot;
   the_tools_file_snapshot(&snapshot);
   int response, saved = 1, reset_font = 0;
   if (ui.settings) { gtk_window_present(GTK_WINDOW(ui.settings)); return; }
   dialog = gtk_dialog_new_with_buttons("Settings", GTK_WINDOW(ui.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      "_Reset font", 1, "_Close", GTK_RESPONSE_CLOSE, "_Apply", GTK_RESPONSE_APPLY, NULL);
   ui.settings = dialog;
   gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 520);
   tabs = gtk_notebook_new(); gtk_container_set_border_width(GTK_CONTAINER(tabs), 10);
   gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tabs, TRUE, TRUE, 0);
   appearance = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); gtk_container_set_border_width(GTK_CONTAINER(appearance), 10);
   wrapped_label(appearance, "Appearance · This device/app. The monospace font applies to editor cells, output readers and fixed-width fields. UI controls retain the desktop theme.");
   chooser = gtk_font_chooser_widget_new(); gtk_font_chooser_set_filter_func(GTK_FONT_CHOOSER(chooser), monospace_filter, NULL, NULL);
   gtk_font_chooser_set_level(GTK_FONT_CHOOSER(chooser), GTK_FONT_CHOOSER_LEVEL_FAMILY | GTK_FONT_CHOOSER_LEVEL_SIZE);
   gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(chooser), ui.font);
   gtk_font_chooser_set_preview_text(GTK_FONT_CHOOSER(chooser), "THE 0123456789  |  Source → output  |  Ω 界");
   gtk_box_pack_start(GTK_BOX(appearance), chooser, TRUE, TRUE, 0);
   status = wrapped_label(appearance, *ui.font_status ? ui.font_status : "Choose an installed monospace font, 6–48 points. Apply saves explicit changes.");
   gtk_notebook_append_page(GTK_NOTEBOOK(tabs), appearance, gtk_label_new("Appearance"));
   editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_container_set_border_width(GTK_CONTAINER(editor), 12);
   wrapped_label(editor, "Current file/view · Changes use editor commands. Startup defaults are profile-defined; exact provenance may be unknown after macros run.");
   number = gtk_check_button_new_with_label("Show line numbers in prefix"); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(number), snapshot.editor_settings.line_numbers);
   wrap = gtk_check_button_new_with_label("Word wrap"); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wrap), snapshot.editor_settings.wordwrap);
   gtk_box_pack_start(GTK_BOX(editor), number, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(editor), wrap, FALSE, FALSE, 0);
   prefix = gtk_check_button_new_with_label("Show prefix field"); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefix), snapshot.editor_settings.prefix);
   insert = gtk_check_button_new_with_label("Insert mode"); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(insert), snapshot.editor_settings.insert_mode);
   gtk_box_pack_start(GTK_BOX(editor), prefix, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(editor), insert, FALSE, FALSE, 0);
   wrapped_label(editor, snapshot.editor_settings.tab_increment ? "Tab increment (columns)" : "Tabs use custom stops. 0 keeps them; choose an increment to replace them.");
   tab_size = gtk_spin_button_new_with_range(0, 32, 1); gtk_spin_button_set_value(GTK_SPIN_BUTTON(tab_size), snapshot.editor_settings.tab_increment);
   gtk_box_pack_start(GTK_BOX(editor), tab_size, FALSE, FALSE, 0);
   wrapped_label(editor, "For future sessions, edit profile defaults in Tools & Profiles. Handwritten profile source is never rewritten by Settings.");
   button(editor, "Edit profile defaults…", G_CALLBACK(settings_profile_clicked), dialog);
   gtk_notebook_append_page(GTK_NOTEBOOK(tabs), editor, gtk_label_new("Editor"));
   tools = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_container_set_border_width(GTK_CONTAINER(tools), 12);
   wrapped_label(tools, "Profiles, file associations and native tool setup live in Tools & Profiles, separate from Program input and output.");
   gtk_notebook_append_page(GTK_NOTEBOOK(tabs), tools, gtk_label_new("Tools & Profiles"));
   { GtkWidget *jump = gtk_button_new_with_label("Open Tools & Profiles"); gtk_box_pack_start(GTK_BOX(tools), jump, FALSE, FALSE, 0); g_signal_connect(jump, "clicked", G_CALLBACK(settings_setup_clicked), dialog); }
   /* A distinct response is provided outside the tabs so keyboard users can
    * always reach setup without changing a current editor option. */
   gtk_dialog_add_button(GTK_DIALOG(dialog), "Tools & Profiles…", 2);
   gtk_widget_show_all(dialog);
   for (;;)
   {
      response = gtk_dialog_run(GTK_DIALOG(dialog));
      if (response == 1) { reset_font = 1; gtk_font_chooser_set_font(GTK_FONT_CHOOSER(chooser), "Monospace 12"); continue; }
      if (response != GTK_RESPONSE_APPLY) break;
      {
         PangoFontDescription *font = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(chooser));
         if (!font || !font_available(font))
         { gtk_label_set_text(GTK_LABEL(status), "Choose an installed monospace font from 6 to 48 points."); if (font) pango_font_description_free(font); continue; }
         if (reset_font || ui.font_unsaved || !pango_font_description_equal(font, ui.font)) { apply_font(font); saved = save_font_preferences(); }
         pango_font_description_free(font);
         if (CURRENT_VIEW && snapshot.editor_settings.line_numbers != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(number))) queue_command(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(number)) ? "set number on" : "set number off");
         if (CURRENT_VIEW && snapshot.editor_settings.wordwrap != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wrap))) queue_command(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wrap)) ? "set wordwrap on" : "set wordwrap off");
         if (CURRENT_VIEW && snapshot.editor_settings.prefix != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefix))) queue_command(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefix)) ? "set prefix on" : "set prefix off");
         if (CURRENT_VIEW && snapshot.editor_settings.insert_mode != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(insert))) queue_command(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(insert)) ? "set insertmode on" : "set insertmode off");
         if (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tab_size)) > 0 && snapshot.editor_settings.tab_increment != (unsigned)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tab_size)))
         { char command[64]; g_snprintf(command, sizeof(command), "set tabs incr %d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tab_size))); queue_command(command); }
      }
      break;
   }
   ui.settings = NULL; gtk_widget_destroy(dialog);
   if (response == 2) show_setup(0);
   if (response == 3) { show_setup(0); tool_action(NULL, "profile"); }
   if (!saved) message(ui.font_status);
}

static void show_about(void)
{
   const char *authors[] = {"Mark Hessling — original THE", "CRXTHE contributors — independent fork", NULL};
   char *details = g_strdup_printf("Independent C99 fork of THE 4.0.\nGTK %u.%u.%u · native workbench · %s\nCore editing and profiles retain editor command authority.\nDependencies retain their own licences; see installed share/doc/the.\nSee LICENSE-NOTICE.md for the retained curses-driver licence notice.", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(), the_tools_backend_available() ? "native tool bundle configured" : "native tool bundle unavailable");
   gtk_show_about_dialog(GTK_WINDOW(ui.window), "program-name", "CRXTHE", "version", the_version ? (const char *)the_version : "0.10.dev", "comments", details,
      "authors", authors, "license-type", GTK_LICENSE_GPL_2_0, "license", "CRXTHE is licensed under GNU GPL version 2 or later. Original THE attribution is retained. Dependencies retain their own licence terms.", "wrap-license", TRUE,
      "website", "https://github.com/adesutherland/THE", "website-label", "CRXTHE source and issue tracker", NULL);
   g_free(details);
}

static void local_action(GSimpleAction *action, GVariant *parameter, gpointer data)
{
   const char *name = g_action_get_name(G_ACTION(action));
   (void)parameter; (void)data;
   ui.menu_canceled = 0;
   if (!strcmp(name, "preferences") || !strcmp(name, "gui.settings")) show_settings();
   else if (!strcmp(name, "about") || !strcmp(name, "gui.about")) show_about();
   else if (!strcmp(name, "tools-profiles") || !strcmp(name, "gui.tools-profiles")) show_setup(0);
   else if (!strcmp(name, "program") || !strcmp(name, "gui.program")) program_clicked(NULL, NULL);
   else if (!strcmp(name, "gui.files")) file_action(NULL, "file.open");
   else if (!strcmp(name, "editor")) focus_editor();
   else if (!strcmp(name, "profile-defaults")) { show_setup(0); tool_action(NULL, "profile"); }
}

static gboolean menu_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data)
{ (void)widget; (void)event; (void)data; g_set_object(&ui.menu_button_origin, gtk_window_get_focus(GTK_WINDOW(ui.window))); return FALSE; }
static void menu_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; (void)data; open_native_menu(ui.menu_button_origin ? ui.menu_button_origin : ui.editor, NULL); g_clear_object(&ui.menu_button_origin); }

static void register_tool_clicked(GtkWidget *widget, gpointer data)
{
   const char *arguments[5];
   TheInputEvent event;
   (void)widget; (void)data;
   arguments[0] = gtk_entry_get_text(GTK_ENTRY(ui.tool_fields[0]));
   arguments[1] = gtk_combo_box_get_active_id(GTK_COMBO_BOX(ui.tool_role));
   arguments[2] = gtk_entry_get_text(GTK_ENTRY(ui.tool_fields[1]));
   arguments[3] = gtk_entry_get_text(GTK_ENTRY(ui.tool_fields[2]));
   arguments[4] = gtk_entry_get_text(GTK_ENTRY(ui.tool_fields[3]));
   if (the_panel_active || in_macro) return;
   if (!arguments[1] || !the_tools_prepare_input("register", "", "", 5, arguments, &event) || !queue_event(event))
      message("Tool registration could not be queued. Check the ID, role and compiled image path.");
}

static void runner_clicked(GtkWidget *widget, gpointer data)
{
   const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(ui.runner));
   TheInputEvent event;
   (void)widget; (void)data;
   if (!id || the_panel_active || in_macro) return;
   if (the_tools_prepare_lines_bound("run", id, "", gtk_entry_get_text(GTK_ENTRY(ui.runner_arguments)),
       ui.file_snapshot.source_id, ui.file_snapshot.revision, "", &event) && queue_event(event)) show_tool_page(ui.console);
}

static void setup_switched(GtkNotebook *notebook, GtkWidget *page, guint number, gpointer data)
{
   (void)notebook; (void)page; (void)data;
   if (number == 2 && ui.build_reader.scroller && reader_at_tail(&ui.build_reader)
   && !gtk_text_buffer_get_has_selection(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.build_output))))
   { ui.build_reader.unread = 0; reader_unread(&ui.build_reader); }
}

static void create_setup(void)
{
   GtkWidget *controls, *profiles, *types, *scroller, *expander, *form, *grid, *runner_row;
   static const char *captions[] = {"ID", "Compiled image", "Extensions", "Label"};
   static const char *columns[] = {"File types", "Tool", "Role", "Availability", "Ownership / details"};
   GtkListStore *store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
   unsigned i;
   ui.setup = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
   gtk_container_set_border_width(GTK_CONTAINER(ui.setup), 6);
   gtk_widget_set_name(ui.setup, "the-tools-profiles");
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), ui.setup, gtk_label_new("Tools & Profiles"));
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   gtk_box_pack_start(GTK_BOX(ui.setup), controls, FALSE, FALSE, 0);
   { GtkWidget *label = gtk_label_new("Tools & Profiles"); gtk_label_set_xalign(GTK_LABEL(label), 0); gtk_box_pack_start(GTK_BOX(controls), label, TRUE, TRUE, 0); }
   ui.expand_control = button(controls, "Expand pane", G_CALLBACK(expand_auxiliary), NULL);
   button(controls, "Editor", G_CALLBACK(return_editor), NULL);
   button(controls, "Hide", G_CALLBACK(hide_tool_page), ui.setup);
   ui.setup_tabs = gtk_notebook_new(); gtk_notebook_set_scrollable(GTK_NOTEBOOK(ui.setup_tabs), TRUE);
   gtk_box_pack_start(GTK_BOX(ui.setup), ui.setup_tabs, TRUE, TRUE, 0);
   g_signal_connect(ui.setup_tabs, "switch-page", G_CALLBACK(setup_switched), NULL);
   profiles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_container_set_border_width(GTK_CONTAINER(profiles), 10);
   scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_add(GTK_CONTAINER(scroller), profiles);
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.setup_tabs), scroller, gtk_label_new("Profiles"));
   ui.profile_summary = wrapped_label(profiles, "Profile information is available after startup.");
   gtk_label_set_selectable(GTK_LABEL(ui.profile_summary), TRUE);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_box_pack_start(GTK_BOX(profiles), controls, FALSE, FALSE, 0);
   ui.edit_profile = button(controls, "Edit profile defaults", G_CALLBACK(tool_action), "profile");
   ui.apply_image = button(controls, "Import compiled profile…", G_CALLBACK(apply_image_clicked), NULL);
   wrapped_label(profiles, "Edit source in the normal editor, then use its Build profile and Apply profile actions. A compiled-only import has no editable source. The editor response reports the immediate result; Build results holds compiler output.");
   types = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); gtk_container_set_border_width(GTK_CONTAINER(types), 8);
   scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_add(GTK_CONTAINER(scroller), types);
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.setup_tabs), scroller, gtk_label_new("File types & tools"));
   wrapped_label(types, "Installed associations and permitted runners. Profiles and registered tools remain the source of workbench behavior.");
   scroller = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(scroller, -1, 100);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(types), scroller, TRUE, TRUE, 0);
   ui.tools_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)); g_object_unref(store);
   for (i = 0; i < G_N_ELEMENTS(columns); i++)
   {
      GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
      GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(columns[i], renderer, "text", i, NULL);
      gtk_tree_view_column_set_resizable(column, TRUE); gtk_tree_view_append_column(GTK_TREE_VIEW(ui.tools_view), column);
   }
   gtk_container_add(GTK_CONTAINER(scroller), ui.tools_view);
   expander = gtk_expander_new("Run with a chosen runner / arguments"); gtk_box_pack_start(GTK_BOX(types), expander, FALSE, FALSE, 0);
   form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_container_add(GTK_CONTAINER(expander), form);
   runner_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_box_pack_start(GTK_BOX(form), runner_row, FALSE, FALSE, 0);
   ui.runner = gtk_combo_box_text_new(); gtk_box_pack_start(GTK_BOX(runner_row), ui.runner, TRUE, TRUE, 0);
   button(runner_row, "Run current file", G_CALLBACK(runner_clicked), NULL);
   ui.runner_arguments = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(ui.runner_arguments), "One argument (use RX RUN for multiple arguments)");
   gtk_box_pack_start(GTK_BOX(form), ui.runner_arguments, FALSE, FALSE, 0);
   expander = gtk_expander_new("Register a compiled tool"); gtk_box_pack_start(GTK_BOX(types), expander, FALSE, FALSE, 0);
   form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_container_add(GTK_CONTAINER(expander), form);
   grid = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(grid), 6); gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
   gtk_box_pack_start(GTK_BOX(form), grid, FALSE, FALSE, 0);
   for (i = 0; i < G_N_ELEMENTS(captions); i++)
   {
      GtkWidget *label = gtk_label_new(captions[i]); gtk_label_set_xalign(GTK_LABEL(label), 0);
      gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
      ui.tool_fields[i] = gtk_entry_new(); gtk_widget_set_hexpand(ui.tool_fields[i], TRUE);
      gtk_grid_attach(GTK_GRID(grid), ui.tool_fields[i], 1, i, 1, 1);
   }
   ui.tool_role = gtk_combo_box_text_new();
   gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui.tool_role), "runner", "Source runner");
   gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui.tool_role), "program", "Program");
   gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui.tool_role), "profile", "Profile");
   gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui.tool_role), "language", "Language / highlighter");
   gtk_combo_box_set_active(GTK_COMBO_BOX(ui.tool_role), 0);
   gtk_box_pack_start(GTK_BOX(form), ui.tool_role, FALSE, FALSE, 0);
   button(form, "Register tool", G_CALLBACK(register_tool_clicked), NULL);
   wrapped_label(form, "Registration uses the native tool API and retains its validation and persistence rules. Edit profile defaults to make profile-owned definitions.");
}

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char name[32];
   unsigned long generation;
   GMenu *section; /* borrowed from the current menu model */
   int index;
   char label[THE_WORKBENCH_LABEL_MAX + 1], reason[256];
} GtkMenuAction;

static const char *menu_action_reason(const TheWorkbenchAction *action)
{
   if (action && action->kind == THE_WORKBENCH_ACTION_COMMAND)
   {
      if (!strcmp(action->id, "profile.action.8") && !strcmp(action->operand, "rx menu-run"))
         return ui.file_snapshot.actions[1].enabled ? "ok" : *ui.file_snapshot.actions[1].reason ? ui.file_snapshot.actions[1].reason : "No runner is associated with this file";
      if (!strcmp(action->id, "profile.action.11") && !strcmp(action->operand, "rx stop"))
         return the_tool_console()->running ? "ok" : "No task is running";
      if (!strcmp(action->id, "profile.action.12") && !strcmp(action->operand, "rx clear"))
         return the_tool_program_console()->bytes ? "ok" : "Program output is empty";
      if (!strcmp(action->id, "profile.action.13") && !strcmp(action->operand, "rx eof"))
         return the_tool_program_console()->running && !the_tool_program_console()->eof ? "ok" : "No open Program input";
   }
   if (action && action->kind == THE_WORKBENCH_ACTION_BUILTIN && action->builtin == THE_FRONTEND_ACTION_EDIT_UNDO)
   {
      GtkWidget *focus = ui.menu_focus ? ui.menu_focus : ui.window ? gtk_window_get_focus(GTK_WINDOW(ui.window)) : NULL;
      if (GTK_IS_ENTRY(focus) || GTK_IS_TEXT_VIEW(focus)) return "Undo is unavailable in this native field";
   }
   return the_workbench_action_reason(action);
}

static int menu_action_enabled(const TheWorkbenchAction *action)
{
   const char *reason = menu_action_reason(action);
   return !strcmp(reason, "ok") || !strcmp(reason, "argument-required");
}

static void menu_action(GSimpleAction *action, GVariant *parameter, gpointer data)
{
   GtkMenuAction *entry = data;
   const TheWorkbenchAction *found = the_workbench_action_find(entry->id);
   TheInputEvent event;
   char *path = NULL;
   (void)action; (void)parameter;
   if (the_panel_active || in_macro) return;
   if (found && found->generation == entry->generation && found->kind == THE_WORKBENCH_ACTION_BUILTIN)
   {
      GtkWidget *origin = ui.menu_focus ? ui.menu_focus : gtk_window_get_focus(GTK_WINDOW(ui.window));
      if (found->builtin == THE_FRONTEND_ACTION_EDIT_UNDO && (GTK_IS_ENTRY(origin) || GTK_IS_TEXT_VIEW(origin))) return;
      const char *name = the_frontend_action_name(found->builtin);
      if (name && (!strcmp(name, "edit.copy") || !strcmp(name, "edit.cut") || !strcmp(name, "edit.paste") || !strcmp(name, "edit.select-all")))
      {
         GtkWidget *focus = ui.menu_focus ? ui.menu_focus : gtk_window_get_focus(GTK_WINDOW(ui.window));
         if (clipboard_action(name, focus)) { ui.menu_canceled = 0; gtk_widget_grab_focus(focus ? focus : ui.editor); }
         return;
      }
   }
   if (found && found->generation == entry->generation && !strcmp(found->id, "profile.action.8")
   && found->kind == THE_WORKBENCH_ACTION_COMMAND && !strcmp(found->operand, "rx menu-run"))
   { ui.menu_canceled = 0; current_file_action(NULL, GUINT_TO_POINTER(1)); return; }
   /* The chooser supplies data to the typed action. Core invocation still
    * revalidates the copied generation, path and active frontend policy. */
   if (found && found->generation == entry->generation
   && found->kind == THE_WORKBENCH_ACTION_BUILTIN && !found->operand[0]
   && (found->builtin == THE_FRONTEND_ACTION_FILE_OPEN
       || found->builtin == THE_FRONTEND_ACTION_FILE_CREATE
       || found->builtin == THE_FRONTEND_ACTION_BUFFER_SWITCH))
   {
      int creating = found->builtin == THE_FRONTEND_ACTION_FILE_CREATE;
      GtkWidget *dialog = gtk_file_chooser_dialog_new(
         creating ? "New file" : "Open file", GTK_WINDOW(ui.window),
         creating ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
         "_Cancel", GTK_RESPONSE_CANCEL, creating ? "_Create" : "_Open",
         GTK_RESPONSE_ACCEPT, NULL);
      if (CURRENT_FILE)
         gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), (char *)CURRENT_FILE->fpath);
      if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
         path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      gtk_widget_destroy(dialog);
      if (!path) return;
   }
   if (the_input_event_from_workbench_invoke(entry->id, entry->generation, path, &event)
   && queue_event(event))
   {
      ui.menu_canceled = 0;
      if (!found || found->kind != THE_WORKBENCH_ACTION_PANEL)
         focus_editor();
   }
   g_free(path);
}

/* GMenu labels use underscores for mnemonics. Escape them so a published
 * label remains literal data, including slash, quotes and Unicode. */
static char *menu_label(const char *label)
{
   GString *escaped = g_string_new(NULL);
   for (; *label; label++)
   {
      if (*label == '_') g_string_append_c(escaped, '_');
      g_string_append_c(escaped, *label);
   }
   return g_string_free(escaped, FALSE);
}

static const char *menu_group(const TheWorkbenchMenu *menu)
{
   static const char *groups[] = {"file", "edit", "view", "tools", "help"};
   size_t i;
   for (i = 0; i < G_N_ELEMENTS(groups); i++)
      if ((!strncmp(menu->id, "the.", 4) && !strcmp(menu->id + 4, groups[i]))
      || (!strncmp(menu->id, "profile.", 8) && !strcmp(menu->id + 8, groups[i]))) return groups[i];
   return menu->id;
}

static void sync_menus(void)
{
   size_t menus[THE_WORKBENCH_MENU_MAX], items[THE_WORKBENCH_ITEM_MAX];
   size_t nm, ni, i, j, k, n;
   if (!ui.application || !ui.menu_model) return;
   if (ui.menu_revision != the_workbench_revision())
   {
      if (ui.native_menu) gtk_menu_shell_deactivate(GTK_MENU_SHELL(ui.native_menu));
      for (i = 0; i < ui.menu_actions->len; i++)
      {
         GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
         g_action_map_remove_action(G_ACTION_MAP(ui.application), entry->name);
      }
      g_ptr_array_set_size(ui.menu_actions, 0);
      g_menu_remove_all(ui.menu_model);
      nm = the_workbench_menu_count(); ni = the_workbench_item_count();
      for (i = 0; i < nm; i++)
      {
         menus[i] = i;
         for (j = i; j && the_workbench_menu_at(menus[j - 1])->order >
                             the_workbench_menu_at(menus[j])->order; j--)
         { k = menus[j]; menus[j] = menus[j - 1]; menus[j - 1] = k; }
      }
      for (i = 0; i < nm; i++)
      {
         const TheWorkbenchMenu *menu = the_workbench_menu_at(menus[i]);
         const char *group = menu_group(menu);
         GMenu *submenu, *section;
         char *label;
         for (j = 0; j < i; j++) if (!strcmp(group, menu_group(the_workbench_menu_at(menus[j])))) break;
         if (j < i) continue;
         submenu = g_menu_new(); section = g_menu_new(); label = menu_label(menu->label);
         n = 0;
         for (j = 0; j < ni; j++)
         {
            const TheWorkbenchItem *candidate = the_workbench_item_at(j);
            const TheWorkbenchMenu *source_menu = NULL;
            size_t source;
            for (source = 0; source < nm; source++) if (!strcmp(the_workbench_menu_at(source)->id, candidate->menu)) { source_menu = the_workbench_menu_at(source); break; }
            if (source_menu && !strcmp(menu_group(source_menu), group))
            {
               items[n++] = j;
               for (k = n - 1; k && the_workbench_item_at(items[k - 1])->order >
                                     the_workbench_item_at(items[k])->order; k--)
               { size_t swap = items[k]; items[k] = items[k - 1]; items[k - 1] = swap; }
            }
         }
         for (j = 0; j < n; j++)
         {
            const TheWorkbenchItem *item = the_workbench_item_at(items[j]);
            const TheWorkbenchAction *action = the_workbench_action_find(item->action);
            if (item->separator)
            {
               if (g_menu_model_get_n_items(G_MENU_MODEL(section)))
                  g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
               g_object_unref(section); section = g_menu_new();
            }
            else if (action)
            {
               GtkMenuAction *entry = g_new0(GtkMenuAction, 1);
               GSimpleAction *native;
               char *detailed, *item_label = menu_label(item->label);
               strcpy(entry->id, action->id); entry->generation = action->generation;
               strcpy(entry->label, item->label); entry->section = section;
               entry->index = g_menu_model_get_n_items(G_MENU_MODEL(section));
               /* A delayed native-menu event must never resolve a recycled
                * action name to a different published definition. */
               g_snprintf(entry->name, sizeof(entry->name), "wb-%lu-%u",
                          entry->generation, ui.menu_actions->len);
               native = g_simple_action_new(entry->name, NULL);
               g_object_set_data_full(G_OBJECT(native), "the-workbench-entry", entry, g_free);
               g_signal_connect(native, "activate", G_CALLBACK(menu_action), entry);
               g_simple_action_set_enabled(native, menu_action_enabled(action));
               g_action_map_add_action(G_ACTION_MAP(ui.application), G_ACTION(native));
               g_ptr_array_add(ui.menu_actions, entry);
               detailed = g_strconcat("app.", entry->name, NULL);
               g_menu_append(section, item_label, detailed);
               g_free(detailed); g_free(item_label); g_object_unref(native);
            }
         }
         if (!strcmp(group, "view"))
         {
            g_menu_append(section, "Files…", "app.gui.files");
            g_menu_append(section, "Program console", "app.gui.program");
            g_menu_append(section, "Tools & Profiles", "app.gui.tools-profiles");
         }
         if (!strcmp(group, "tools"))
         {
            g_menu_append(section, "Tools & Profiles…", "app.gui.tools-profiles");
#ifndef GDK_WINDOWING_QUARTZ
            g_menu_append(section, "Settings…", "app.preferences");
#endif
         }
#ifndef GDK_WINDOWING_QUARTZ
         if (!strcmp(group, "help"))
            g_menu_append(section, "About CRXTHE", "app.about");
#endif
         if (g_menu_model_get_n_items(G_MENU_MODEL(section)))
            g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
         g_menu_append_submenu(ui.menu_model, label, G_MENU_MODEL(submenu));
         g_free(label); g_object_unref(section); g_object_unref(submenu);
      }
      ui.menu_revision = the_workbench_revision();
   }
   for (i = 0; i < ui.menu_actions->len; i++)
   {
      GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
      GAction *action = g_action_map_lookup_action(G_ACTION_MAP(ui.application), entry->name);
      const TheWorkbenchAction *definition = the_workbench_action_find(entry->id);
      const char *reason = menu_action_reason(definition);
      int enabled = menu_action_enabled(definition);
      g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
      if (strcmp(entry->reason, reason))
      {
         char *label = menu_label(entry->label);
         char *display = enabled ? g_strdup(label) : g_strdup_printf("%s (%s)", label, reason);
         char *detailed = g_strconcat("app.", entry->name, NULL);
         g_menu_remove(entry->section, entry->index);
         g_menu_insert(entry->section, entry->index, display, detailed);
         g_strlcpy(entry->reason, reason, sizeof(entry->reason));
         g_free(label); g_free(display); g_free(detailed);
      }
   }
}

static void native_menu_deactivated(GtkWidget *widget, gpointer data)
{
   (void)data;
   if (ui.native_menu == widget)
   {
      ui.native_menu = NULL;
      finish_menu_focus();
      gtk_widget_destroy(widget);
   }
}

static void open_native_menu(GtkWidget *focus, GdkEventKey *trigger)
{
   sync_menus();
   if (ui.native_menu) return;
   remember_menu_focus(focus);
   ui.menu_canceled = 1;
#ifdef GDK_WINDOWING_QUARTZ
   {
      GMenu *popup = g_menu_new(), *application = g_menu_new();
      g_menu_append_section(popup, NULL, G_MENU_MODEL(ui.menu_model));
      g_menu_append(application, "Settings…", "app.preferences"); g_menu_append(application, "About CRXTHE", "app.about");
      g_menu_append_submenu(popup, "Application", G_MENU_MODEL(application));
      ui.native_menu = gtk_menu_new_from_model(G_MENU_MODEL(popup));
      g_object_unref(application); g_object_unref(popup);
   }
#else
   ui.native_menu = gtk_menu_new_from_model(G_MENU_MODEL(ui.menu_model));
#endif
   gtk_menu_attach_to_widget(GTK_MENU(ui.native_menu), ui.window, NULL);
   g_signal_connect(ui.native_menu, "deactivate", G_CALLBACK(native_menu_deactivated), NULL);
   gtk_widget_show_all(ui.native_menu);
   gtk_menu_popup_at_widget(GTK_MENU(ui.native_menu), ui.editor,
                           GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent *)trigger);
   gtk_menu_shell_select_first(GTK_MENU_SHELL(ui.native_menu), TRUE);
}

static void application_activated(GApplication *application, gpointer data)
{
   (void)application; (void)data;
   if (ui.window)
      gtk_window_present(GTK_WINDOW(ui.window));
}

static int activate(void)
{
   memset(&ui, 0, sizeof(ui));
   g_queue_init(&ui.inputs);
   ui.timeout = -1;
   ui.cursor_visible = ui.blink_visible = 1;
   ui.program_reader.version = ui.build_reader.version = (unsigned)-1;
   headless_driver_reset();
   return 1;
}

static int startup(const TheDriverStartupOptions *options, char *error, size_t capacity)
{
   GtkWidget *root, *toolbar, *scroller, *heading, *controls, *entry, *actions;
   GError *application_error = NULL;
   (void)options;
   if (!gtk_init_check(NULL, NULL))
   {
      g_snprintf(error, capacity, "GTK 3 could not open a display");
      return 0;
   }
   /* GMenu/GAction lets GtkApplication place the same menu in the macOS
    * system menu bar and provide an in-window fallback on other desktops.
    * NON_UNIQUE preserves the editor's existing process and file ownership. */
   g_set_application_name("CRXTHE");
   ui.application = gtk_application_new("org.crxthe.Editor", G_APPLICATION_NON_UNIQUE);
   g_signal_connect(ui.application, "activate", G_CALLBACK(application_activated), NULL);
#ifdef GDK_WINDOWING_QUARTZ
   /* Quartz uses the session delegate to route Dock/system Quit through
    * app.quit. It must reach CANCEL's unsaved-buffer checks, like the menu. */
   g_object_set(ui.application, "register-session", TRUE, NULL);
#endif
   if (!g_application_register(G_APPLICATION(ui.application), NULL, &application_error))
   {
      g_snprintf(error, capacity, "GTK application registration failed: %s",
                    application_error->message);
      g_clear_error(&application_error);
      g_clear_object(&ui.application);
      return 0;
   }
   the_tools_native_initialise();
   ui.window = gtk_application_window_new(ui.application);
   gtk_window_set_title(GTK_WINDOW(ui.window), "CRXTHE — GTK");
   gtk_window_set_default_size(GTK_WINDOW(ui.window), 1120, 820);
   g_signal_connect(ui.window, "delete-event", G_CALLBACK(close_window), NULL);
   g_signal_connect(ui.window, "key-press-event", G_CALLBACK(workbench_key_pressed), NULL);
   g_signal_connect(ui.window, "set-focus", G_CALLBACK(remember_nonlog_focus), NULL);
   root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(ui.window), root);
   {
      /* Quartz Dock/system Quit is an application lifecycle action. */
      GSimpleAction *quit = g_simple_action_new("quit", NULL);
      g_signal_connect_swapped(quit, "activate", G_CALLBACK(queue_command), "cancel");
      g_action_map_add_action(G_ACTION_MAP(ui.application), G_ACTION(quit));
      g_object_unref(quit);
   }
   {
      static const char *names[] = {"about", "preferences", "gui.settings", "gui.about", "gui.tools-profiles", "gui.program", "gui.files", "editor", "profile-defaults"};
      size_t i;
      for (i = 0; i < G_N_ELEMENTS(names); i++)
      {
         GSimpleAction *action = g_simple_action_new(names[i], NULL);
         g_signal_connect(action, "activate", G_CALLBACK(local_action), NULL);
         g_action_map_add_action(G_ACTION_MAP(ui.application), G_ACTION(action)); g_object_unref(action);
      }
#ifdef GDK_WINDOWING_QUARTZ
      { const char *keys[] = {"<Meta>comma", NULL}; gtk_application_set_accels_for_action(ui.application, "app.preferences", keys); }
#endif
   }
   ui.menu_model = g_menu_new();
   ui.menu_actions = g_ptr_array_new();
   sync_menus();
   gtk_application_set_menubar(ui.application, G_MENU_MODEL(ui.menu_model));
   toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_container_set_border_width(GTK_CONTAINER(toolbar), 6);
   gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);
   ui.first_control = button(toolbar, "Menu", G_CALLBACK(menu_clicked), NULL);
   g_signal_connect(ui.first_control, "button-press-event", G_CALLBACK(menu_button_pressed), NULL);
   button(toolbar, "Open", G_CALLBACK(file_action), "file.open");
   gtk_widget_set_tooltip_text(ui.first_control, "Open file. Tab visits controls; Esc returns to the editor.");
   button(toolbar, "Save", G_CALLBACK(file_action), "file.save");
   button(toolbar, "Undo", G_CALLBACK(file_action), "edit.undo");
   ui.file_title = gtk_label_new("");
   gtk_label_set_ellipsize(GTK_LABEL(ui.file_title), PANGO_ELLIPSIZE_MIDDLE);
   gtk_label_set_xalign(GTK_LABEL(ui.file_title), 0);
   gtk_box_pack_start(GTK_BOX(toolbar), ui.file_title, TRUE, TRUE, 6);
   actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   ui.task_row = actions;
   gtk_container_set_border_width(GTK_CONTAINER(actions), 6);
   gtk_box_pack_start(GTK_BOX(root), actions, FALSE, FALSE, 0);
   ui.build = button(actions, "Build source", G_CALLBACK(current_file_action), GUINT_TO_POINTER(0));
   ui.run = button(actions, "Run", G_CALLBACK(current_file_action), GUINT_TO_POINTER(1));
   ui.apply = button(actions, "Apply profile", G_CALLBACK(current_file_action), GUINT_TO_POINTER(2));
   gtk_widget_set_name(ui.build, "the-file-build");
   gtk_widget_set_name(ui.run, "the-program-run");
   gtk_widget_set_name(ui.apply, "the-file-apply");
   ui.task_state = wrapped_label(actions, "");
   gtk_label_set_ellipsize(GTK_LABEL(ui.task_state), PANGO_ELLIPSIZE_END);
   gtk_label_set_line_wrap(GTK_LABEL(ui.task_state), FALSE);
   ui.split = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
   gtk_widget_set_name(ui.split, "the-editor-console-split");
   g_signal_connect(ui.split, "size-allocate", G_CALLBACK(split_allocated), NULL);
   gtk_box_pack_start(GTK_BOX(root), ui.split, TRUE, TRUE, 0);
   ui.editor = gtk_drawing_area_new();
   gtk_widget_set_can_focus(ui.editor, TRUE);
   gtk_widget_set_tooltip_text(ui.editor, "Shift-Tab: console and workbench controls. F6: command/file. F10: menu. Esc: command line.");
   gtk_widget_set_hexpand(ui.editor, TRUE);
   gtk_widget_set_vexpand(ui.editor, TRUE);
   gtk_widget_set_size_request(ui.editor, 200, 120);
   gtk_widget_add_events(ui.editor, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
      | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK
      | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_FOCUS_CHANGE_MASK);
   gtk_paned_pack1(GTK_PANED(ui.split), ui.editor, TRUE, FALSE);
   ui.im = gtk_im_multicontext_new();
   g_signal_connect(ui.im, "commit", G_CALLBACK(im_commit), NULL);
   restore_font_preferences();
   g_signal_connect(ui.editor, "style-updated", G_CALLBACK(font_style_changed), NULL);
   g_signal_connect(ui.editor, "draw", G_CALLBACK(draw_editor), NULL);
   g_signal_connect(ui.editor, "size-allocate", G_CALLBACK(allocated), NULL);
   g_signal_connect(ui.editor, "key-press-event", G_CALLBACK(key_pressed), NULL);
   g_signal_connect(ui.editor, "key-release-event", G_CALLBACK(key_released), NULL);
   g_signal_connect(ui.editor, "button-press-event", G_CALLBACK(clicked), NULL);
   g_signal_connect(ui.editor, "motion-notify-event", G_CALLBACK(pointer_moved), NULL);
   g_signal_connect(ui.editor, "button-release-event", G_CALLBACK(pointer_released), NULL);
   g_signal_connect(ui.editor, "scroll-event", G_CALLBACK(scrolled), NULL);
   g_signal_connect(ui.editor, "realize", G_CALLBACK(editor_realized), NULL);
   g_signal_connect(ui.editor, "focus-in-event", G_CALLBACK(editor_focus), NULL);
   g_signal_connect(ui.editor, "focus-out-event", G_CALLBACK(editor_focus), NULL);
   ui.auxiliary = gtk_notebook_new();
   ui.logs = g_ptr_array_new_with_free_func(free_log_pane);
   gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ui.auxiliary), TRUE);
   gtk_notebook_set_show_border(GTK_NOTEBOOK(ui.auxiliary), FALSE);
   gtk_notebook_set_scrollable(GTK_NOTEBOOK(ui.auxiliary), TRUE);
   g_signal_connect(ui.auxiliary, "switch-page", G_CALLBACK(auxiliary_switched), NULL);
   gtk_paned_pack2(GTK_PANED(ui.split), ui.auxiliary, FALSE, FALSE);
   ui.console = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   gtk_widget_set_name(ui.console, "the-program-console");
   gtk_container_set_border_width(GTK_CONTAINER(ui.console), 6);
   ui.program_reader.page = ui.console;
   ui.program_reader.tab = gtk_label_new("Program");
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), ui.console, ui.program_reader.tab);
   heading = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   ui.header = heading;
   gtk_style_context_add_class(gtk_widget_get_style_context(heading), "the-console-header");
   gtk_box_pack_start(GTK_BOX(ui.console), heading, FALSE, FALSE, 0);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_box_pack_start(GTK_BOX(heading), controls, FALSE, FALSE, 0);
   {
      GtkWidget *label = gtk_label_new("Program console");
      gtk_label_set_xalign(GTK_LABEL(label), 0);
      gtk_box_pack_start(GTK_BOX(controls), label, TRUE, TRUE, 0);
   }
   ui.state = gtk_label_new("Ready");
   gtk_widget_set_name(ui.state, "the-console-status");
   gtk_label_set_ellipsize(GTK_LABEL(ui.state), PANGO_ELLIPSIZE_END);
   gtk_label_set_xalign(GTK_LABEL(ui.state), 0);
   gtk_box_pack_start(GTK_BOX(heading), ui.state, FALSE, FALSE, 0);
   ui.stop = button(controls, "Stop", G_CALLBACK(tool_action), "stop");
   ui.eof = button(controls, "EOF", G_CALLBACK(tool_action), "eof");
   button(controls, "Clear", G_CALLBACK(tool_action), "clear");
   ui.program_reader.latest = button(controls, "Latest", G_CALLBACK(reader_latest), &ui.program_reader);
   gtk_widget_set_tooltip_text(ui.program_reader.latest, "Show the latest output and resume following it. New output leaves a scrolled reader or selection in place.");
   button(controls, "Hide", G_CALLBACK(hide_tool_page), ui.console);
   scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_set_size_request(scroller, -1, 70);
   gtk_box_pack_start(GTK_BOX(ui.console), scroller, TRUE, TRUE, 0);
   ui.output = gtk_text_view_new();
   gtk_widget_set_name(ui.output, "the-console-output");
   gtk_text_view_set_editable(GTK_TEXT_VIEW(ui.output), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(ui.output), FALSE);
   gtk_text_view_set_monospace(GTK_TEXT_VIEW(ui.output), TRUE);
   gtk_container_add(GTK_CONTAINER(scroller), ui.output);
   ui.program_reader.view = ui.output; ui.program_reader.scroller = scroller;
   bind_reader_input(&ui.program_reader);
   g_signal_connect(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)), "value-changed", G_CALLBACK(reader_scrolled), &ui.program_reader);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output)), "stderr", NULL);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output)), "stdout", NULL);
   entry = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_widget_set_name(entry, "the-console-input-row");
   gtk_box_pack_start(GTK_BOX(ui.console), entry, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(entry), gtk_label_new("Program input"), FALSE, FALSE, 0);
   ui.input = gtk_entry_new();
   gtk_widget_set_name(ui.input, "the-console-input");
   gtk_widget_set_tooltip_text(ui.input, "Program stdin. Enter sends; F6 returns to command/file; F10 opens the menu; Esc returns to the editor.");
   gtk_entry_set_max_length(GTK_ENTRY(ui.input), 4096);
   gtk_box_pack_start(GTK_BOX(entry), ui.input, TRUE, TRUE, 0);
   g_signal_connect(ui.input, "activate", G_CALLBACK(send_input), NULL);
   g_signal_connect(ui.input, "insert-text", G_CALLBACK(program_input_insert), NULL);
   g_signal_connect(ui.input, "paste-clipboard", G_CALLBACK(program_input_paste), NULL);
   ui.send = button(entry, "Send", G_CALLBACK(send_input), NULL);
   ui.build_result = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   gtk_widget_set_name(ui.build_result, "the-build-result");
   gtk_container_set_border_width(GTK_CONTAINER(ui.build_result), 6);
   ui.build_reader.page = ui.build_result;
   ui.build_reader.tab = gtk_label_new("Build result");
   create_setup();
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.setup_tabs), ui.build_result, ui.build_reader.tab);
   heading = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   gtk_style_context_add_class(gtk_widget_get_style_context(heading), "the-console-header");
   gtk_box_pack_start(GTK_BOX(ui.build_result), heading, FALSE, FALSE, 0);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_box_pack_start(GTK_BOX(heading), controls, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(controls), gtk_label_new("Build result"), TRUE, TRUE, 0);
   ui.build_stop = button(controls, "Stop", G_CALLBACK(tool_action), "stop");
   ui.build_reader.latest = button(controls, "Latest", G_CALLBACK(reader_latest), &ui.build_reader);
   gtk_widget_set_tooltip_text(ui.build_reader.latest, "Show the latest build output and resume following it.");
   button(controls, "Hide", G_CALLBACK(hide_tool_page), ui.build_result);
   ui.build_state = gtk_label_new("Build the current source, then Run or Apply profile.");
   gtk_widget_set_name(ui.build_state, "the-build-status");
   gtk_label_set_xalign(GTK_LABEL(ui.build_state), 0);
   gtk_label_set_line_wrap(GTK_LABEL(ui.build_state), TRUE);
   gtk_box_pack_start(GTK_BOX(heading), ui.build_state, FALSE, FALSE, 0);
   ui.build_profile = gtk_label_new("");
   gtk_widget_set_name(ui.build_profile, "the-build-profile");
   gtk_label_set_xalign(GTK_LABEL(ui.build_profile), 0);
   gtk_label_set_line_wrap(GTK_LABEL(ui.build_profile), TRUE);
   gtk_box_pack_start(GTK_BOX(heading), ui.build_profile, FALSE, FALSE, 0);
   scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_set_size_request(scroller, -1, 70);
   gtk_box_pack_start(GTK_BOX(ui.build_result), scroller, TRUE, TRUE, 0);
   ui.build_output = gtk_text_view_new();
   gtk_widget_set_name(ui.build_output, "the-build-output");
   gtk_text_view_set_editable(GTK_TEXT_VIEW(ui.build_output), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(ui.build_output), FALSE);
   gtk_text_view_set_monospace(GTK_TEXT_VIEW(ui.build_output), TRUE);
   gtk_container_add(GTK_CONTAINER(scroller), ui.build_output);
   ui.build_reader.view = ui.build_output; ui.build_reader.scroller = scroller;
   bind_reader_input(&ui.build_reader);
   g_signal_connect(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)), "value-changed", G_CALLBACK(reader_scrolled), &ui.build_reader);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.build_output)), "stderr", NULL);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.build_output)), "stdout", NULL);
   ui.console_css = gtk_css_provider_new();
   gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(ui.window),
      GTK_STYLE_PROVIDER(ui.console_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
   gtk_widget_show_all(ui.window);
   focus_editor();
   the_tool_program_console_visibility(1, 0);
   sync_console();
   ui.active = 1;
   /* Legacy core spelling for an active interactive display. GTK still
    * renders exclusively through TheDriverOps and never initializes curses. */
   curses_started = TRUE;
   ui.tick = g_timeout_add(500, blink, NULL);
   terminal_lines = ui.rows ? ui.rows : 28;
   terminal_cols = ui.cols ? ui.cols : 100;
   return 1;
}

static TheDriverWindow *track(TheDriverWindow *window)
{
   GtkSurface *surface;
   if (!window)
      return NULL;
   surface = g_new0(GtkSurface, 1);
   surface->window = window;
   ui.surfaces = g_list_append(ui.surfaces, surface);
   return window;
}

static TheDriverWindow *create_window(int rows, int cols, int row, int col)
{
   TheDriverWindow *window = track(the_headless_driver_ops.create_window(rows, cols, row, col));
   if (window && ui.menu_focus && in_macro && !ui.menu_popup)
      ui.menu_popup = window;
   return window;
}

static TheDriverWindow *create_role(CHARTYPE scrno, short role,
                                     int rows, int cols, int row, int col)
{
   return track(headless_driver_create_screen_role(scrno, role, rows, cols, row, col));
}

static TheDriverWindow *create_global(TheDriverGlobalWindowRole role,
                                       int rows, int cols, int row, int col)
{
   return track(headless_driver_create_global_window(role, rows, cols, row, col));
}

static void delete_window(TheDriverWindow *window)
{
   GList *item;
   for (item = ui.surfaces; item; item = item->next)
      if (((GtkSurface *)item->data)->window == window)
      {
         g_free(item->data);
         ui.surfaces = g_list_delete_link(ui.surfaces, item);
         break;
      }
   the_headless_driver_ops.delete_window(window);
   if (window == ui.menu_popup)
      finish_menu_focus();
   redraw();
}

static void refresh_window(TheDriverWindow *window)
{
   GList *item;
   for (item = ui.surfaces; item; item = item->next)
      if (((GtkSurface *)item->data)->window == window)
         ((GtkSurface *)item->data)->visible = 1;
   the_headless_driver_ops.refresh_window(window);
   redraw();
}

static void clear_screen_roles(void)
{
   headless_driver_set_current_screen(current_screen);
   the_headless_driver_ops.clear_current_screen_roles();
}

static void set_role_attr(short role, TheDriverAttr attr)
{
   the_headless_driver_ops.set_window_attr(driver_current_role_window(role), attr);
}

static void clear_role_to_eol(short role)
{
   headless_driver_set_current_screen(current_screen);
   the_headless_driver_ops.clear_current_role_to_eol(role);
}

static void update(void)
{
   the_headless_driver_ops.update();
   if (ui.window && CURRENT_FILE)
   {
      char *title = g_strdup_printf("%s%s — CRXTHE (GTK)",
         the_editor_buffer_dirty(CURRENT_FILE) ? "* " : "", CURRENT_FILE->fname);
      gtk_window_set_title(GTK_WINDOW(ui.window), title);
      g_free(title);
   }
   sync_console();
   sync_logs();
   sync_menus();
   redraw();
}

static int read_input(TheInputEvent *event)
{
   gint64 deadline = ui.timeout < 0 ? G_MAXINT64
                                    : g_get_monotonic_time() + ui.timeout * 1000;
   *event = the_input_event_none();
   if (!in_macro && !the_panel_active && the_tools_take_menu_run(event)) return 1;
   while (ui.active)
   {
      if (ui.menu_focus && !ui.menu_popup && !ui.native_menu && !in_macro && g_queue_is_empty(&ui.inputs))
         finish_menu_focus();
      if (!ui.input_window && !the_panel_active && !in_macro && ui.rows && ui.cols
      && (ui.rows != terminal_lines || ui.cols != terminal_cols))
         return the_input_event_from_resize(ui.rows, ui.cols, event);
      if (pop_input(event))
      {
         ui.blink_visible = 1;
         return 1;
      }
      the_tools_native_poll();
      sync_console();
      sync_logs();
      sync_menus();
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      if (!g_queue_is_empty(&ui.inputs))
         continue;
      if (g_get_monotonic_time() >= deadline)
         return 0;
      g_usleep(5000);
   }
   return 0;
}

static int read_raw(TheDriverWindow *window)
{
   TheInputEvent event;
   int key = THE_KEY_NONE;
   ui.input_window = window;
   redraw();
   if (read_input(&event))
      the_input_event_to_legacy_key(&event, &key);
   ui.input_window = NULL;
   return key;
}

static void set_timeout(int milliseconds) { ui.timeout = milliseconds; }
static void present_cursor(bool visible) { ui.cursor_visible = visible; redraw(); }
static void resize(int rows, int cols)
{
   if (rows > 0) terminal_lines = rows;
   if (cols > 0) terminal_cols = cols;
}
static const char *version(void) { return "gtk3"; }

static void shutdown_driver(int prompt)
{
   GList *item;
   (void)prompt;
   ui.active = 0;
   curses_started = FALSE;
   if (ui.tick) g_source_remove(ui.tick);
   ui.tick = 0;
   if (ui.split_layout_idle) g_source_remove(ui.split_layout_idle);
   ui.split_layout_idle = 0;
   if (ui.console_css)
   {
      gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(ui.window),
                                                    GTK_STYLE_PROVIDER(ui.console_css));
      g_object_unref(ui.console_css);
      ui.console_css = NULL;
   }
   if (ui.font_css)
   {
      gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(ui.window), GTK_STYLE_PROVIDER(ui.font_css));
      g_clear_object(&ui.font_css);
   }
   g_clear_object(&ui.filefields_css);
   cancel_reader_scroll(&ui.program_reader); cancel_reader_scroll(&ui.build_reader);
   if (ui.program_reader.text) g_string_free(ui.program_reader.text, TRUE);
   if (ui.build_reader.text) g_string_free(ui.build_reader.text, TRUE);
   ui.program_reader.text = ui.build_reader.text = NULL;
   if (ui.native_menu) gtk_menu_shell_deactivate(GTK_MENU_SHELL(ui.native_menu));
   g_clear_object(&ui.menu_focus);
   g_clear_object(&ui.menu_button_origin);
   g_clear_pointer(&ui.catalog_signature, g_free);
   ui.menu_popup = NULL;
   g_clear_object(&ui.log_origin);
   g_clear_object(&ui.last_nonlog_focus);
   if (ui.window) gtk_widget_destroy(ui.window);
   if (ui.logs) g_ptr_array_unref(ui.logs);
   ui.logs = NULL;
   ui.window = ui.editor = ui.console = NULL;
   g_clear_object(&ui.menu_model);
   if (ui.menu_actions) g_ptr_array_unref(ui.menu_actions);
   ui.menu_actions = NULL;
   if (ui.im) g_object_unref(ui.im);
   ui.im = NULL;
   if (ui.application)
   {
      /* The editor owns the event loop while its windows exist. With the
       * final window destroyed, run() returns immediately and completes
       * GtkApplication's shutdown, native-menu and D-Bus cleanup. */
      g_application_run(G_APPLICATION(ui.application), 0, NULL);
      g_clear_object(&ui.application);
   }
   if (ui.font) pango_font_description_free(ui.font);
   ui.font = NULL;
   for (item = ui.surfaces; item; item = item->next)
      g_free(item->data);
   g_list_free(ui.surfaces);
   ui.surfaces = NULL;
   while (!g_queue_is_empty(&ui.inputs))
   {
      GtkInput *input = g_queue_pop_head(&ui.inputs);
      g_free(input->text);
      g_free(input->form);
      g_free(input->filefields);
      g_free(input);
   }
   headless_driver_reset();
   g_clear_object(&ui.filefields_css);
   g_clear_pointer(&ui.filefields_scratch, g_free);
}

static const TheDriverModuleLifecycle lifecycle = {
   .name = "gtk", .activate = activate, .start = startup,
   .shutdown = shutdown_driver, .resize_terminal = resize,
   .set_current_screen = headless_driver_set_current_screen,
   .set_screen_current_role = headless_driver_set_screen_current_role,
   .create_screen_role = create_role, .create_global_window = create_global,
   .read_raw_window_key = read_raw, .ui_version = version,
   .current_cursor_shape = cursor_shape, .current_cursor_blink = cursor_blink,
   .log_count = headless_driver_log_count, .log_entry = headless_driver_log_entry
};

int the_driver_module_abi_version(void) { return THE_DRIVER_MODULE_ABI_VERSION; }
const TheDriverOps *the_driver_module_ops(void)
{
   operations = the_headless_driver_ops;
   operations.create_window = create_window;
   operations.delete_window = delete_window;
   operations.refresh_window = refresh_window;
   operations.refresh_window_now = refresh_window;
   operations.clear_current_screen_roles = clear_screen_roles;
   operations.set_current_role_attr = set_role_attr;
   operations.clear_current_role_to_eol = clear_role_to_eol;
   operations.update = update;
   operations.read_input_event = read_input;
   operations.set_current_window_timeout = set_timeout;
   operations.present_cursor = present_cursor;
   return &operations;
}
const TheDriverModuleLifecycle *the_driver_module_lifecycle(void) { return &lifecycle; }
