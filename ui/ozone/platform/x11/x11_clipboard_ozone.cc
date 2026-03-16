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

#if BUILDFLAG(IS_LINUX)
  auto it = data_map.find(kMimeTypeUriList);
  if (it != data_map.end()) {
    std::string unparsed(base::as_string_view(*it->second));
    std::vector<std::string> paths =
        ui::clipboard_util::GetPathsFromUriList(unparsed);

    std::string key = ui::clipboard_util::RegisterPathsWithPortal(paths);
    if (!key.empty()) {
      auto data_bytes =
          base::MakeRefCounted<base::RefCountedBytes>(base::as_byte_span(key));
      helper_->InsertMapping(kMimeTypePortalFileTransfer, data_bytes);
      helper_->InsertMapping(kMimeTypePortalFiles, data_bytes);
    }
  }
#endif

  helper_->TakeOwnershipOfSelection(buffer);
}

void X11ClipboardOzone::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  DCHECK(!callback.is_null());
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

}  // namespace ui
