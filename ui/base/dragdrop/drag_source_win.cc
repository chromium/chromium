// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_source_win.h"

#include "ui/base/dragdrop/os_exchange_data_provider_win.h"

namespace ui {

Microsoft::WRL::ComPtr<DragSourceWin> DragSourceWin::Create() {
  return Microsoft::WRL::Make<DragSourceWin>();
}

DragSourceWin::DragSourceWin() = default;

HRESULT DragSourceWin::QueryContinueDrag(BOOL escape_pressed, DWORD key_state) {
  if (cancel_drag_ || escape_pressed)
    return DRAGDROP_S_CANCEL;

  if (!(key_state & MK_LBUTTON) && !(key_state & MK_RBUTTON)) {
    OnDragSourceDrop();
    return DRAGDROP_S_DROP;
  }

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
