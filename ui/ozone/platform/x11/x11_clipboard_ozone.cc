// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

using base::Contains;

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";
const char kClipboard[] = "CLIPBOARD";
const char kTargets[] = "TARGETS";
const char kTimestamp[] = "TIMESTAMP";

// Helps to allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
void ExpandTypes(std::vector<std::string>* list) {
  bool has_mime_type_text = Contains(*list, ui::kMimeTypeText);
  bool has_string = Contains(*list, kMimeTypeLinuxString);
  bool has_mime_type_utf8 = Contains(*list, kMimeTypeTextUtf8);
  bool has_utf8_string = Contains(*list, kMimeTypeLinuxUtf8String);
  if (has_mime_type_text && !has_string)
    list->push_back(kMimeTypeLinuxString);
  if (has_string && !has_mime_type_text)
    list->push_back(ui::kMimeTypeText);
  if (has_mime_type_utf8 && !has_utf8_string)
    list->push_back(kMimeTypeLinuxUtf8String);
  if (has_utf8_string && !has_mime_type_utf8)
    list->push_back(kMimeTypeTextUtf8);
}

}  // namespace

// Maintains state of a single selection (aka system clipboard buffer).
struct X11ClipboardOzone::SelectionState {
  SelectionState() = default;
  ~SelectionState() = default;

  // DataMap we keep from |OfferClipboardData| to service remote requests for
  // the clipboard.
  PlatformClipboard::DataMap offer_data_map;

  // DataMap from |RequestClipboardData| that we write remote clipboard
  // contents to before calling the completion callback.
  PlatformClipboard::DataMap* request_data_map = nullptr;

  // Mime types supported by remote clipboard.
  std::vector<std::string> mime_types;

  // Data most recently read from remote clipboard.
  PlatformClipboard::Data data;

  // Mime type of most recently read data from remote clipboard.
  std::string data_mime_type;

  // Callbacks are stored when we haven't already prefetched the remote
  // clipboard.
  PlatformClipboard::GetMimeTypesClosure get_available_mime_types_callback;
  PlatformClipboard::RequestDataClosure request_clipboard_data_callback;

  // The time that this instance took ownership of the clipboard.
  x11::Time acquired_selection_timestamp;
};

X11ClipboardOzone::X11ClipboardOzone()
    : atom_clipboard_(x11::GetAtom(kClipboard)),
      atom_targets_(x11::GetAtom(kTargets)),
      atom_timestamp_(x11::GetAtom(kTimestamp)),
      x_property_(x11::GetAtom(kChromeSelection)),
      connection_(x11::Connection::Get()),
      x_window_(x11::CreateDummyWindow("Chromium Clipboard Window")) {
  connection_->xfixes().QueryVersion(
      {x11::XFixes::major_version, x11::XFixes::minor_version});
  if (!connection_->xfixes().present())
    return;
  using_xfixes_ = true;

  // Register to receive standard X11 events.
  connection_->AddEventObserver(this);

  for (auto atom : {atom_clipboard_, x11::Atom::PRIMARY}) {
    // Register the selection state.
    selection_state_.emplace(atom, std::make_unique<SelectionState>());
    // Register to receive XFixes notification when selection owner changes.
    connection_->xfixes().SelectSelectionInput(
        {x_window_, atom, x11::XFixes::SelectionEventMask::SetSelectionOwner});
    // Prefetch the current remote clipboard contents.
    QueryTargets(atom);
  }
}

X11ClipboardOzone::~X11ClipboardOzone() {
  connection_->RemoveEventObserver(this);
}

void X11ClipboardOzone::OnEvent(const x11::Event& xev) {
  if (auto* request = xev.As<x11::SelectionRequestEvent>()) {
    if (request->owner == x_window_)
      OnSelectionRequest(*request);
  } else if (auto* notify = xev.As<x11::SelectionNotifyEvent>()) {
    if (notify->requestor == x_window_)
      OnSelectionNotify(*notify);
  } else if (auto* notify = xev.As<x11::XFixes::SelectionNotifyEvent>()) {
    if (notify->window == x_window_)
      OnSetSelectionOwnerNotify(*notify);
  }
}

