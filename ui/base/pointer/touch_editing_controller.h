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
  // TODO(b/263419885): Rewrite MenuCommands as an enum class.
  enum MenuCommands {
    // Don't use command ID 0 - a lot of tests use 0 for "no command".
    kCut = 1,
    kCopy,
    kPaste,
    kSelectAll,
    kSelectWord,
    kLastTouchEditableCommandId = kSelectWord,
  };

  // TODO(b/266345972): Consider switching from local coordinates to screen
  // coordinates in this interface and see if it will simplify things.

  // Moves the caret to |position|. |position| is in local coordinates.
  virtual void MoveCaret(const gfx::Point& position) = 0;

  // Moves the logical end of the selection according to |extent| while keeping
  // the logical start of the selection fixed. Here, |extent| corresponds to the
  // position (in local coordinates) of the touch handle being dragged to update
  // the selection range.
  //
  // Note that the resultant end of the selection depends on the behaviour of
  // the TouchEditable, e.g. for "expand by word, shrink by character", the
  // selection end can move to the character or word boundary nearest to
  // |extent| depending on the previous extent position:
  // ____________________________________
  // | textf|ield wit|h selected text   |
  // ------------------------------------
  //                 ^extent
  //        ^start   ^end
  //
  // ____________________________________
  // | textf|ield with selec|ted| text   |
  // ------------------------------------
  //                        ^extent
  //        ^start              ^end
  //
  virtual void MoveRangeSelectionExtent(const gfx::Point& extent) = 0;

  // Sets the logical start and end of the selection according to |base| and
  // |extent|. |base| corresponds to the position of the fixed touch handle and
  // determines the logical start of the selection. |extent| corresponds to the
  // position of the currently dragging handle and determines the logical end of
  // the selection, which may be visually before, on, or after the logical start
  // of the selection. Both |base| and |start| are in local coordinates.
  virtual void SelectBetweenCoordinates(const gfx::Point& base,
                                        const gfx::Point& extent) = 0;

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

  // Checks whether the client is currently in a selection dragging state, i.e.
  // whether it is currently handling scroll gestures to adjust the cursor or
  // selection. If so, selection changes will notify the controller to update
  // the quick menu and touch selection magnifier without showing touch handles.
  virtual bool IsSelectionDragging() const = 0;

  // Converts a point to/from screen coordinates from/to client view.
  virtual void ConvertPointToScreen(gfx::Point* point) = 0;
  virtual void ConvertPointFromScreen(gfx::Point* point) = 0;

  // Tells the editable to open context menu.
  virtual void OpenContextMenu(const gfx::Point& anchor) = 0;

  // Tells the editable to end touch editing and destroy touch selection
  // controller it owns.
  virtual void DestroyTouchSelection() = 0;

 protected:
  ~TouchEditable() override {}
};

}  // namespace ui

#endif  // UI_BASE_POINTER_TOUCH_EDITING_CONTROLLER_H_
