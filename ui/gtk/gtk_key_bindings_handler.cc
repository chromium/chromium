// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_key_bindings_handler.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"

using ui::TextEditCommand;

namespace gtk {

GtkKeyBindingsHandler::GtkKeyBindingsHandler()
    : fake_window_(gtk_offscreen_window_new()), handler_(CreateNewHandler()) {
  DCHECK(!GtkCheckVersion(4));
  gtk_container_add(
      GlibCast<GtkContainer>(fake_window_.get(), gtk_container_get_type()),
      handler_);
}

GtkKeyBindingsHandler::~GtkKeyBindingsHandler() {
  gtk_widget_destroy(handler_);
  gtk_widget_destroy(fake_window_);
}

bool GtkKeyBindingsHandler::MatchEvent(
    const ui::Event& event,
    std::vector<ui::TextEditCommandAuraLinux>* edit_commands) {
  CHECK(event.IsKeyEvent());

  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  if (key_event.is_char())
    return false;

  GdkEvent* gdk_event = GdkEventFromKeyEvent(key_event);
  if (!gdk_event)
    return false;

  edit_commands_.clear();
  // If this key event matches a predefined key binding, corresponding signal
  // will be emitted.

  auto* key = reinterpret_cast<GdkEventKey*>(gdk_event);
  DCHECK(key->type == GdkKeyPress() || key->type == GdkKeyRelease());
  gtk_bindings_activate_event(G_OBJECT(handler_.get()), key);
  gdk_event_free(gdk_event);

  bool matched = !edit_commands_.empty();
  if (edit_commands)
    edit_commands->swap(edit_commands_);
  return matched;
}

GtkWidget* GtkKeyBindingsHandler::CreateNewHandler() {
  Handler* handler =
      static_cast<Handler*>(g_object_new(HandlerGetType(), nullptr));

  handler->owner = this;

  // We don't need to show the |handler| object on screen, so set its size to
  // zero.
  gtk_widget_set_size_request(GTK_WIDGET(handler), 0, 0);

  // Prevents it from handling any events by itself.
  gtk_widget_set_sensitive(GTK_WIDGET(handler), FALSE);
  gtk_widget_set_events(GTK_WIDGET(handler), 0);
  gtk_widget_set_can_focus(GTK_WIDGET(handler), TRUE);

  return GTK_WIDGET(handler);
}

void GtkKeyBindingsHandler::EditCommandMatched(TextEditCommand command,
                                               const std::string& value) {
  edit_commands_.emplace_back(command, value);
}

void GtkKeyBindingsHandler::HandlerInit(Handler* self) {
  self->owner = nullptr;
}

void GtkKeyBindingsHandler::HandlerClassInit(HandlerClass* klass) {
  // Overrides all virtual methods related to editor key bindings.
  klass->backspace = BackSpace;
  klass->copy_clipboard = CopyClipboard;
  klass->cut_clipboard = CutClipboard;
  klass->delete_from_cursor = DeleteFromCursor;
  klass->insert_at_cursor = InsertAtCursor;
  klass->move_cursor = MoveCursor;
  klass->paste_clipboard = PasteClipboard;
  klass->set_anchor = SetAnchor;
  klass->toggle_overwrite = ToggleOverwrite;

  // "move-focus", "move-viewport", "select-all" and "toggle-cursor-visible"
  // have no corresponding virtual methods. Since glib 2.18 (gtk 2.14),
  // g_signal_override_class_handler() is introduced to override a signal
  // handler.
  g_signal_override_class_handler("move-focus", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveFocus));

  g_signal_override_class_handler("move-viewport", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveViewport));

  g_signal_override_class_handler("select-all", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(SelectAll));

  g_signal_override_class_handler("toggle-cursor-visible",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(ToggleCursorVisible));

  g_signal_override_class_handler("show-help", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(ShowHelp));
}

GType GtkKeyBindingsHandler::HandlerGetType() {
  static gsize type_id = 0;
  if (g_once_init_enter(&type_id)) {
    GType type = g_type_register_static_simple(
        GTK_TYPE_TEXT_VIEW, g_intern_static_string("GtkKeyBindingsHandler"),
        sizeof(HandlerClass),
        reinterpret_cast<GClassInitFunc>(HandlerClassInit), sizeof(Handler),
        reinterpret_cast<GInstanceInitFunc>(HandlerInit),
        static_cast<GTypeFlags>(0));
    g_once_init_leave(&type_id, type);
  }
  return type_id;
}

GtkKeyBindingsHandler* GtkKeyBindingsHandler::GetHandlerOwner(
    GtkTextView* text_view) {
  Handler* handler =
      G_TYPE_CHECK_INSTANCE_CAST(text_view, HandlerGetType(), Handler);
  DCHECK(handler);
  return handler->owner;
}

void GtkKeyBindingsHandler::BackSpace(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommand::DELETE_BACKWARD, std::string());
}

void GtkKeyBindingsHandler::CopyClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::COPY,
                                                 std::string());
}

void GtkKeyBindingsHandler::CutClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::CUT,
                                                 std::string());
}