// We are the clipboard owner, and a remote peer has requested either:
// TARGETS: List of mime types that we support for the clipboard.
// TIMESTAMP: Time when we took ownership of the clipboard.
// <mime-type>: Mime type to receive clipboard as.
void X11ClipboardOzone::OnSelectionRequest(
    const x11::SelectionRequestEvent& event) {
  // The property must be set.
  if (event.property == x11::Atom::None)
    return;

  // target=TARGETS.
  auto& selection_state =
      GetSelectionState(static_cast<x11::Atom>(event.selection));
  auto target = static_cast<x11::Atom>(event.target);
  PlatformClipboard::DataMap& offer_data_map = selection_state.offer_data_map;
  if (target == atom_targets_) {
    std::vector<std::string> targets;
    // Add TIMESTAMP.
    targets.push_back(kTimestamp);
    for (auto& entry : offer_data_map) {
      targets.push_back(entry.first);
    }
    // Expand types, then convert from string to atom.
    ExpandTypes(&targets);
    std::vector<x11::Atom> atoms;
    for (auto& entry : targets)
      atoms.push_back(x11::GetAtom(entry.c_str()));
    SetArrayProperty(event.requestor, event.property, x11::Atom::ATOM, atoms);

  } else if (target == atom_timestamp_) {
    // target=TIMESTAMP.
    SetProperty(event.requestor, event.property, x11::Atom::INTEGER,
                selection_state.acquired_selection_timestamp);
  } else {
    // Send clipboard data.
    std::string target_name;
    if (auto reply = connection_->GetAtomName({event.target}).Sync())
      target_name = std::move(reply->name);

    std::string key = target_name;
    // Allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
    if (key == kMimeTypeLinuxUtf8String &&
        !Contains(offer_data_map, kMimeTypeLinuxUtf8String)) {
      key = kMimeTypeTextUtf8;
    } else if (key == kMimeTypeLinuxString &&
               !Contains(offer_data_map, kMimeTypeLinuxString)) {
      key = kMimeTypeText;
    }
    auto it = offer_data_map.find(key);
    if (it != offer_data_map.end()) {
      SetArrayProperty(event.requestor, event.property, event.target,
                       it->second->data());
    }
  }

  // Notify remote peer that clipboard has been sent.
  x11::SelectionNotifyEvent selection_event{
      .time = event.time,
      .requestor = event.requestor,
      .selection = event.selection,
      .target = event.target,
      .property = event.property,
  };
  x11::SendEvent(selection_event, selection_event.requestor,
                 x11::EventMask::NoEvent);
}

// A remote peer owns the clipboard.  This event is received in response to
// our request for TARGETS (GetAvailableMimeTypes), or a specific mime type
// (RequestClipboardData).
void X11ClipboardOzone::OnSelectionNotify(
    const x11::SelectionNotifyEvent& event) {
  // GetAvailableMimeTypes.
  auto selection = static_cast<x11::Atom>(event.selection);
  auto& selection_state = GetSelectionState(selection);
  if (static_cast<x11::Atom>(event.target) == atom_targets_) {
    std::vector<x11::Atom> targets;
    GetArrayProperty(x_window_, x_property_, &targets);

    selection_state.mime_types.clear();
    for (auto target : targets) {
      if (auto reply = connection_->GetAtomName({target}).Sync())
        selection_state.mime_types.push_back(std::move(reply->name));
    }

    // If we have a saved callback, invoke it now with expanded types, otherwise
    // guess that we will want 'text/plain' and fetch it now.
    if (selection_state.get_available_mime_types_callback) {
      std::vector<std::string> result(selection_state.mime_types);
      ExpandTypes(&result);
      std::move(selection_state.get_available_mime_types_callback)
          .Run(std::move(result));
    } else {
      selection_state.data_mime_type = kMimeTypeText;
      ReadRemoteClipboard(selection);
    }
  }

  // RequestClipboardData.
  if (static_cast<x11::Atom>(event.property) == x_property_) {
    x11::Atom type;
    std::vector<uint8_t> data;
    GetArrayProperty(x_window_, x_property_, &data, &type);
    x11::DeleteProperty(x_window_, x_property_);
    if (type != x11::Atom::None)
      selection_state.data = scoped_refptr<base::RefCountedBytes>(
          base::RefCountedBytes::TakeVector(&data));

    // If we have a saved callback, invoke it now, otherwise this was a prefetch
    // and we have already saved |data_| for the next call to
    // |RequestClipboardData|.
    if (selection_state.request_clipboard_data_callback) {
      selection_state.request_data_map->emplace(selection_state.data_mime_type,
                                                selection_state.data);
      std::move(selection_state.request_clipboard_data_callback)
          .Run(selection_state.data);
    }
  } else if (static_cast<x11::Atom>(event.property) == x11::Atom::None &&
             selection_state.request_clipboard_data_callback) {
    // If the remote peer could not send data in the format we requested,
    // or failed for any reason, we will send empty data.
    std::move(selection_state.request_clipboard_data_callback)
        .Run(selection_state.data);
  }
}

void X11ClipboardOzone::OnSetSelectionOwnerNotify(
    const x11::XFixes::SelectionNotifyEvent& event) {
  // Reset state and fetch remote clipboard if there is a new remote owner.
  x11::Atom selection = event.selection;
  if (!IsSelectionOwner(BufferForSelectionAtom(selection))) {
    auto& selection_state = GetSelectionState(selection);
    selection_state.mime_types.clear();
    selection_state.data_mime_type.clear();
    selection_state.data.reset();
    QueryTargets(selection);
  }

  // Increase the sequence number if the callback is set.
  if (update_sequence_cb_)
    update_sequence_cb_.Run(BufferForSelectionAtom(selection));
}

