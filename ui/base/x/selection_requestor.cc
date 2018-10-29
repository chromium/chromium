// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <algorithm>

#include "base/run_loop.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

// The period of |abort_timer_|. Arbitrary but must be <= than
// kRequestTimeoutMs.
const int KSelectionRequestorTimerPeriodMs = 100;

// The amount of time to wait for a request to complete before aborting it.
const int kRequestTimeoutMs = 10000;

static_assert(KSelectionRequestorTimerPeriodMs <= kRequestTimeoutMs,
              "timer period must be <= request timeout");

// Combines |data| into a single RefCountedMemory object.
scoped_refptr<base::RefCountedMemory> CombineRefCountedMemory(
    const std::vector<scoped_refptr<base::RefCountedMemory> >& data) {
  if (data.size() == 1u)
    return data[0];

  size_t length = 0;
  for (size_t i = 0; i < data.size(); ++i)
    length += data[i]->size();
  std::vector<unsigned char> combined_data;
  combined_data.reserve(length);

  for (size_t i = 0; i < data.size(); ++i) {
    combined_data.insert(combined_data.end(),
                         data[i]->front(),
                         data[i]->front() + data[i]->size());
  }
  return base::RefCountedBytes::TakeVector(&combined_data);
}

}  // namespace

SelectionRequestor::SelectionRequestor(XDisplay* x_display,
                                       XID x_window,
                                       PlatformEventDispatcher* dispatcher)
    : x_display_(x_display),
      x_window_(x_window),
      x_property_(x11::None),
      dispatcher_(dispatcher),
      current_request_index_(0u) {
  x_property_ = gfx::GetAtom(kChromeSelection);
}

SelectionRequestor::~SelectionRequestor() {}

bool SelectionRequestor::PerformBlockingConvertSelection(
    XAtom selection,
    XAtom target,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_items,
    XAtom* out_type) {
  base::TimeTicks timeout =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kRequestTimeoutMs);
  Request request(selection, target, timeout);
  requests_.push_back(&request);
  if (current_request_index_ == (requests_.size() - 1))
    ConvertSelectionForCurrentRequest();
  BlockTillSelectionNotifyForRequest(&request);

  auto request_it = std::find(requests_.begin(), requests_.end(), &request);
  CHECK(request_it != requests_.end());
  if (static_cast<int>(current_request_index_) >
      request_it - requests_.begin()) {
    --current_request_index_;
  }
  requests_.erase(request_it);

  if (requests_.empty())
    abort_timer_.Stop();

  if (request.success) {
    if (out_data)
      *out_data = CombineRefCountedMemory(request.out_data);
    if (out_data_items)
      *out_data_items = request.out_data_items;
    if (out_type)
      *out_type = request.out_type;
  }
  return request.success;
}

