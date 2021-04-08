// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/x/x11_clipboard_helper.h"

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
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  DCHECK(!callback.is_null());
  helper_->CreateNewClipboardData();
  for (const auto& item : data_map)
    helper_->InsertMapping(item.first, item.second);
  helper_->TakeOwnershipOfSelection(buffer);
  std::move(callback).Run();
}

void X11ClipboardOzone::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  DCHECK(!callback.is_null());
  auto atoms =
      mime_type == kMimeTypeText
          ? helper_->GetTextAtoms()
          : helper_->GetAtomsForFormat(ClipboardFormatType::GetType(mime_type));
  auto selection_data = helper_->Read(buffer, atoms);
  std::move(callback).Run(selection_data.TakeBytes());
}

void X11ClipboardOzone::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  DCHECK(!callback.is_null());
  auto available_types = helper_->GetAvailableAtomNames(buffer);
  std::move(callback).Run(available_types);
}

bool X11ClipboardOzone::IsSelectionOwner(ClipboardBuffer buffer) {
  return helper_->IsSelectionOwner(buffer);
}

void X11ClipboardOzone::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  update_sequence_cb_ = std::move(cb);
}

bool X11ClipboardOzone::IsSelectionBufferAvailable() const {
  return true;
}

void X11ClipboardOzone::OnSelectionChanged(ClipboardBuffer buffer) {
  if (update_sequence_cb_)
    update_sequence_cb_.Run(buffer);
}

}  // namespace ui