x11::Atom X11ClipboardOzone::SelectionAtomForBuffer(
    ClipboardBuffer buffer) const {
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste:
      return atom_clipboard_;
    case ClipboardBuffer::kSelection:
      return x11::Atom::PRIMARY;
    default:
      NOTREACHED();
      return x11::Atom::None;
  }
}

ClipboardBuffer X11ClipboardOzone::BufferForSelectionAtom(
    x11::Atom selection) const {
  if (selection == x11::Atom::PRIMARY)
    return ClipboardBuffer::kSelection;
  if (selection == atom_clipboard_)
    return ClipboardBuffer::kCopyPaste;
  NOTREACHED();
  return ClipboardBuffer::kCopyPaste;
}

X11ClipboardOzone::SelectionState& X11ClipboardOzone::GetSelectionState(
    x11::Atom selection) {
  DCHECK(Contains(selection_state_, selection));
  return *selection_state_[selection];
}

void X11ClipboardOzone::QueryTargets(x11::Atom selection) {
  GetSelectionState(selection).mime_types.clear();
  connection_->ConvertSelection({x_window_, selection, atom_targets_,
                                 x_property_, x11::Time::CurrentTime});
}

void X11ClipboardOzone::ReadRemoteClipboard(x11::Atom selection) {
  auto& selection_state = GetSelectionState(selection);
  selection_state.data.reset();
  // Allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
  std::string target = selection_state.data_mime_type;
  if (!Contains(selection_state.mime_types, target)) {
    if (target == kMimeTypeText) {
      target = kMimeTypeLinuxString;
    } else if (target == kMimeTypeTextUtf8) {
      target = kMimeTypeLinuxUtf8String;
    }
  }

  connection_->ConvertSelection({x_window_, selection, x11::GetAtom(target),
                                 x_property_, x11::Time::CurrentTime});
}

void X11ClipboardOzone::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  const x11::Atom selection = SelectionAtomForBuffer(buffer);
  auto& selection_state = GetSelectionState(selection);
  const auto timestamp =
      static_cast<x11::Time>(X11EventSource::GetInstance()->GetTimestamp());
  selection_state.acquired_selection_timestamp = timestamp;
  selection_state.offer_data_map = data_map;
  // Only take ownership if we are using xfixes.
  // TODO(joelhockey): Make clipboard work without xfixes.
  if (using_xfixes_) {
    connection_->SetSelectionOwner({x_window_, selection, timestamp});
  }
  std::move(callback).Run();
}

void X11ClipboardOzone::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  const x11::Atom selection = SelectionAtomForBuffer(buffer);
  auto& selection_state = GetSelectionState(selection);
  // If we are not using xfixes, return empty data.
  // TODO(joelhockey): Make clipboard work without xfixes.
  // If we have already prefetched the clipboard for the correct mime type,
  // then send it right away, otherwise save the callback and attempt to get the
  // requested mime type from the remote clipboard.
  if (!using_xfixes_ ||
      (selection_state.data_mime_type == mime_type && selection_state.data &&
       !selection_state.data->data().empty())) {
    data_map->emplace(mime_type, selection_state.data);
    std::move(callback).Run(selection_state.data);
    return;
  }

  // If we know the available mime types, and it is not this, send empty now.
  if (!selection_state.mime_types.empty() &&
      !Contains(selection_state.mime_types, mime_type)) {
    std::move(callback).Run(PlatformClipboard::Data());
    return;
  }

  selection_state.data_mime_type = mime_type;
  selection_state.request_data_map = data_map;
  DCHECK(selection_state.request_clipboard_data_callback.is_null());
  selection_state.request_clipboard_data_callback = std::move(callback);
  ReadRemoteClipboard(selection);
}

void X11ClipboardOzone::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  const x11::Atom selection = SelectionAtomForBuffer(buffer);
  auto& selection_state = GetSelectionState(selection);
  // If we are not using xfixes, return empty data.
  // TODO(joelhockey): Make clipboard work without xfixes.
  // If we already have the list of supported mime types, send the expanded list
  // of types right away, otherwise save the callback and get the list of
  // TARGETS from the remote clipboard.
  if (!using_xfixes_ || !selection_state.mime_types.empty()) {
    std::vector<std::string> result(selection_state.mime_types);
    ExpandTypes(&result);
    std::move(callback).Run(std::move(result));
    return;
  }
  DCHECK(selection_state.get_available_mime_types_callback.is_null());
  selection_state.get_available_mime_types_callback = std::move(callback);
  QueryTargets(selection);
}

bool X11ClipboardOzone::IsSelectionOwner(ClipboardBuffer buffer) {
  // If we are not using xfixes, then we are always the owner.
  // TODO(joelhockey): Make clipboard work without xfixes.
  if (!using_xfixes_)
    return true;

  auto reply =
      connection_->GetSelectionOwner({SelectionAtomForBuffer(buffer)}).Sync();
  return reply && reply->owner == x_window_;
}

void X11ClipboardOzone::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  update_sequence_cb_ = std::move(cb);
}

bool X11ClipboardOzone::IsSelectionBufferAvailable() const {
  return true;
}

}  // namespace ui