void SelectionRequestor::PerformBlockingConvertSelectionWithParameter(
    XAtom selection,
    XAtom target,
    const std::vector<XAtom>& parameter) {
  SetAtomArrayProperty(x_window_, kChromeSelection, "ATOM", parameter);
  PerformBlockingConvertSelection(selection, target, NULL, NULL, NULL);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    XAtom selection,
    const std::vector<XAtom>& types) {
  for (auto it = types.begin(); it != types.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> data;
    XAtom type = x11::None;
    if (PerformBlockingConvertSelection(selection,
                                        *it,
                                        &data,
                                        NULL,
                                        &type) &&
        type == *it) {
      return SelectionData(type, data);
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(const XEvent& event) {
  Request* request = GetCurrentRequest();
  XAtom event_property = event.xselection.property;
  if (!request ||
      request->completed ||
      request->selection != event.xselection.selection ||
      request->target != event.xselection.target) {
    // ICCCM requires us to delete the property passed into SelectionNotify.
    if (event_property != x11::None)
      XDeleteProperty(x_display_, x_window_, event_property);
    return;
  }

  bool success = false;
  if (event_property == x_property_) {
    scoped_refptr<base::RefCountedMemory> out_data;
    success = ui::GetRawBytesOfProperty(x_window_,
                                        x_property_,
                                        &out_data,
                                        &request->out_data_items,
                                        &request->out_type);
    if (success) {
      request->out_data.clear();
      request->out_data.push_back(out_data);
    }
  }
  if (event_property != x11::None)
    XDeleteProperty(x_display_, x_window_, event_property);

  if (request->out_type == gfx::GetAtom(kIncr)) {
    request->data_sent_incrementally = true;
    request->out_data.clear();
    request->out_data_items = 0u;
    request->out_type = x11::None;
    request->timeout = base::TimeTicks::Now() +
        base::TimeDelta::FromMilliseconds(kRequestTimeoutMs);
  } else {
    CompleteRequest(current_request_index_, success);
  }
}

bool SelectionRequestor::CanDispatchPropertyEvent(const XEvent& event) {
  return event.xproperty.window == x_window_ &&
      event.xproperty.atom == x_property_ &&
      event.xproperty.state == PropertyNewValue;
}

void SelectionRequestor::OnPropertyEvent(const XEvent& event) {
  Request* request = GetCurrentRequest();
  if (!request || !request->data_sent_incrementally)
    return;

  scoped_refptr<base::RefCountedMemory> out_data;
  size_t out_data_items = 0u;
  Atom out_type = x11::None;
  bool success = ui::GetRawBytesOfProperty(x_window_,
                                           x_property_,
                                           &out_data,
                                           &out_data_items,
                                           &out_type);
  if (!success) {
    CompleteRequest(current_request_index_, false);
    return;
  }

  if (request->out_type != x11::None && request->out_type != out_type) {
    CompleteRequest(current_request_index_, false);
    return;
  }

  request->out_data.push_back(out_data);
  request->out_data_items += out_data_items;
  request->out_type = out_type;

  // Delete the property to tell the selection owner to send the next chunk.
  XDeleteProperty(x_display_, x_window_, x_property_);

  request->timeout = base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kRequestTimeoutMs);

  if (out_data->size() == 0u)
    CompleteRequest(current_request_index_, true);
}

void SelectionRequestor::AbortStaleRequests() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (size_t i = current_request_index_; i < requests_.size(); ++i) {
    if (requests_[i]->timeout <= now)
      CompleteRequest(i, false);
  }
}

void SelectionRequestor::CompleteRequest(size_t index, bool success) {
   if (index >= requests_.size())
     return;

  Request* request = requests_[index];
  if (request->completed)
    return;
  request->success = success;
  request->completed = true;

  if (index == current_request_index_) {
    while (GetCurrentRequest() && GetCurrentRequest()->completed)
      ++current_request_index_;
    ConvertSelectionForCurrentRequest();
  }

  if (!request->quit_closure.is_null())
    request->quit_closure.Run();
}

void SelectionRequestor::ConvertSelectionForCurrentRequest() {
  Request* request = GetCurrentRequest();
  if (request) {
    XConvertSelection(x_display_, request->selection, request->target,
                      x_property_, x_window_, x11::CurrentTime);
  }
}

void SelectionRequestor::BlockTillSelectionNotifyForRequest(Request* request) {
  if (PlatformEventSource::GetInstance()) {
    if (!abort_timer_.IsRunning()) {
      abort_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(KSelectionRequestorTimerPeriodMs),
          this, &SelectionRequestor::AbortStaleRequests);
    }

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    request->quit_closure = run_loop.QuitClosure();
    run_loop.Run();

    // We cannot put logic to process the next request here because the RunLoop
    // might be nested. For instance, request 'B' may start a RunLoop while the
    // RunLoop for request 'A' is running. It is not possible to end the RunLoop
    // for request 'A' without first ending the RunLoop for request 'B'.
  } else {
    // This occurs if PerformBlockingConvertSelection() is called during
    // shutdown and the PlatformEventSource has already been destroyed.
    while (!request->completed &&
           request->timeout > base::TimeTicks::Now()) {
      if (XPending(x_display_)) {
        XEvent event;
        XNextEvent(x_display_, &event);
        dispatcher_->DispatchEvent(&event);
      }
    }
  }
}

SelectionRequestor::Request* SelectionRequestor::GetCurrentRequest() {
  return current_request_index_ == requests_.size() ?
      NULL : requests_[current_request_index_];
}

SelectionRequestor::Request::Request(XAtom selection,
                                     XAtom target,
                                     base::TimeTicks timeout)
    : selection(selection),
      target(target),
      data_sent_incrementally(false),
      out_data_items(0u),
      out_type(x11::None),
      success(false),
      timeout(timeout),
      completed(false) {}

SelectionRequestor::Request::~Request() {
}

}  // namespace ui
