// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drop_target_win.h"

#include <shlobj.h>

#include "base/check.h"

namespace ui {

IDropTargetHelper* DropTargetWin::cached_drop_target_helper_ = nullptr;

DropTargetWin::DropTargetWin() : hwnd_(nullptr), ref_count_(0) {}

DropTargetWin::~DropTargetWin() = default;

void DropTargetWin::Init(HWND hwnd) {
  DCHECK(!hwnd_);
  DCHECK(hwnd);
  HRESULT result = RegisterDragDrop(hwnd, this);
  DCHECK(SUCCEEDED(result));
}


// static
IDropTargetHelper* DropTargetWin::DropHelper() {
  if (!cached_drop_target_helper_) {
    CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
                     IID_IDropTargetHelper,
                     reinterpret_cast<void**>(&cached_drop_target_helper_));
  }
  return cached_drop_target_helper_;
}

///////////////////////////////////////////////////////////////////////////////
// DropTargetWin, IDropTarget implementation:

HRESULT DropTargetWin::DragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINTL cursor_position,
                                 DWORD* effect) {
  // Tell the helper that we entered so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->DragEnter(GetHWND(), data_object,
                           reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  current_data_object_ = data_object;
  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect =
      OnDragEnter(current_data_object_.get(), key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT DropTargetWin::DragOver(DWORD key_state,
                                POINTL cursor_position,
                                DWORD* effect) {
  // Tell the helper that we moved over it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragOver(reinterpret_cast<POINT*>(&cursor_position), *effect);

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect =
      OnDragOver(current_data_object_.get(), key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT DropTargetWin::DragLeave() {
  // Tell the helper that we moved out of it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragLeave();

  OnDragLeave(current_data_object_.get());

  current_data_object_ = nullptr;
  return S_OK;
}

HRESULT DropTargetWin::Drop(IDataObject* data_object,
                            DWORD key_state,
                            POINTL cursor_position,
                            DWORD* effect) {
  // Tell the helper that we dropped onto it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->Drop(current_data_object_.get(),
                      reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDrop(current_data_object_.get(), key_state, screen_pt, *effect);
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// DropTargetWin, IUnknown implementation:

HRESULT DropTargetWin::QueryInterface(const IID& iid, void** object) {
  *object = nullptr;
  if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDropTarget)) {
    *object = this;
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG DropTargetWin::AddRef() {
  return ++ref_count_;
}

ULONG DropTargetWin::Release() {
  if (--ref_count_ == 0) {
    delete this;
    return 0U;
  }
  return ref_count_;
}

DWORD DropTargetWin::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  return DROPEFFECT_NONE;
}

DWORD DropTargetWin::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effect) {
  return DROPEFFECT_NONE;
}

void DropTargetWin::OnDragLeave(IDataObject* data_object) {
}

DWORD DropTargetWin::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  return DROPEFFECT_NONE;
}

}  // namespace ui
