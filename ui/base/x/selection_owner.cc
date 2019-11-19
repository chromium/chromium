// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_owner.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

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
size_t GetMaxRequestSize(XDisplay* display) {
  long extended_max_size = XExtendedMaxRequestSize(display);
  long max_size =
      (extended_max_size ? extended_max_size : XMaxRequestSize(display)) - 100;
  return std::min(static_cast<long>(0x40000),
                  std::max(static_cast<long>(0), max_size));
}

// Gets the value of an atom pair array property. On success, true is returned
// and the value is stored in |value|.
bool GetAtomPairArrayProperty(XID window,
                              XAtom property,
                              std::vector<std::pair<XAtom,XAtom> >* value) {
  XAtom type = x11::None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* properties = nullptr;
  unsigned long remaining_bytes = 0;

  int result = XGetWindowProperty(gfx::GetXDisplay(), window, property,
                                  0,           // offset into property data to
                                               // read
                                  (~0L),       // entire array
                                  x11::False,  // deleted
                                  AnyPropertyType, &type, &format, &num_items,
                                  &remaining_bytes, &properties);
  gfx::XScopedPtr<unsigned char> scoped_properties(properties);

  if (result != x11::Success)
    return false;

  // GTK does not require |type| to be kAtomPair.
  if (format != 32 || num_items % 2 != 0)
    return false;

  XAtom* atom_properties = reinterpret_cast<XAtom*>(properties);
  value->clear();
  for (size_t i = 0; i < num_items; i+=2)
    value->push_back(std::make_pair(atom_properties[i], atom_properties[i+1]));
  return true;
}

}  // namespace

SelectionOwner::SelectionOwner(XDisplay* x_display,
                               XID x_window,
                               XAtom selection_name)
    : x_display_(x_display),
      x_window_(x_window),
      selection_name_(selection_name),
      max_request_size_(GetMaxRequestSize(x_display)) {}

SelectionOwner::~SelectionOwner() {
  // If we are the selection owner, we need to release the selection so we
  // don't receive further events. However, we don't call ClearSelectionOwner()
  // because we don't want to do this indiscriminately.
  if (XGetSelectionOwner(x_display_, selection_name_) == x_window_)
    XSetSelectionOwner(x_display_, selection_name_, x11::None,
                       x11::CurrentTime);
}

void SelectionOwner::RetrieveTargets(std::vector<XAtom>* targets) {
  for (const auto& format_target : format_map_)
    targets->push_back(format_target.first);
}

void SelectionOwner::TakeOwnershipOfSelection(
    const SelectionFormatMap& data) {
  acquired_selection_timestamp_ = X11EventSource::GetInstance()->GetTimestamp();
  XSetSelectionOwner(x_display_, selection_name_, x_window_,
                     acquired_selection_timestamp_);

  if (XGetSelectionOwner(x_display_, selection_name_) == x_window_) {
    // The X server agrees that we are the selection owner. Commit our data.
    format_map_ = data;
  }
}

void SelectionOwner::ClearSelectionOwner() {
  XSetSelectionOwner(x_display_, selection_name_, x11::None, x11::CurrentTime);
  format_map_ = SelectionFormatMap();
}

void SelectionOwner::OnSelectionRequest(const XEvent& event) {
  XID requestor = event.xselectionrequest.requestor;
  XAtom requested_target = event.xselectionrequest.target;
  XAtom requested_property = event.xselectionrequest.property;

  // Incrementally build our selection. By default this is a refusal, and we'll
  // override the parts indicating success in the different cases.
  XEvent reply;
  reply.xselection.type = SelectionNotify;
  reply.xselection.requestor = requestor;
  reply.xselection.selection = event.xselectionrequest.selection;
  reply.xselection.target = requested_target;
  reply.xselection.property = x11::None;  // Indicates failure
  reply.xselection.time = event.xselectionrequest.time;

  if (requested_target == gfx::GetAtom(kMultiple)) {
    // The contents of |requested_property| should be a list of
    // <target,property> pairs.
    std::vector<std::pair<XAtom,XAtom> > conversions;
    if (GetAtomPairArrayProperty(requestor,
                                 requested_property,
                                 &conversions)) {
      std::vector<XAtom> conversion_results;
      for (const std::pair<XAtom, XAtom>& conversion : conversions) {
        bool conversion_successful =
            ProcessTarget(conversion.first, requestor, conversion.second);
        conversion_results.push_back(conversion.first);
        conversion_results.push_back(conversion_successful ? conversion.second
                                                           : x11::None);
      }

      // Set the property to indicate which conversions succeeded. This matches
      // what GTK does.
      XChangeProperty(
          x_display_, requestor, requested_property, gfx::GetAtom(kAtomPair),
          32, PropModeReplace,
          reinterpret_cast<const unsigned char*>(&conversion_results.front()),
          conversion_results.size());

      reply.xselection.property = requested_property;
    }
  } else {
    if (ProcessTarget(requested_target, requestor, requested_property))
      reply.xselection.property = requested_property;
  }

  // Send off the reply.
  XSendEvent(x_display_, requestor, x11::False, 0, &reply);
}

