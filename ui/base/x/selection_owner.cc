// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_owner.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

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

// Returns a conservative max size of the data we can pass into
// XChangeProperty(). Copied from GTK.
size_t GetMaxRequestSize(x11::Connection* connection) {
  long extended_max_size = connection->extended_max_request_length();
  long max_size =
      (extended_max_size ? extended_max_size
                         : connection->setup().maximum_request_length) -
      100;
  return std::min(static_cast<long>(0x40000),
                  std::max(static_cast<long>(0), max_size));
}

// Gets the value of an atom pair array property. On success, true is returned
// and the value is stored in |value|.
bool GetAtomPairArrayProperty(
    x11::Window window,
    x11::Atom property,
    std::vector<std::pair<x11::Atom, x11::Atom>>* value) {
  std::vector<x11::Atom> atoms;
  // Since this is an array of atom pairs, ensure ensure |atoms|
  // has an element count that's a multiple of 2.
  if (!ui::GetArrayProperty(window, property, &atoms) || atoms.size() % 2 != 0)
    return false;

  value->clear();
  for (size_t i = 0; i < atoms.size(); i += 2)
    value->push_back(std::make_pair(atoms[i], atoms[i + 1]));
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

SelectionOwner::SelectionOwner(x11::Connection* connection,
                               x11::Window x_window,
                               x11::Atom selection_name)
    : x_window_(x_window),
      selection_name_(selection_name),
      max_request_size_(GetMaxRequestSize(connection)) {}

SelectionOwner::~SelectionOwner() {
  // If we are the selection owner, we need to release the selection so we
  // don't receive further events. However, we don't call ClearSelectionOwner()
  // because we don't want to do this indiscriminately.
  if (GetSelectionOwner(selection_name_) == x_window_)
    SetSelectionOwner(x11::Window::None, selection_name_);
}

void SelectionOwner::RetrieveTargets(std::vector<x11::Atom>* targets) {
  for (const auto& format_target : format_map_)
    targets->push_back(format_target.first);
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

void SelectionOwner::OnSelectionRequest(const x11::Event& x11_event) {
  auto& request = *x11_event.As<x11::SelectionRequestEvent>();
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

  if (requested_target == gfx::GetAtom(kMultiple)) {
    // The contents of |requested_property| should be a list of
    // <target,property> pairs.
    std::vector<std::pair<x11::Atom, x11::Atom>> conversions;
    if (GetAtomPairArrayProperty(requestor, requested_property, &conversions)) {
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
      ui::SetArrayProperty(requestor, requested_property,
                           gfx::GetAtom(kAtomPair), conversion_results);

      reply.property = requested_property;
    }
  } else {
    if (ProcessTarget(requested_target, requestor, requested_property))
      reply.property = requested_property;
  }

  // Send off the reply.
  x11::SendEvent(reply, requestor, x11::EventMask::NoEvent);
}

void SelectionOwner::OnSelectionClear(const x11::Event& event) {
  DLOG(ERROR) << "SelectionClear";

  // TODO(erg): If we receive a SelectionClear event while we're handling data,
  // we need to delay clearing.
}

bool SelectionOwner::CanDispatchPropertyEvent(const x11::Event& event) {
  return event.As<x11::PropertyNotifyEvent>()->state == x11::Property::Delete &&
         FindIncrementalTransferForEvent(event) != incremental_transfers_.end();
}

void SelectionOwner::OnPropertyEvent(const x11::Event& event) {
  auto it = FindIncrementalTransferForEvent(event);
  if (it == incremental_transfers_.end())
    return;

  ProcessIncrementalTransfer(&(*it));
  if (!it->data.get())
    CompleteIncrementalTransfer(it);
}

bool SelectionOwner::ProcessTarget(x11::Atom target,
                                   x11::Window requestor,
                                   x11::Atom property) {
  x11::Atom multiple_atom = gfx::GetAtom(kMultiple);
  x11::Atom save_targets_atom = gfx::GetAtom(kSaveTargets);
  x11::Atom targets_atom = gfx::GetAtom(kTargets);
  x11::Atom timestamp_atom = gfx::GetAtom(kTimestamp);

  if (target == multiple_atom || target == save_targets_atom)
    return false;

  if (target == timestamp_atom) {
    ui::SetProperty(requestor, property, x11::Atom::INTEGER,
                    acquired_selection_timestamp_);
    return true;
  }

  if (target == targets_atom) {
    // We have been asked for TARGETS. Send an atom array back with the data
    // types we support.
    std::vector<x11::Atom> targets = {timestamp_atom, targets_atom,
                                      save_targets_atom, multiple_atom};
    RetrieveTargets(&targets);

    ui::SetArrayProperty(requestor, property, x11::Atom::ATOM, targets);
    return true;
  }

  // Try to find the data type in map.
  auto it = format_map_.find(target);
  if (it != format_map_.end()) {
    if (it->second->size() > max_request_size_) {
      // We must send the data back in several chunks due to a limitation in
      // the size of X requests. Notify the selection requestor that the data
      // will be sent incrementally by returning data of type "INCR".
      uint32_t length = it->second->size();
      ui::SetProperty(requestor, property, gfx::GetAtom(kIncr), length);

      // Wait for the selection requestor to indicate that it has processed
      // the selection result before sending the first chunk of data. The
      // selection requestor indicates this by deleting |property|.
      base::TimeTicks timeout =
          base::TimeTicks::Now() +
          base::TimeDelta::FromMilliseconds(kIncrementalTransferTimeoutMs);
      incremental_transfers_.emplace_back(
          requestor, target, property,
          std::make_unique<XScopedEventSelector>(
              requestor, x11::EventMask::PropertyChange),
          it->second, 0, timeout);

      // Start a timer to abort the data transfer in case that the selection
      // requestor does not support the INCR property or gets destroyed during
      // the data transfer.
      if (!incremental_transfer_abort_timer_.IsRunning()) {
        incremental_transfer_abort_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(KSelectionOwnerTimerPeriodMs),
            this, &SelectionOwner::AbortStaleIncrementalTransfers);
      }
    } else {
      auto& mem = it->second;
      std::vector<uint8_t> data(mem->data(), mem->data() + mem->size());
      ui::SetArrayProperty(requestor, property, target, data);
    }
    return true;
  }

  // I would put error logging here, but GTK ignores TARGETS and spams us
  // looking for its own internal types.
  return false;
}

void SelectionOwner::ProcessIncrementalTransfer(IncrementalTransfer* transfer) {
  size_t remaining = transfer->data->size() - transfer->offset;
  size_t chunk_length = std::min(remaining, max_request_size_);
  const uint8_t* data = transfer->data->front() + transfer->offset;
  std::vector<uint8_t> buf(data, data + chunk_length);
  ui::SetArrayProperty(transfer->window, transfer->property, transfer->target,
                       buf);
  transfer->offset += chunk_length;
  transfer->timeout =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kIncrementalTransferTimeoutMs);

  // When offset == data->size(), we still need to transfer a zero-sized chunk
  // to notify the selection requestor that the transfer is complete. Clear
  // transfer->data once the zero-sized chunk is sent to indicate that state
  // related to this data transfer can be cleared.
  if (chunk_length == 0)
    transfer->data = nullptr;
}

void SelectionOwner::AbortStaleIncrementalTransfers() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (int i = static_cast<int>(incremental_transfers_.size()) - 1; i >= 0;
       --i) {
    if (incremental_transfers_[i].timeout <= now)
      CompleteIncrementalTransfer(incremental_transfers_.begin() + i);
  }
}

void SelectionOwner::CompleteIncrementalTransfer(
    std::vector<IncrementalTransfer>::iterator it) {
  incremental_transfers_.erase(it);

  if (incremental_transfers_.empty())
    incremental_transfer_abort_timer_.Stop();
}

std::vector<SelectionOwner::IncrementalTransfer>::iterator
SelectionOwner::FindIncrementalTransferForEvent(const x11::Event& event) {
  for (auto it = incremental_transfers_.begin();
       it != incremental_transfers_.end(); ++it) {
    const auto* prop = event.As<x11::PropertyNotifyEvent>();
    if (it->window == prop->window && it->property == prop->atom)
      return it;
  }
  return incremental_transfers_.end();
}

SelectionOwner::IncrementalTransfer::IncrementalTransfer(
    x11::Window window,
    x11::Atom target,
    x11::Atom property,
    std::unique_ptr<XScopedEventSelector> event_selector,
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
