// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/x/selection_utils.h"
#include "ui/ozone/public/platform_clipboard.h"

#if BUILDFLAG(IS_LINUX)
#include "base/memory/weak_ptr.h"
#endif

namespace ui {

class XClipboardHelper;

// Handles clipboard operations for X11.
// Registers to receive standard X11 events, as well as
// XFixesSetSelectionOwnerNotify.  When the remote owner changes, TARGETS and
// text/plain are preemptively fetched.  They can then be provided immediately
// to GetAvailableMimeTypes, and RequestClipboardData when mime_type is
// text/plain.  Otherwise GetAvailableMimeTypes and RequestClipboardData call
// the appropriate X11 functions and invoke callbacks when the associated events
// are received.
class X11ClipboardOzone : public PlatformClipboard {
 public:
  X11ClipboardOzone();
  X11ClipboardOzone(const X11ClipboardOzone&) = delete;
  X11ClipboardOzone& operator=(const X11ClipboardOzone&) = delete;
  ~X11ClipboardOzone() override;

  // PlatformClipboard:
  void OfferClipboardData(ClipboardBuffer buffer,
                          const PlatformClipboard::DataMap& data_map) override;
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override;
  void IsSelectionOwner(ClipboardBuffer buffer,
                        IsSelectionOwnerClosure callback) override;
  void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback data_changed_callback) override;
  bool IsSelectionBufferAvailable() const override;

 private:
  void OnSelectionChanged(ClipboardBuffer buffer);

#if BUILDFLAG(IS_LINUX)
  void OnPortalKeyRead(PlatformClipboard::RequestDataClosure callback,
                       SelectionData selection_data);
  void OnPathsExtracted(PlatformClipboard::RequestDataClosure callback,
                        std::vector<std::string> paths);
  void OnGetAvailableMimeTypesForPortal(
      ClipboardBuffer buffer,
      std::vector<x11::Atom> uri_list_atoms,
      std::vector<x11::Atom> portal_atoms,
      PlatformClipboard::RequestDataClosure callback,
      const std::vector<std::string>& mime_types);
#endif

  const std::unique_ptr<XClipboardHelper> helper_;

  ClipboardDataChangedCallback clipboard_changed_callback_;

#if BUILDFLAG(IS_LINUX)
  base::WeakPtrFactory<X11ClipboardOzone> weak_factory_{this};
#endif
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
