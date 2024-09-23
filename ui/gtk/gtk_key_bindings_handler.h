// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_
#define UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/events/platform_event.h"
#include "ui/gtk/gtk_compat.h"

namespace ui {
class Event;
}

namespace gtk {

// This class is a convenience class for handling editor key bindings defined
// in gtk keyboard theme.
// In gtk, only GtkEntry and GtkTextView support customizing editor key bindings
// through keyboard theme. And in gtk keyboard theme definition file, each key
// binding must be bound to a specific class or object. So existing keyboard
// themes only define editor key bindings exactly for GtkEntry and GtkTextView.
// Then, the only way for us to intercept editor key bindings defined in
// keyboard theme, is to create a GtkEntry or GtkTextView object and call
// gtk_bindings_activate_event() against it for the key events. If a key event
// matches a predefined key binding, corresponding signal will be emitted.
// GtkTextView is used here because it supports more key bindings than GtkEntry,
// but in order to minimize the side effect of using a GtkTextView object, a new
// class derived from GtkTextView is used, which overrides all signals related
// to key bindings, to make sure GtkTextView won't receive them.
//
// See third_party/blink/renderer/core/editing/commands/editor_command.cc for
// detailed definition of Blink edit commands.
class GtkKeyBindingsHandler {
 public:
  GtkKeyBindingsHandler();
  virtual ~GtkKeyBindingsHandler();

  // Matches a key event against predefined gtk key bindings, false will be
  // returned if the key event doesn't correspond to a predefined key binding.
  // Edit commands matched with |event| will be stored in |edit_commands|, if
  // non-nullptr.
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands);

 private:
  // Object structure of Handler class, which is derived from GtkTextView.
  struct Handler {
    // Starting in Gtk4, GtkTextView subclasses from GtkWidget instead of
    // GtkContainer.  This class is only used on Gtk3, so to ensure ABI
    // compatibility, we always want the Gtk3 struct layout even when building
    // with Gtk4 headers.  To facilitate this, we manually copy the class
    // hierarchy up to GtkWidget.
    GtkWidget widget;
    raw_ptr<void> container_private;
    raw_ptr<void> text_view_private;
    raw_ptr<GtkKeyBindingsHandler> owner;
  };

  // Class structure of Handler class.
  struct HandlerClass {
    // Class layout for types changes between GTK versions, but is stable within
    // the same major version.  This class is only used on Gtk3, so manually
    // expand the class layout as it appears in Gtk3.
    GInitiallyUnownedClass parent_class;

    // GtkWidgetClass and GtkContainerClass
    guint pad0;
    void* pad1[95];
    unsigned int pad2 : 1;
    void* pad3[8];

    // GtkTextViewClass
    void (*populate_popup)(GtkTextView* text_view, GtkWidget* popup);
    void (*move_cursor)(GtkTextView* text_view,
                        GtkMovementStep step,
                        gint count,
                        gboolean extend_selection);
    void (*set_anchor)(GtkTextView* text_view);
    void (*insert_at_cursor)(GtkTextView* text_view, const gchar* str);
    void (*delete_from_cursor)(GtkTextView* text_view,
                               GtkDeleteType type,
                               gint count);
    void (*backspace)(GtkTextView* text_view);
    void (*cut_clipboard)(GtkTextView* text_view);
    void (*copy_clipboard)(GtkTextView* text_view);
    void (*paste_clipboard)(GtkTextView* text_view);
    void (*toggle_overwrite)(GtkTextView* text_view);
    GtkTextBuffer* (*create_buffer)(GtkTextView* text_view);
    void (*draw_layer)(GtkTextView* text_view,
                       GtkTextViewLayer layer,
                       cairo_t* cr);
    gboolean (*extend_selection)(GtkTextView* text_view,
                                 GtkTextExtendSelection granularity,
                                 const GtkTextIter* location,
                                 GtkTextIter* start,
                                 GtkTextIter* end);
    void (*insert_emoji)(GtkTextView* text_view);
    void* pad4[4];
  };

  // Creates a new instance of Handler class.
  GtkWidget* CreateNewHandler();

  // Adds an edit command to the key event.
  void EditCommandMatched(ui::TextEditCommand command,
                          const std::string& value);

  // Initializes Handler structure.
  static void HandlerInit(Handler* self);

  // Initializes HandlerClass structure.
  static void HandlerClassInit(HandlerClass* klass);

  // Registeres Handler class to GObject type system and return its type id.
  static GType HandlerGetType();

  // Gets the GtkKeyBindingsHandler object which owns the Handler object.
  static GtkKeyBindingsHandler* GetHandlerOwner(GtkTextView* text_view);

  // Handler of "backspace" signal.
  static void BackSpace(GtkTextView* text_view);

  // Handler of "copy-clipboard" signal.
  static void CopyClipboard(GtkTextView* text_view);

  // Handler of "cut-clipboard" signal.
  static void CutClipboard(GtkTextView* text_view);

  // Handler of "delete-from-cursor" signal.
  static void DeleteFromCursor(GtkTextView* text_view,
                               GtkDeleteType type,
                               gint count);

  // Handler of "insert-at-cursor" signal.
  static void InsertAtCursor(GtkTextView* text_view, const gchar* str);

  // Handler of "move-cursor" signal.
  static void MoveCursor(GtkTextView* text_view,
                         GtkMovementStep step,
                         gint count,
                         gboolean extend_selection);

  // Handler of "move-viewport" signal.
  static void MoveViewport(GtkTextView* text_view,
                           GtkScrollStep step,
                           gint count);

  // Handler of "paste-clipboard" signal.
  static void PasteClipboard(GtkTextView* text_view);

  // Handler of "select-all" signal.
  static void SelectAll(GtkTextView* text_view, gboolean select);

  // Handler of "set-anchor" signal.
  static void SetAnchor(GtkTextView* text_view);

  // Handler of "toggle-cursor-visible" signal.
  static void ToggleCursorVisible(GtkTextView* text_view);

  // Handler of "toggle-overwrite" signal.
  static void ToggleOverwrite(GtkTextView* text_view);

  // Handler of "show-help" signal.
  static gboolean ShowHelp(GtkWidget* widget, GtkWidgetHelpType arg1);

  // Handler of "move-focus" signal.
  static void MoveFocus(GtkWidget* widget, GtkDirectionType arg1);

  raw_ptr<GtkWidget> fake_window_;
  raw_ptr<GtkWidget> handler_;

  // Buffer to store the match results.
  std::vector<ui::TextEditCommandAuraLinux> edit_commands_;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_
