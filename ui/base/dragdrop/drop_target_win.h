// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DROP_TARGET_WIN_H_
#define UI_BASE_DRAGDROP_DROP_TARGET_WIN_H_

#include <objidl.h>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

// Windows interface.
struct IDropTargetHelper;

namespace ui {

// A DropTarget implementation that takes care of the details of dnd. While
// this class is concrete, subclasses will most likely want to override various
// OnXXX methods.
//
// Because DropTarget is ref counted you shouldn't delete it directly,
// rather wrap it in a scoped_refptr. Be sure and invoke RevokeDragDrop(m_hWnd)
// before the HWND is deleted too.
//
// This class is meant to be used in a STA and is not multithread-safe.
class COMPONENT_EXPORT(UI_BASE) DropTargetWin : public IDropTarget {
 public:
  DropTargetWin();

  DropTargetWin(const DropTargetWin&) = delete;
  DropTargetWin& operator=(const DropTargetWin&) = delete;

  virtual ~DropTargetWin();

  // Initialize the drop target by associating it with the given HWND.
  void Init(HWND hwnd);

  // IDropTarget implementation:
  HRESULT __stdcall DragEnter(IDataObject* data_object,
                              DWORD key_state,
                              POINTL cursor_position,
                              DWORD* effect) override;
  HRESULT __stdcall DragOver(DWORD key_state,
                             POINTL cursor_position,
                             DWORD* effect) override;
  HRESULT __stdcall DragLeave() override;
  HRESULT __stdcall Drop(IDataObject* data_object,
                         DWORD key_state,
                         POINTL cursor_position,
                         DWORD* effect) override;

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object) override;
  ULONG __stdcall AddRef() override;
  ULONG __stdcall Release() override;

 protected:
  // Returns the hosting HWND.
  HWND GetHWND() { return hwnd_; }

  // Invoked when the cursor first moves over the hwnd during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  // Invoked when the cursor moves over the window during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  // Invoked when the cursor moves outside the bounds of the hwnd during a
  // dnd session.
  virtual void OnDragLeave(IDataObject* data_object);

  // Invoked when the drop ends on the window. This should return the operation
  // that was taken.
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Returns the cached drop helper, creating one if necessary. The returned
  // object is not addrefed. May return NULL if the object couldn't be created.
  static IDropTargetHelper* DropHelper();

  // The data object currently being dragged over this drop target.
  scoped_refptr<IDataObject> current_data_object_;

  // A helper object that is used to provide drag image support while the mouse
  // is dragging over the content area.
  //
  // DO NOT ACCESS DIRECTLY! Use DropHelper() instead, which will lazily create
  // this if it doesn't exist yet. This object can take tens of milliseconds to
  // create, and we don't want to block any window opening for this, especially
  // since often, DnD will never be used. Instead, we force this penalty to the
  // first time it is actually used.
  static IDropTargetHelper* cached_drop_target_helper_;

  // The HWND of the source. This HWND is used to determine coordinates for
  // mouse events that are sent to the renderer notifying various drag states.
  HWND hwnd_;

  ULONG ref_count_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DROP_TARGET_WIN_H_
