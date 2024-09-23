// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/x/selection_requestor.h"

#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

// The amount of time to wait for a request to complete before aborting it.
const int kRequestTimeoutMs = 1000;

// Combines |data| into a single std::vector<uint8_t>.
std::vector<uint8_t> CombineData(
    const std::vector<scoped_refptr<base::RefCountedMemory>>& data) {
  size_t bytes = 0;
  for (const auto& datum : data) {
    bytes += datum->size();
  }
  std::vector<uint8_t> combined;
  combined.reserve(bytes);
  for (const auto& datum : data) {
    std::copy(datum->data(), datum->data() + datum->size(),
              std::back_inserter(combined));
  }
  return combined;
}

}  // namespace

SelectionRequestor::SelectionRequestor(x11::Window x_window,
                                       XClipboardHelper* helper)
    : x_window_(x_window),
      helper_(helper),
      x_property_(x11::GetAtom(kChromeSelection)) {}

SelectionRequestor::~SelectionRequestor() = default;

bool SelectionRequestor::PerformBlockingConvertSelection(
    x11::Atom selection,
    x11::Atom target,
    std::vector<uint8_t>* out_data,
    x11::Atom* out_type) {
  base::TimeTicks timeout =
      base::TimeTicks::Now() + base::Milliseconds(kRequestTimeoutMs);
  Request request(selection, target, timeout);
  requests_.push_back(&request);
  if (current_request_index_ == (requests_.size() - 1)) {
    ConvertSelectionForCurrentRequest();
  }
  BlockTillSelectionNotifyForRequest(&request);

  auto request_it = base::ranges::find(requests_, &request);
  CHECK(request_it != requests_.end());
  if (static_cast<int>(current_request_index_) >
      request_it - requests_.begin()) {
    --current_request_index_;
  }
  requests_.erase(request_it);

  if (request.success) {
    if (out_data) {
      *out_data = CombineData(request.out_data);
    }
    if (out_type) {
      *out_type = request.out_type;
    }
  }
  return request.success;
}

void SelectionRequestor::PerformBlockingConvertSelectionWithParameter(
    x11::Atom selection,
    x11::Atom target,
    const std::vector<x11::Atom>& parameter) {
  x11::Connection::Get()->SetArrayProperty(
      x_window_, x11::GetAtom(kChromeSelection), x11::Atom::ATOM, parameter);
  PerformBlockingConvertSelection(selection, target, nullptr, nullptr);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    x11::Atom selection,
    const std::vector<x11::Atom>& types) {
  for (const x11::Atom& item : types) {
    std::vector<uint8_t> data;
    x11::Atom type = x11::Atom::None;
    if (PerformBlockingConvertSelection(selection, item, &data, &type) &&
        type == item) {
      return SelectionData(type, base::RefCountedBytes::TakeVector(&data));
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(
    const x11::SelectionNotifyEvent& selection) {
  Request* request = GetCurrentRequest();
  x11::Atom event_property = selection.property;
  if (!request || request->completed ||
      request->selection != selection.selection ||
      request->target != selection.target) {
    // ICCCM requires us to delete the property passed into SelectionNotify.
    if (event_property != x11::Atom::None) {
      x11::Connection::Get()->DeleteProperty(x_window_, event_property);
    }
    return;
  }

  bool success = false;
  if (event_property == x_property_) {
    scoped_refptr<base::RefCountedMemory> out_data;
    success = ui::GetRawBytesOfProperty(x_window_, x_property_, &out_data,
                                        &request->out_type);
    if (success) {
      request->out_data.clear();
      request->out_data.push_back(out_data);
    }
  }
  if (event_property != x11::Atom::None) {
    x11::Connection::Get()->DeleteProperty(x_window_, event_property);
  }

  if (request->out_type == x11::GetAtom(kIncr)) {
    request->data_sent_incrementally = true;
    request->out_data.clear();
    request->out_type = x11::Atom::None;
    request->timeout =
        base::TimeTicks::Now() + base::Milliseconds(kRequestTimeoutMs);
  } else {
    CompleteRequest(current_request_index_, success);
  }
}

bool SelectionRequestor::CanDispatchPropertyEvent(
    const x11::PropertyNotifyEvent& prop) {
  return prop.window == x_window_ && prop.atom == x_property_ &&
         prop.state == x11::Property::NewValue;
}

void SelectionRequestor::OnPropertyEvent(
    const x11::PropertyNotifyEvent& event) {
  Request* request = GetCurrentRequest();
  if (!request || !request->data_sent_incrementally) {
    return;
  }

  scoped_refptr<base::RefCountedMemory> out_data;
  x11::Atom out_type = x11::Atom::None;
  bool success =
      ui::GetRawBytesOfProperty(x_window_, x_property_, &out_data, &out_type);
  if (!success) {
    CompleteRequest(current_request_index_, false);
    return;
  }

  if (request->out_type != x11::Atom::None && request->out_type != out_type) {
    CompleteRequest(current_request_index_, false);
    return;
  }

  request->out_data.push_back(out_data);
  request->out_type = out_type;

  // Delete the property to tell the selection owner to send the next chunk.
  x11::Connection::Get()->DeleteProperty(x_window_, x_property_);

  request->timeout =
      base::TimeTicks::Now() + base::Milliseconds(kRequestTimeoutMs);

  if (!out_data->size()) {
    CompleteRequest(current_request_index_, true);
  }
}

void SelectionRequestor::AbortStaleRequests() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (size_t i = current_request_index_; i < requests_.size(); ++i) {
    if (requests_[i]->timeout <= now) {
      CompleteRequest(i, false);
    }
  }
}

void SelectionRequestor::CompleteRequest(size_t index, bool success) {
  if (index >= requests_.size()) {
    return;
  }

  Request* request = requests_[index];
  if (request->completed) {
    return;
  }
  request->success = success;
  request->completed = true;

  if (index == current_request_index_) {
    while (GetCurrentRequest() && GetCurrentRequest()->completed) {
      ++current_request_index_;
    }
    ConvertSelectionForCurrentRequest();
  }
}

void SelectionRequestor::ConvertSelectionForCurrentRequest() {
  Request* request = GetCurrentRequest();
  if (request) {
    x11::Connection::Get()->ConvertSelection({
        .requestor = x_window_,
        .selection = request->selection,
        .target = request->target,
        .property = x_property_,
        .time = x11::Time::CurrentTime,
    });
  }
}

void SelectionRequestor::BlockTillSelectionNotifyForRequest(Request* request) {
  auto* connection = x11::Connection::Get();
  auto& events = connection->events();
  size_t i = 0;
  while (!request->completed && request->timeout > base::TimeTicks::Now()) {
    connection->Flush();
    connection->ReadResponses();
    size_t events_size = events.size();
    for (; i < events_size; ++i) {
      auto& event = events[i];
      if (helper_->DispatchEvent(event)) {
        event = x11::Event();
      }
    }
    DCHECK_EQ(events_size, events.size());
  }
  AbortStaleRequests();
}

SelectionRequestor::Request* SelectionRequestor::GetCurrentRequest() {
  return current_request_index_ == requests_.size()
             ? nullptr
             : requests_[current_request_index_];
}

SelectionRequestor::Request::Request(x11::Atom selection,
                                     x11::Atom target,
                                     base::TimeTicks timeout)
    : selection(selection),
      target(target),
      data_sent_incrementally(false),
      out_type(x11::Atom::None),
      success(false),
      timeout(timeout),
      completed(false) {}

SelectionRequestor::Request::~Request() = default;

}  // namespace ui
