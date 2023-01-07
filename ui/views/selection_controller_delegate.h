// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_SELECTION_CONTROLLER_DELEGATE_H_
#define UI_VIEWS_SELECTION_CONTROLLER_DELEGATE_H_

#include "ui/views/views_export.h"

namespace views {

// An interface implemented/managed by a view which uses the
// SelectionController.
class VIEWS_EXPORT SelectionControllerDelegate {
 public:
  // Returns the associated RenderText instance to be used for selection.
  virtual gfx::RenderText* GetRenderTextForSelectionController() = 0;

  // Methods related to properties of the associated view.

  // Returns true if the associated text view is read only.
  virtual bool IsReadOnly() const = 0;
  // Returns whether the associated view supports drag-and-drop.
  virtual bool SupportsDrag() const = 0;
  // Returns whether there is a drag operation originating from the associated
  // view.
  virtual bool HasTextBeingDragged() const = 0;
  // Sets whether text is being dragged from the associated view. Called only if
  // the delegate supports drag.
  virtual void SetTextBeingDragged(bool value) = 0;
  // Returns the height of the associated view.
  virtual int GetViewHeight() const = 0;
  // Returns the width of the associated view.
  virtual int GetViewWidth() const = 0;
  // Returns the drag selection timer delay. This is the duration after which a
  // drag selection is updated when the event location is outside the text
  // bounds.
  virtual int GetDragSelectionDelay() const = 0;

  // Called before a pointer action which may change the associated view's
  // selection and/or text. Should not be called in succession and must always
  // be followed by an OnAfterPointerAction call.
  virtual void OnBeforePointerAction() = 0;
  // Called after a pointer action. |text_changed| and |selection_changed| can
  // be used by subclasses to make any necessary updates like redraw the text.
  // Must always be preceeded by an OnBeforePointerAction call.
  virtual void OnAfterPointerAction(bool text_changed,
                                    bool selection_changed) = 0;

  // Pastes the text from the selection clipboard at the current cursor
  // position. Always called within a pointer action for a non-readonly view.
  // Returns true if some text was pasted.
  virtual bool PasteSelectionClipboard() = 0;
  // Updates the selection clipboard with the currently selected text. Should
  // empty the selection clipboard if no text is currently selected.
  // NO-OP if the associated text view is obscured. Since this does not modify
  // the render text instance, it may be called outside of a pointer action.
  virtual void UpdateSelectionClipboard() = 0;

 protected:
  virtual ~SelectionControllerDelegate() = default;
};

}  // namespace views

#endif  // UI_VIEWS_SELECTION_CONTROLLER_DELEGATE_H_
