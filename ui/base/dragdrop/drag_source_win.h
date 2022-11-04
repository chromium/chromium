// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
#define UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_

#include <objidl.h>
#include <wrl/implements.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

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

  DragSourceWin(const DragSourceWin&) = delete;
  DragSourceWin& operator=(const DragSourceWin&) = delete;

  ~DragSourceWin() override = default;

  // Stop the drag operation at the next chance we get.  This doesn't
  // synchronously stop the drag (since Windows is controlling that),
  // but lets us tell Windows to cancel the drag the next chance we get.
  void CancelDrag() {
    cancel_drag_ = true;
  }

  // IDropSource implementation:
  HRESULT __stdcall QueryContinueDrag(BOOL escape_pressed,
                                      DWORD key_state) override;
  HRESULT __stdcall GiveFeedback(DWORD effect) override;

  // Used to set the active data object for the current drag operation. The
  // caller must ensure that |data| is not destroyed before the nested drag loop
  // terminates.
  void set_data(const OSExchangeData* data) { data_ = data; }

 protected:
  virtual void OnDragSourceDrop();

 private:
  // Set to true if we want to cancel the drag operation.
  bool cancel_drag_ = false;

  raw_ptr<const OSExchangeData> data_ = nullptr;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
