// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>

#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace ui {
class KeyEvent;
class MouseEvent;
class GestureEvent;
class ScopedClipboardWriter;
class SimpleMenuModel;
}  // namespace ui

namespace views {

class Textfield;

// This defines the callback interface for other code to be notified of changes
// in the state of a text field.
class VIEWS_EXPORT TextfieldController {
 public:
  // This method is called whenever the text in the field is changed by the
  // user. It won't be called if the text is changed by calling
  // Textfield::SetText() or Textfield::AppendText().
  virtual void ContentsChanged(Textfield* sender,
                               const std::u16string& new_contents) {}

  // Called to get notified about keystrokes in the edit.
  // Returns true if the message was handled and should not be processed
  // further. If it returns false the processing continues.
  virtual bool HandleKeyEvent(Textfield* sender, const ui::KeyEvent& key_event);

  // Called to get notified about mouse events in the edit.
  // Returns true if the message was handled and should not be processed
  // further. Currently, only mouse down events and mouse wheel events are sent
  // here.
  virtual bool HandleMouseEvent(Textfield* sender,
                                const ui::MouseEvent& mouse_event);

  // Called to get notified about gesture events in the edit.
  // Returns true if the message was handled and should not be processed
  // further. Currently, only tap events are sent here.
  virtual bool HandleGestureEvent(Textfield* sender,
                                  const ui::GestureEvent& gesture_event);

  // Called before performing a user action that may change the textfield.
  // It's currently only supported by Views implementation.
  virtual void OnBeforeUserAction(Textfield* sender) {}

  // Called after performing a user action that may change the textfield.
  // It's currently only supported by Views implementation.
  virtual void OnAfterUserAction(Textfield* sender) {}

  // Called before performing a Cut or Copy operation, allowing the controller
  // to intercept and provide copy contents. Implementations can populate
  // `copy_contents` and return true to override the default obtained text;
  // returning false will cause the textfield to use its standard clipboard
  // handling. The textfield's existing selection will be deleted when this
  // method is called for a Cut operation.
  // NOTE: This hook runs before OnAfterCutOrCopy() and may affect the copied
  // text set in the clipboard, as well as changes to the textfield.
  virtual bool OnBeforeCutOrCopy(Textfield* sender,
                                 std::u16string* copy_contents);

  // Called after performing a Cut or Copy operation.
  virtual void OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_buffer) {}

  // Called before performing a paste operation, allowing the controller to
  // intercept and provide paste contents. Implementations can populate
  // `paste_contents` and return true to override the default clipboard read;
  // returning false will cause the textfield to use its standard clipboard
  // handling.
  // NOTE: This hook runs before OnAfterPaste() and only affects the paste
  // content source; it does not change how the content is inserted.
  virtual bool OnBeforePaste(Textfield* sender, std::u16string* paste_contents);

  // Called after performing a Paste operation.
  virtual void OnAfterPaste() {}

  // Called after the textfield has written drag data to give the controller a
  // chance to modify the drag data.
  virtual void OnWriteDragData(ui::OSExchangeData* data) {}

  // Called after the textfield has set default drag operations to give the
  // controller a chance to update them.
  virtual void OnGetDragOperationsForTextfield(int* drag_operations) {}

  // Returns a `ui::ScopedClipboardWriter` to be used for clipboard write
  // operations. This lets the controller enhance the data written to the
  // clipboard by adding extra information such as the exact source of the data.
  virtual std::unique_ptr<ui::ScopedClipboardWriter> CreateClipboardWriter();

  // Enables the controller to append to the accepted drop formats.
  virtual void AppendDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) {}

  // Called when a drop of dragged data happens on the textfield. This method is
  // called before regular handling of the drop. If this returns a drag
  // operation other than `ui::mojom::DragOperation::kNone`, regular handling is
  // skipped.
  virtual ui::mojom::DragOperation OnDrop(const ui::DropTargetEvent& event);

  // Called to get async drop callback to be run later.
  virtual views::View::DropCallback CreateDropCallback(
      const ui::DropTargetEvent& event);

  // Gives the controller a chance to modify the context menu contents.
  virtual void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {}

 protected:
  virtual ~TextfieldController() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_CONTROLLER_H_
