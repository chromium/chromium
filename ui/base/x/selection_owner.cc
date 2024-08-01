// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/x/selection_owner.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

const char kIncr[] = "INCR";
const char kSaveTargets[] = "SAVE_TARGETS";
const char kTargets[] = "TARGETS";

namespace {

const char kAtomPair[] = "ATOM_PAIR";
const char kMultiple[] = "MULTIPLE";
const char kTimestamp[] = "TIMESTAMP";

// The period of |incremental_transfer_abort_timer_|. Arbitrary but must be <=
// than kIncrementalTransferTimeoutMs.
const int KSelectionOwnerTimerPeriodMs = 1000;

// The amount of time to wait for the selection requestor to process the data
// sent by the selection owner before aborting an incremental data transfer.
const int kIncrementalTransferTimeoutMs = 10000;

static_assert(KSelectionOwnerTimerPeriodMs <= kIncrementalTransferTimeoutMs,
              "timer period must be <= transfer timeout");

size_t GetMaxIncrementalTransferSize() {
  ssize_t size = x11::Connection::Get()->MaxRequestSizeInBytes();
  // Conservatively subtract 100 bytes for the GetProperty request, padding etc.
  DCHECK_GT(size, 100);
  return std::min<size_t>(size - 100, 0x100000);
}

// Gets the value of an atom pair array property. On success, true is returned
// and the value is stored in |value|.
bool GetAtomPairArrayProperty(
    x11::Connection& connection,
    x11::Window window,
    x11::Atom property,
    std::vector<std::pair<x11::Atom, x11::Atom>>* value) {
  std::vector<x11::Atom> atoms;
  // Since this is an array of atom pairs, ensure ensure |atoms|
  // has an element count that's a multiple of 2.
  if (!connection.GetArrayProperty(window, property, &atoms) ||
      atoms.size() % 2 != 0) {
    return false;
  }

  value->clear();
  for (size_t i = 0; i < atoms.size(); i += 2) {
    value->push_back(std::make_pair(atoms[i], atoms[i + 1]));
  }
  return true;
}

x11::Window GetSelectionOwner(x11::Atom selection) {
  auto response = x11::Connection::Get()->GetSelectionOwner({selection}).Sync();
  return response ? response->owner : x11::Window::None;
}

void SetSelectionOwner(x11::Window window,
                       x11::Atom selection,
                       x11::Time time = x11::Time::CurrentTime) {
  x11::Connection::Get()->SetSelectionOwner({window, selection, time});
}

}  // namespace

SelectionOwner::SelectionOwner(x11::Connection& connection,
                               x11::Window x_window,
                               x11::Atom selection_name)
    : connection_(connection),
      x_window_(x_window),
      selection_name_(selection_name) {}

SelectionOwner::~SelectionOwner() {
  // If we are the selection owner, we need to release the selection so we
  // don't receive further events. However, we don't call ClearSelectionOwner()
  // because we don't want to do this indiscriminately.
  if (GetSelectionOwner(selection_name_) == x_window_) {
    SetSelectionOwner(x11::Window::None, selection_name_);
  }
}

void SelectionOwner::RetrieveTargets(std::vector<x11::Atom>* targets) {
  for (const auto& format_target : format_map_) {
    targets->push_back(format_target.first);
  }
}

void SelectionOwner::TakeOwnershipOfSelection(const SelectionFormatMap& data) {
  acquired_selection_timestamp_ = X11EventSource::GetInstance()->GetTimestamp();
  SetSelectionOwner(x_window_, selection_name_, acquired_selection_timestamp_);

  if (GetSelectionOwner(selection_name_) == x_window_) {
    // The X server agrees that we are the selection owner. Commit our data.
    format_map_ = data;
  }
}

void SelectionOwner::ClearSelectionOwner() {
  SetSelectionOwner(x11::Window::None, selection_name_);
  format_map_ = SelectionFormatMap();
}

