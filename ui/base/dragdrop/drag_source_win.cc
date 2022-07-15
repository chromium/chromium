// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_source_win.h"

#include "ui/base/dragdrop/os_exchange_data_provider_win.h"

namespace ui {

Microsoft::WRL::ComPtr<DragSourceWin> DragSourceWin::Create() {
  return Microsoft::WRL::Make<DragSourceWin>();
}

DragSourceWin::DragSourceWin() : cancel_drag_(false), data_(nullptr) {
}

HRESULT DragSourceWin::QueryContinueDrag(BOOL escape_pressed, DWORD key_state) {
  if (cancel_drag_)
    return DRAGDROP_S_CANCEL;

  if (escape_pressed) {
    OnDragSourceCancel();
    return DRAGDROP_S_CANCEL;
  }

  if (!(key_state & MK_LBUTTON) && !(key_state & MK_RBUTTON)) {
    OnDragSourceDrop();
    return DRAGDROP_S_DROP;
  }

  OnDragSourceMove();
  return S_OK;
}

HRESULT DragSourceWin::GiveFeedback(DWORD effect) {
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

void DragSourceWin::OnDragSourceDrop() {
  DCHECK(data_);
  OSExchangeDataProviderWin::GetDataObjectImpl(*data_)->set_in_drag_loop(false);
}

}  // namespace ui