void SelectionOwner::OnSelectionClear(const XEvent& event) {
  DLOG(ERROR) << "SelectionClear";

  // TODO(erg): If we receive a SelectionClear event while we're handling data,
  // we need to delay clearing.
}

bool SelectionOwner::CanDispatchPropertyEvent(const XEvent& event) {
  return event.xproperty.state == PropertyDelete &&
         FindIncrementalTransferForEvent(event) != incremental_transfers_.end();
}

void SelectionOwner::OnPropertyEvent(const XEvent& event) {
  auto it = FindIncrementalTransferForEvent(event);
  if (it == incremental_transfers_.end())
    return;

  ProcessIncrementalTransfer(&(*it));
  if (!it->data.get())
    CompleteIncrementalTransfer(it);
}

bool SelectionOwner::ProcessTarget(XAtom target,
                                   XID requestor,
                                   XAtom property) {
  XAtom multiple_atom = gfx::GetAtom(kMultiple);
  XAtom save_targets_atom = gfx::GetAtom(kSaveTargets);
  XAtom targets_atom = gfx::GetAtom(kTargets);
  XAtom timestamp_atom = gfx::GetAtom(kTimestamp);

  if (target == multiple_atom || target == save_targets_atom)
    return false;

  if (target == timestamp_atom) {
    XChangeProperty(
        x_display_, requestor, property, XA_INTEGER, 32, PropModeReplace,
        reinterpret_cast<unsigned char*>(&acquired_selection_timestamp_), 1);
    return true;
  }

  if (target == targets_atom) {
    // We have been asked for TARGETS. Send an atom array back with the data
    // types we support.
    std::vector<XAtom> targets = {timestamp_atom, targets_atom,
                                  save_targets_atom, multiple_atom};
    RetrieveTargets(&targets);

    XChangeProperty(x_display_, requestor, property, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(&targets.front()),
                    targets.size());
    return true;
  }

  // Try to find the data type in map.
  auto it = format_map_.find(target);
  if (it != format_map_.end()) {
    if (it->second->size() > max_request_size_) {
      // We must send the data back in several chunks due to a limitation in
      // the size of X requests. Notify the selection requestor that the data
      // will be sent incrementally by returning data of type "INCR".
      long length = it->second->size();
      XChangeProperty(x_display_, requestor, property, gfx::GetAtom(kIncr), 32,
                      PropModeReplace,
                      reinterpret_cast<unsigned char*>(&length), 1);

      // Wait for the selection requestor to indicate that it has processed
      // the selection result before sending the first chunk of data. The
      // selection requestor indicates this by deleting |property|.
      base::TimeTicks timeout =
          base::TimeTicks::Now() +
          base::TimeDelta::FromMilliseconds(kIncrementalTransferTimeoutMs);
      incremental_transfers_.push_back(IncrementalTransfer(
          requestor, target, property,
          std::make_unique<XScopedEventSelector>(requestor, PropertyChangeMask),
          it->second, 0, timeout));

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
      XChangeProperty(
          x_display_,
          requestor,
          property,
          target,
          8,
          PropModeReplace,
          const_cast<unsigned char*>(it->second->front()),
          it->second->size());
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
  XChangeProperty(
      x_display_,
      transfer->window,
      transfer->property,
      transfer->target,
      8,
      PropModeReplace,
      const_cast<unsigned char*>(transfer->data->front() + transfer->offset),
      chunk_length);
  transfer->offset += chunk_length;
  transfer->timeout = base::TimeTicks::Now() +
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
  for (int i = static_cast<int>(incremental_transfers_.size()) - 1;
       i >= 0; --i) {
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
    SelectionOwner::FindIncrementalTransferForEvent(const XEvent& event) {
  for (auto it = incremental_transfers_.begin();
       it != incremental_transfers_.end(); ++it) {
    if (it->window == event.xproperty.window &&
        it->property == event.xproperty.atom) {
      return it;
    }
  }
  return incremental_transfers_.end();
}

SelectionOwner::IncrementalTransfer::IncrementalTransfer(
    XID window,
    XAtom target,
    XAtom property,
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

SelectionOwner::IncrementalTransfer& SelectionOwner::IncrementalTransfer::
operator=(IncrementalTransfer&&) = default;

SelectionOwner::IncrementalTransfer::~IncrementalTransfer() {
}

}  // namespace ui