void SelectionOwner::OnSelectionRequest(
    const x11::SelectionRequestEvent& request) {
  auto requestor = request.requestor;
  x11::Atom requested_target = request.target;
  x11::Atom requested_property = request.property;

  // Incrementally build our selection. By default this is a refusal, and we'll
  // override the parts indicating success in the different cases.
  x11::SelectionNotifyEvent reply{
      .time = request.time,
      .requestor = requestor,
      .selection = request.selection,
      .target = requested_target,
      .property = x11::Atom::None,  // Indicates failure
  };

  if (requested_target == x11::GetAtom(kMultiple)) {
    // The contents of |requested_property| should be a list of
    // <target,property> pairs.
    std::vector<std::pair<x11::Atom, x11::Atom>> conversions;
    if (GetAtomPairArrayProperty(connection_.get(), requestor,
                                 requested_property, &conversions)) {
      std::vector<x11::Atom> conversion_results;
      for (const std::pair<x11::Atom, x11::Atom>& conversion : conversions) {
        bool conversion_successful =
            ProcessTarget(conversion.first, requestor, conversion.second);
        conversion_results.push_back(conversion.first);
        conversion_results.push_back(conversion_successful ? conversion.second
                                                           : x11::Atom::None);
      }

      // Set the property to indicate which conversions succeeded. This matches
      // what GTK does.
      connection_->SetArrayProperty(requestor, requested_property,
                                    x11::GetAtom(kAtomPair),
                                    conversion_results);

      reply.property = requested_property;
    }
  } else {
    if (ProcessTarget(requested_target, requestor, requested_property)) {
      reply.property = requested_property;
    }
  }

  // Send off the reply.
  connection_->SendEvent(reply, requestor, x11::EventMask::NoEvent);
}

void SelectionOwner::OnSelectionClear(const x11::SelectionClearEvent& event) {
  DVLOG(1) << "SelectionClear";

  // TODO(erg): If we receive a SelectionClear event while we're handling data,
  // we need to delay clearing.
}

bool SelectionOwner::CanDispatchPropertyEvent(
    const x11::PropertyNotifyEvent& event) {
  return event.state == x11::Property::Delete &&
         FindIncrementalTransferForEvent(event) != incremental_transfers_.end();
}

void SelectionOwner::OnPropertyEvent(const x11::PropertyNotifyEvent& event) {
  auto it = FindIncrementalTransferForEvent(event);
  if (it == incremental_transfers_.end()) {
    return;
  }

  ProcessIncrementalTransfer(&(*it));
  if (!it->data.get()) {
    CompleteIncrementalTransfer(it);
  }
}

bool SelectionOwner::ProcessTarget(x11::Atom target,
                                   x11::Window requestor,
                                   x11::Atom property) {
  x11::Atom multiple_atom = x11::GetAtom(kMultiple);
  x11::Atom save_targets_atom = x11::GetAtom(kSaveTargets);
  x11::Atom targets_atom = x11::GetAtom(kTargets);
  x11::Atom timestamp_atom = x11::GetAtom(kTimestamp);

  if (target == multiple_atom || target == save_targets_atom) {
    return false;
  }

  if (target == timestamp_atom) {
    connection_->SetProperty(requestor, property, x11::Atom::INTEGER,
                             acquired_selection_timestamp_);
    return true;
  }

  if (target == targets_atom) {
    // We have been asked for TARGETS. Send an atom array back with the data
    // types we support.
    std::vector<x11::Atom> targets = {timestamp_atom, targets_atom,
                                      save_targets_atom, multiple_atom};
    RetrieveTargets(&targets);

    connection_->SetArrayProperty(requestor, property, x11::Atom::ATOM,
                                  targets);
    return true;
  }

  // Try to find the data type in map.
  auto it = format_map_.find(target);
  if (it != format_map_.end()) {
    if (it->second->size() > GetMaxIncrementalTransferSize()) {
      // We must send the data back in several chunks due to a limitation in
      // the size of X requests. Notify the selection requestor that the data
      // will be sent incrementally by returning data of type "INCR".
      uint32_t length = it->second->size();
      connection_->SetProperty(requestor, property, x11::GetAtom(kIncr),
                               length);

      // Wait for the selection requestor to indicate that it has processed
      // the selection result before sending the first chunk of data. The
      // selection requestor indicates this by deleting |property|.
      base::TimeTicks timeout =
          base::TimeTicks::Now() +
          base::Milliseconds(kIncrementalTransferTimeoutMs);
      incremental_transfers_.emplace_back(
          requestor, target, property,
          connection_->ScopedSelectEvent(requestor,
                                         x11::EventMask::PropertyChange),
          it->second, 0, timeout);

      // Start a timer to abort the data transfer in case that the selection
      // requestor does not support the INCR property or gets destroyed during
      // the data transfer.
      if (!incremental_transfer_abort_timer_.IsRunning()) {
        incremental_transfer_abort_timer_.Start(
            FROM_HERE, base::Milliseconds(KSelectionOwnerTimerPeriodMs), this,
            &SelectionOwner::AbortStaleIncrementalTransfers);
      }
    } else {
      auto& mem = it->second;
      std::vector<uint8_t> data(mem->data(), mem->data() + mem->size());
      connection_->SetArrayProperty(requestor, property, target, data);
    }
    return true;
  }

  // I would put error logging here, but GTK ignores TARGETS and spams us
  // looking for its own internal types.
  return false;
}

