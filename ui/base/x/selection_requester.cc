// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requester.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
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
    std::copy(datum->data(), UNSAFE_TODO(datum->data() + datum->size()),
              std::back_inserter(combined));
  }
  return combined;
}

}  // namespace

SelectionRequester::SelectionRequester(x11::Window x_window,
                                       XClipboardHelper* helper)
    : x_window_(x_window),
      helper_(helper),
      x_property_(x11::GetAtom(kChromeSelection)) {}

SelectionRequester::~SelectionRequester() = default;

base::WeakPtr<SelectionRequester> SelectionRequester::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SelectionRequester::PerformConvertSelectionAsync(
    x11::Atom selection,
    x11::Atom target,
    ConvertSelectionCallback callback) {
  requests_.push_back(std::make_unique<Request>(
      selection, target,
      base::TimeTicks::Now() + base::Milliseconds(kRequestTimeoutMs),
      std::move(callback)));
  if (requests_.size() == 1) {
    ConvertSelectionForCurrentRequest();
  }

  if (!abort_timer_.IsRunning()) {
    abort_timer_.Start(FROM_HERE, base::Milliseconds(kRequestTimeoutMs), this,
                       &SelectionRequester::AbortStaleRequests);
  }
}

void SelectionRequester::RequestTypesAsync(
    x11::Atom selection,
    const std::vector<x11::Atom>& types,
    base::OnceCallback<void(SelectionData)> callback) {
  RequestTypesRecursive(selection,
                        std::vector<x11::Atom>(types.rbegin(), types.rend()),
                        std::move(callback));
}

void SelectionRequester::RequestTypesRecursive(
    x11::Atom selection,
    std::vector<x11::Atom> types,
    base::OnceCallback<void(SelectionData)> callback) {
  if (types.empty()) {
    std::move(callback).Run(SelectionData());
    return;
  }

  x11::Atom target = types.back();
  types.pop_back();

  PerformConvertSelectionAsync(
      selection, target,
      base::BindOnce(&SelectionRequester::OnRequestTypesAsyncResponse,
                     GetWeakPtr(), selection, std::move(types),
                     std::move(callback)));
}

void SelectionRequester::OnRequestTypesAsyncResponse(
    x11::Atom selection,
    std::vector<x11::Atom> remaining_types,
    base::OnceCallback<void(SelectionData)> callback,
    bool success,
    std::vector<uint8_t> data,
    x11::Atom type) {
  if (success && type != x11::Atom::None) {
    std::move(callback).Run(SelectionData(
        type, base::MakeRefCounted<base::RefCountedBytes>(std::move(data))));
    return;
  }
  RequestTypesRecursive(selection, std::move(remaining_types),
                        std::move(callback));
}

void SelectionRequester::OnSelectionNotify(
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
    // Note: `this` may be deleted after this call.
    CompleteRequest(0, success);
  }
}

bool SelectionRequester::CanDispatchPropertyEvent(
    const x11::PropertyNotifyEvent& prop) {
  return prop.window == x_window_ && prop.atom == x_property_ &&
         prop.state == x11::Property::NewValue;
}

void SelectionRequester::OnPropertyEvent(
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
    // Note: `this` may be deleted after this call.
    CompleteRequest(0, false);
    return;
  }

  if (request->out_type != x11::Atom::None && request->out_type != out_type) {
    // Note: `this` may be deleted after this call.
    CompleteRequest(0, false);
    return;
  }

  request->out_data.push_back(out_data);
  request->out_type = out_type;

  // Delete the property to tell the selection owner to send the next chunk.
  x11::Connection::Get()->DeleteProperty(x_window_, x_property_);

  request->timeout =
      base::TimeTicks::Now() + base::Milliseconds(kRequestTimeoutMs);

  if (!out_data->size()) {
    // Note: `this` may be deleted after this call.
    CompleteRequest(0, true);
  }
}

void SelectionRequester::AbortStaleRequests() {
  base::TimeTicks now = base::TimeTicks::Now();
  auto weak_this = GetWeakPtr();
  // Iterate backwards. CompleteRequest() only erases from the front, so
  // trailing items shifting left won't affect our backward scan.
  for (int i = static_cast<int>(requests_.size()) - 1; i >= 0; --i) {
    if (requests_[i]->timeout <= now && !requests_[i]->completed) {
      CompleteRequest(i, false);
      if (!weak_this) {
        return;
      }
    }
  }

  if (!requests_.empty()) {
    abort_timer_.Start(FROM_HERE, base::Milliseconds(kRequestTimeoutMs), this,
                       &SelectionRequester::AbortStaleRequests);
  }
}

void SelectionRequester::CompleteRequest(size_t index, bool success) {
  CHECK_LT(index, requests_.size());

  Request* request = requests_[index].get();
  if (request->completed) {
    return;
  }

  // 1. Mark completed
  request->success = success;
  request->completed = true;

  // 2. Extract data & callback
  std::vector<uint8_t> out_data;
  if (success) {
    out_data = CombineData(request->out_data);
  }
  x11::Atom out_type = request->out_type;
  auto callback = std::move(request->callback);

  // 3. Update `requests_` queue
  if (index == 0) {
    size_t completed_count = 0;
    while (completed_count < requests_.size() &&
           requests_[completed_count]->completed) {
      ++completed_count;
    }

    if (completed_count > 0) {
      requests_.erase(requests_.begin(), requests_.begin() + completed_count);
    }

    ConvertSelectionForCurrentRequest();
  }

  // 4. Safely run the callback
  if (callback) {
    std::move(callback).Run(success, std::move(out_data), out_type);
  }
}

void SelectionRequester::ConvertSelectionForCurrentRequest() {
  Request* request = GetCurrentRequest();
  if (request) {
    x11::Connection::Get()->ConvertSelection({
        .requestor = x_window_,
        .selection = request->selection,
        .target = request->target,
        .property = x_property_,
        .time = x11::Time::CurrentTime,
    });
    x11::Connection::Get()->Flush();
  }
}

SelectionRequester::Request* SelectionRequester::GetCurrentRequest() {
  return requests_.empty() ? nullptr : requests_.front().get();
}

SelectionRequester::Request::Request(x11::Atom selection,
                                     x11::Atom target,
                                     base::TimeTicks timeout,
                                     ConvertSelectionCallback callback)
    : selection(selection),
      target(target),
      data_sent_incrementally(false),
      out_type(x11::Atom::None),
      success(false),
      timeout(timeout),
      completed(false),
      callback(std::move(callback)) {}

SelectionRequester::Request::~Request() = default;

}  // namespace ui