void GtkKeyBindingsHandler::DeleteFromCursor(GtkTextView* text_view,
                                             GtkDeleteType type,
                                             gint count) {
  if (!count)
    return;

  TextEditCommand commands[2] = {
      TextEditCommand::INVALID_COMMAND,
      TextEditCommand::INVALID_COMMAND,
  };
  switch (type) {
    case GTK_DELETE_CHARS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_FORWARD
                               : TextEditCommand::DELETE_BACKWARD);
      break;
    case GTK_DELETE_WORD_ENDS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_WORD_FORWARD
                               : TextEditCommand::DELETE_WORD_BACKWARD);
      break;
    case GTK_DELETE_WORDS:
      if (count > 0) {
        commands[0] = TextEditCommand::MOVE_WORD_FORWARD;
        commands[1] = TextEditCommand::DELETE_WORD_BACKWARD;
      } else {
        commands[0] = TextEditCommand::MOVE_WORD_BACKWARD;
        commands[1] = TextEditCommand::DELETE_WORD_FORWARD;
      }
      break;
    case GTK_DELETE_DISPLAY_LINES:
      commands[0] = TextEditCommand::MOVE_TO_BEGINNING_OF_LINE;
      commands[1] = TextEditCommand::DELETE_TO_END_OF_LINE;
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_TO_END_OF_LINE
                               : TextEditCommand::DELETE_TO_BEGINNING_OF_LINE);
      break;
    case GTK_DELETE_PARAGRAPH_ENDS:
      commands[0] =
          (count > 0 ? TextEditCommand::DELETE_TO_END_OF_PARAGRAPH
                     : TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH);
      break;
    case GTK_DELETE_PARAGRAPHS:
      commands[0] = TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH;
      commands[1] = TextEditCommand::DELETE_TO_END_OF_PARAGRAPH;
      break;
    default:
      // GTK_DELETE_WHITESPACE has no corresponding editor command.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count) {
    for (auto& command : commands) {
      if (command != TextEditCommand::INVALID_COMMAND)
        owner->EditCommandMatched(command, std::string());
    }
  }
}

void GtkKeyBindingsHandler::InsertAtCursor(GtkTextView* text_view,
                                           const gchar* str) {
  if (str && *str) {
    GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::INSERT_TEXT,
                                                   str);
  }
}

void GtkKeyBindingsHandler::MoveCursor(GtkTextView* text_view,
                                       GtkMovementStep step,
                                       gint count,
                                       gboolean extend_selection) {
  if (!count)
    return;

  TextEditCommand command;
  switch (step) {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_FORWARD
                             : TextEditCommand::MOVE_BACKWARD);
      }
      break;
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      if (extend_selection) {
        command = (count > 0 ? TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION
                             : TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_RIGHT
                             : TextEditCommand::MOVE_LEFT);
      }
      break;
    case GTK_MOVEMENT_WORDS:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_WORD_RIGHT
                             : TextEditCommand::MOVE_WORD_LEFT);
      }
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      if (extend_selection) {
        command = (count > 0 ? TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION
                             : TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION);
      } else {
        command =
            (count > 0 ? TextEditCommand::MOVE_DOWN : TextEditCommand::MOVE_UP);
      }
      break;
    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_LINE
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_LINE);
      }
      break;
    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::
                       MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_PARAGRAPH
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH);
      }
      break;
    case GTK_MOVEMENT_PAGES:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_PAGE_DOWN
                             : TextEditCommand::MOVE_PAGE_UP);
      }
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_DOCUMENT
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT);
      }
      break;
    default:
      // GTK_MOVEMENT_PARAGRAPHS and GTK_MOVEMENT_HORIZONTAL_PAGES have
      // no corresponding editor commands.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count)
    owner->EditCommandMatched(command, std::string());
}

void GtkKeyBindingsHandler::MoveViewport(GtkTextView* text_view,
                                         GtkScrollStep step,
                                         gint count) {
  // Not supported by Blink.
}

void GtkKeyBindingsHandler::PasteClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::PASTE,
                                                 std::string());
}

void GtkKeyBindingsHandler::SelectAll(GtkTextView* text_view, gboolean select) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      select ? TextEditCommand::SELECT_ALL : TextEditCommand::UNSELECT,
      std::string());
}

void GtkKeyBindingsHandler::SetAnchor(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::SET_MARK,
                                                 std::string());
}

void GtkKeyBindingsHandler::ToggleCursorVisible(GtkTextView* text_view) {
  // Not supported by Blink.
}

void GtkKeyBindingsHandler::ToggleOverwrite(GtkTextView* text_view) {
  // Not supported by Blink.
}

gboolean GtkKeyBindingsHandler::ShowHelp(GtkWidget* widget,
                                         GtkWidgetHelpType arg1) {
  // Just for disabling the default handler.
  return TRUE;
}

void GtkKeyBindingsHandler::MoveFocus(GtkWidget* widget,
                                      GtkDirectionType arg1) {
  // Just for disabling the default handler.
}

}  // namespace gtk
