// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_POINTER_TOUCH_EDITING_CONTROLLER_H_
#define UI_BASE_POINTER_TOUCH_EDITING_CONTROLLER_H_

#include "base/component_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace gfx {
class Point;
class Rect;
class SelectionBound;
}  // namespace gfx

namespace ui {

// An interface implemented by widget that has text that can be selected/edited
// using touch.
class COMPONENT_EXPORT(UI_BASE) TouchEditable
    : public ui::SimpleMenuModel::Delegate {
 public:
  // Commands that all TouchEditables support:
  enum MenuCommands {
    // Don't use command ID 0 - a lot of tests use 0 for "no command".
    kCut = 1,
    kCopy,
    kPaste,
    kSelectAll,
    kLastTouchEditableCommandId = kSelectAll,
  };

  // TODO(mohsen): Consider switching from local coordinates to screen
  // coordinates in this interface and see if it will simplify things.

  // Select everything between start and end (points are in view's local
  // coordinate system). |start| is the logical start and |end| is the logical
  // end of selection. Visually, |start| may lie after |end|.
  virtual void SelectRect(const gfx::Point& start, const gfx::Point& end) = 0;

  // Move the caret to |point|. |point| is in local coordinates.
  virtual void MoveCaretTo(const gfx::Point& point) = 0;

  // Gets the end points of the current selection. The end points |anchor| and
  // |focus| must be the cursor rect for the logical start and logical end of
  // selection (in local coordinates):
  // ____________________________________
  // | textfield with |selected text|   |
  // ------------------------------------
  //                  ^anchor       ^focus
  //
  // Visually, anchor could be to the right of focus in the figure above - it
  // depends on the selection direction.
  virtual void GetSelectionEndPoints(gfx::SelectionBound* anchor,
                                     gfx::SelectionBound* focus) = 0;

  // Gets the bounds of the client view in its local coordinates.
  virtual gfx::Rect GetBounds() = 0;

  // Gets the NativeView hosting the client.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Converts a point to/from screen coordinates from/to client view.
  virtual void ConvertPointToScreen(gfx::Point* point) = 0;
  virtual void ConvertPointFromScreen(gfx::Point* point) = 0;

  // Returns true if the editable draws its own handles (hence, the
  // TouchEditingControllerDeprecated need not draw handles).
  virtual bool DrawsHandles() = 0;

  // Tells the editable to open context menu.
  virtual void OpenContextMenu(const gfx::Point& anchor) = 0;

  // Tells the editable to end touch editing and destroy touch selection
  // controller it owns.
  virtual void DestroyTouchSelection() = 0;

 protected:
  ~TouchEditable() override {}
};

// This defines the callback interface for other code to be notified of changes
// in the state of a TouchEditable.
class COMPONENT_EXPORT(UI_BASE) TouchEditingControllerDeprecated {
 public:
  virtual ~TouchEditingControllerDeprecated() {}

  // Creates a TouchEditingControllerDeprecated. Caller owns the returned
  // object.
  static TouchEditingControllerDeprecated* Create(TouchEditable* client_view);

  // Notifies the controller that the selection has changed.
  virtual void SelectionChanged() = 0;
};

class COMPONENT_EXPORT(UI_BASE) TouchEditingControllerFactory {
 public:
  virtual ~TouchEditingControllerFactory() {}

  static void SetInstance(TouchEditingControllerFactory* instance);

  virtual TouchEditingControllerDeprecated* Create(
      TouchEditable* client_view) = 0;
};

}  // namespace ui

#endif  // UI_BASE_POINTER_TOUCH_EDITING_CONTROLLER_H_