void SelectionOwner::ProcessIncrementalTransfer(IncrementalTransfer* transfer) {
  size_t remaining = transfer->data->size() - transfer->offset;
  size_t chunk_length = std::min(remaining, GetMaxIncrementalTransferSize());
  const uint8_t* data = transfer->data->data() + transfer->offset;
  std::vector<uint8_t> buf(data, data + chunk_length);
  connection_->SetArrayProperty(transfer->window, transfer->property,
                                transfer->target, buf);
  transfer->offset += chunk_length;
  transfer->timeout = base::TimeTicks::Now() +
                      base::Milliseconds(kIncrementalTransferTimeoutMs);

  // When offset == data->size(), we still need to transfer a zero-sized chunk
  // to notify the selection requestor that the transfer is complete. Clear
  // transfer->data once the zero-sized chunk is sent to indicate that state
  // related to this data transfer can be cleared.
  if (chunk_length == 0) {
    transfer->data = nullptr;
  }
}

void SelectionOwner::AbortStaleIncrementalTransfers() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (int i = static_cast<int>(incremental_transfers_.size()) - 1; i >= 0;
       --i) {
    if (incremental_transfers_[i].timeout <= now) {
      CompleteIncrementalTransfer(incremental_transfers_.begin() + i);
    }
  }
}

void SelectionOwner::CompleteIncrementalTransfer(
    std::vector<IncrementalTransfer>::iterator it) {
  incremental_transfers_.erase(it);

  if (incremental_transfers_.empty()) {
    incremental_transfer_abort_timer_.Stop();
  }
}

std::vector<SelectionOwner::IncrementalTransfer>::iterator
SelectionOwner::FindIncrementalTransferForEvent(
    const x11::PropertyNotifyEvent& prop) {
  for (auto it = incremental_transfers_.begin();
       it != incremental_transfers_.end(); ++it) {
    if (it->window == prop.window && it->property == prop.atom) {
      return it;
    }
  }
  return incremental_transfers_.end();
}

SelectionOwner::IncrementalTransfer::IncrementalTransfer(
    x11::Window window,
    x11::Atom target,
    x11::Atom property,
    x11::ScopedEventSelector event_selector,
    const scoped_refptr<base::RefCountedMemory>& data,
    int offset,
    base::TimeTicks timeout)
    : window(window),
      target(target),
      property(property),
      event_selector(std::move(event_selector)),
      data(data),
      offset(offset),
      timeout(timeout) {}

SelectionOwner::IncrementalTransfer::IncrementalTransfer(
    IncrementalTransfer&& other) = default;

SelectionOwner::IncrementalTransfer&
SelectionOwner::IncrementalTransfer::operator=(IncrementalTransfer&&) = default;

SelectionOwner::IncrementalTransfer::~IncrementalTransfer() = default;

}  // namespace ui
