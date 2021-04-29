// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
#define UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_

#include <objidl.h>
#include <wrl/implements.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace ui {

class OSExchangeData;

// A base IDropSource implementation. Handles notifications sent by an active
// drag-drop operation as the user mouses over other drop targets on their
// system. This object tells Windows whether or not the drag should continue,
// and supplies the appropriate cursors.
class DragSourceWin
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDropSource> {
 public:
  // Factory method to avoid exporting the class and all it derives from.
  static COMPONENT_EXPORT(
      UI_BASE) Microsoft::WRL::ComPtr<DragSourceWin> Create();

  // Use Create() to construct these objects. Direct calls to the constructor
  // are an error - it is only public because a WRL helper function creates the
  // objects.
  DragSourceWin();
  ~DragSourceWin() override = default;

  // Stop the drag operation at the next chance we get.  This doesn't
  // synchronously stop the drag (since Windows is controlling that),
  // but lets us tell Windows to cancel the drag the next chance we get.
  void CancelDrag() {
    cancel_drag_ = true;
  }

  // This is used to tell if the drag drop actually started, for generating
  // a BooleanSuccess histogram.
  int num_query_continues() const { return num_query_continues_; }

  // IDropSource implementation:
  HRESULT __stdcall QueryContinueDrag(BOOL escape_pressed,
                                      DWORD key_state) override;
  HRESULT __stdcall GiveFeedback(DWORD effect) override;

  // Used to set the active data object for the current drag operation. The
  // caller must ensure that |data| is not destroyed before the nested drag loop
  // terminates.
  void set_data(const OSExchangeData* data) { data_ = data; }

 protected:
  virtual void OnDragSourceCancel() {}
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove() {}

 private:
  // Set to true if we want to cancel the drag operation.
  bool cancel_drag_;

  const OSExchangeData* data_;

  // The number of times for this drag that Windows asked if the drag should
  // continue. This is used in DesktopDragDropClientWin::StartDragAndDrop to
  // detect if touch drag drop started successfully. See comment there for much
  // more info.
  int num_query_continues_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DragSourceWin);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
