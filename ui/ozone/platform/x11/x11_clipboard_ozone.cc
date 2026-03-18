// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/x/x11_clipboard_helper.h"

#if BUILDFLAG(IS_LINUX)
#include "base/strings/string_view_util.h"
#include "ui/base/clipboard/clipboard_util_linux.h"
#include "ui/gfx/x/atom_cache.h"
#endif

namespace ui {

X11ClipboardOzone::X11ClipboardOzone()
    : helper_(std::make_unique<XClipboardHelper>(
          base::BindRepeating(&X11ClipboardOzone::OnSelectionChanged,
                              base::Unretained(this)))) {
  DCHECK(helper_);
}

X11ClipboardOzone::~X11ClipboardOzone() = default;

void X11ClipboardOzone::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map) {
  helper_->CreateNewClipboardData();
  for (const auto& item : data_map)
    helper_->InsertMapping(item.first, item.second);

  helper_->TakeOwnershipOfSelection(buffer);
}

void X11ClipboardOzone::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  DCHECK(!callback.is_null());

#if BUILDFLAG(IS_LINUX)
  if (mime_type == kMimeTypeUriList) {
    auto uri_list_atoms = helper_->GetAtomsForFormat(
        ClipboardFormatType::CustomPlatformType(kMimeTypeUriList));
    std::vector<x11::Atom> portal_atoms = {
        x11::GetAtom(kMimeTypePortalFileTransfer),
        x11::GetAtom(kMimeTypePortalFiles)};

    helper_->GetAvailableMimeTypesAsync(
        buffer,
        base::BindOnce(&X11ClipboardOzone::OnGetAvailableMimeTypesForPortal,
                       weak_factory_.GetWeakPtr(), buffer,
                       std::move(uri_list_atoms), std::move(portal_atoms),
                       std::move(callback)));
    return;
  }
#endif

  auto atoms = mime_type == kMimeTypePlainText
                   ? helper_->GetTextAtoms()
                   : helper_->GetAtomsForFormat(
                         ClipboardFormatType::CustomPlatformType(mime_type));
  helper_->ReadAsync(buffer, atoms,
                     base::BindOnce(
                         [](PlatformClipboard::RequestDataClosure callback,
                            SelectionData selection_data) {
                           std::move(callback).Run(selection_data.TakeBytes());
                         },
                         std::move(callback)));
}

#if BUILDFLAG(IS_LINUX)
void X11ClipboardOzone::OnPortalKeyRead(
    PlatformClipboard::RequestDataClosure callback,
    SelectionData selection_data) {
  auto bytes = selection_data.TakeBytes();
  if (!bytes) {
    std::move(callback).Run(nullptr);
    return;
  }

  ui::clipboard_util::ExtractPathsFromPortalKey(
      base::as_byte_span(*bytes),
      base::BindOnce(&X11ClipboardOzone::OnPathsExtracted,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void X11ClipboardOzone::OnPathsExtracted(
    PlatformClipboard::RequestDataClosure callback,
    std::vector<std::string> paths) {
  if (paths.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string uri_list = ui::clipboard_util::GetUriListFromPaths(paths);
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedBytes>(
      base::as_byte_span(uri_list)));
}
#endif

void X11ClipboardOzone::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  DCHECK(!callback.is_null());
  helper_->GetAvailableMimeTypesAsync(buffer, std::move(callback));
}

void X11ClipboardOzone::IsSelectionOwner(ClipboardBuffer buffer,
                                         IsSelectionOwnerClosure callback) {
  helper_->IsSelectionOwnerAsync(buffer, std::move(callback));
}

void X11ClipboardOzone::SetClipboardDataChangedCallback(
    ClipboardDataChangedCallback data_changed_callback) {
  clipboard_changed_callback_ = std::move(data_changed_callback);
}

bool X11ClipboardOzone::IsSelectionBufferAvailable() const {
  return true;
}

void X11ClipboardOzone::OnSelectionChanged(ClipboardBuffer buffer) {
  if (clipboard_changed_callback_)
    clipboard_changed_callback_.Run(buffer);
}

#if BUILDFLAG(IS_LINUX)
void X11ClipboardOzone::OnGetAvailableMimeTypesForPortal(
    ClipboardBuffer buffer,
    std::vector<x11::Atom> uri_list_atoms,
    std::vector<x11::Atom> portal_atoms,
    PlatformClipboard::RequestDataClosure callback,
    const std::vector<std::string>& mime_types) {
  bool has_portal = false;
  for (const auto& mt : mime_types) {
    if (mt == kMimeTypePortalFileTransfer || mt == kMimeTypePortalFiles) {
      has_portal = true;
    }
  }

  if (has_portal) {
    helper_->ReadAsync(
        buffer, portal_atoms,
        base::BindOnce(&X11ClipboardOzone::OnPortalKeyRead,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    helper_->ReadAsync(
        buffer, uri_list_atoms,
        base::BindOnce(
            [](PlatformClipboard::RequestDataClosure callback,
               SelectionData selection_data) {
              std::move(callback).Run(selection_data.TakeBytes());
            },
            std::move(callback)));
  }
}
#endif

}  // namespace ui
