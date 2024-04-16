// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
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
  // This is the only function clients may use to request available formats, so
  // include both standard and platform-specific (atom names) values.
  // TODO(crbug.com/40054419): Consider adding a way of filtering mime types and
  // querying availability of specific formats, so implementations can optimize
  // it, if possible. E.g: Avoid multiple roundtrips to check if a given format
  // is available. See ClipboardX11::IsFormatAvailable for example.
  auto types = helper_->GetAvailableTypes(buffer);
  auto atoms = helper_->GetAvailableAtomNames(buffer);
  std::set<std::string> uniq(types.begin(), types.end());
  uniq.insert(atoms.begin(), atoms.end());
  std::move(callback).Run({uniq.begin(), uniq.end()});
}

bool X11ClipboardOzone::IsSelectionOwner(ClipboardBuffer buffer) {
  return helper_->IsSelectionOwner(buffer);
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
