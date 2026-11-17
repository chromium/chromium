// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requester.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";
const char kChromeSelectionAlt[] = "CHROME_SELECTION_ALT";

// The amount of time to wait for the selection owner to make progress on the
// in-flight request before checking whether to abort it. Requests are
// processed serially, so without a timeout, a single owner that never responds
// would block all subsequent requests indefinitely.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(5);

// Maximum time to wait for the initial SelectionNotify when the same live
// selection owner still holds the selection. Encoding a large image (such as
// an 8Kx4K PNG) on demand in another X11 client can take well over 5 seconds
// before the owner sends its initial SelectionNotify.
constexpr base::TimeDelta kMaxInitialResponseTimeout = base::Seconds(60);

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
    base::Extend(combined, base::span<const uint8_t>(*datum));
  }
  return combined;
}

}  // namespace

SelectionRequester::SelectionRequester(x11::Window x_window,
                                       XClipboardHelper* helper)
    : x_window_(x_window),
      helper_(helper),
      x_properties_({x11::Connection::Get()->GetAtom(kChromeSelection),
                     x11::Connection::Get()->GetAtom(kChromeSelectionAlt)}) {}

SelectionRequester::~SelectionRequester() = default;

base::WeakPtr<SelectionRequester> SelectionRequester::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SelectionRequester::PerformConvertSelectionAsync(
    x11::Atom selection,
    x11::Atom target,
    ConvertSelectionCallback callback) {
  requests_.push(std::make_unique<Request>(next_request_id_++, selection,
                                           target, std::move(callback)));
  if (requests_.size() == 1) {
    ConvertSelectionForCurrentRequest();
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
  if (!request || request->selection != selection.selection ||
      request->target != selection.target ||
      (event_property != x11::Atom::None &&
       event_property != request->property)) {
    // This is not the reply to the current request. For example, some
    // selection owners (e.g. xsel) send an extra SelectionNotify after an INCR
    // transfer completes, which may arrive after the next request was sent.
    // ICCCM requires deleting the property passed into SelectionNotify, but
    // the current request's property is left alone since it may already hold
    // the reply to the current request.
    if (event_property != x11::Atom::None &&
        (!request || event_property != request->property)) {
      x11::Connection::Get()->DeleteProperty(x_window_, event_property);
    }
    return;
  }

  bool success = false;
  if (event_property == request->property) {
    scoped_refptr<base::RefCountedMemory> out_data;
    success = ui::GetRawBytesOfProperty(x_window_, request->property, &out_data,
                                        &request->out_type);
    if (success) {
      request->out_data.clear();
      request->out_data.push_back(out_data);
    }
  }
  if (event_property != x11::Atom::None) {
    x11::Connection::Get()->DeleteProperty(x_window_, event_property);
  }

  if (request->out_type == x11::Connection::Get()->GetAtom(kIncr)) {
    request->data_sent_incrementally = true;
    request->checking_owner = false;
    request->out_data.clear();
    request->out_type = x11::Atom::None;
    // The owner is making progress, so give it more time.
    timeout_timer_.Start(FROM_HERE, kRequestTimeout, this,
                         &SelectionRequester::OnRequestTimeout);
  } else {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(success);
  }
}

bool SelectionRequester::CanDispatchPropertyEvent(
    const x11::PropertyNotifyEvent& prop) {
  return prop.window == x_window_ &&
         std::ranges::contains(x_properties_, prop.atom) &&
         prop.state == x11::Property::NewValue;
}

void SelectionRequester::OnPropertyEvent(
    const x11::PropertyNotifyEvent& event) {
  Request* request = GetCurrentRequest();
  if (!request || !request->data_sent_incrementally ||
      event.atom != request->property) {
    return;
  }

  scoped_refptr<base::RefCountedMemory> out_data;
  x11::Atom out_type = x11::Atom::None;
  bool success = ui::GetRawBytesOfProperty(x_window_, request->property,
                                           &out_data, &out_type);
  if (!success) {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(false);
    return;
  }

  if (request->out_type != x11::Atom::None && request->out_type != out_type) {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(false);
    return;
  }

  request->out_data.push_back(out_data);
  request->out_type = out_type;

  // Delete the property to tell the selection owner to send the next chunk.
  x11::Connection::Get()->DeleteProperty(x_window_, request->property);

  if (!out_data->size()) {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(true);
    return;
  }

  // The owner is making progress, so give it more time.
  timeout_timer_.Start(FROM_HERE, kRequestTimeout, this,
                       &SelectionRequester::OnRequestTimeout);
}

void SelectionRequester::OnGetInitialOwner(
    uint64_t request_id,
    x11::GetSelectionOwnerResponse response) {
  Request* request = GetCurrentRequest();
  // Replies and events are dispatched in sequence order, so this reply is
  // handled before any SelectionNotify for the ConvertSelection sent after it.
  // The request can only have completed already if the timeout fired first.
  if (!request || request->id != request_id) {
    return;
  }
  request->owner = response ? response->owner : x11::Window::None;
}

void SelectionRequester::OnRequestTimeout() {
  Request* request = GetCurrentRequest();
  if (!request) {
    return;
  }

  // Only the wait for the initial SelectionNotify can be extended, and only
  // while a live owner holds the selection. The request is aborted:
  // - Once an INCR transfer has started. The timer is restarted whenever a
  //   chunk arrives, so this means the transfer stalled for `kRequestTimeout`.
  // - If the owner check sent by the previous timeout got no reply within
  //   `kRequestTimeout`, which means the X server itself is unresponsive.
  // - If `owner` is None. Either the selection had no owner when the request
  //   was sent, or not even the reply to the initial GetSelectionOwner arrived
  //   within `kRequestTimeout`. Neither case has a live owner worth waiting
  //   for.
  // - Once `kMaxInitialResponseTimeout` has passed, so that an owner that never
  //   replies can't block the queue indefinitely.
  if (request->data_sent_incrementally || request->checking_owner ||
      request->owner == x11::Window::None ||
      base::TimeTicks::Now() >= request->initial_response_deadline) {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(false);
    return;
  }

  // Check whether `owner` still owns the selection. If it exited or another
  // client took the selection, it will never reply. The X server answers this
  // itself without involving the owner, so the reply normally arrives long
  // before the owner's SelectionNotify. If the SelectionNotify arrives first,
  // the request completes and OnCheckOwnerOnTimeout() ignores the reply.
  request->checking_owner = true;
  auto* connection = x11::Connection::Get();
  connection->GetSelectionOwner({request->selection})
      .OnResponse(base::BindOnce(&SelectionRequester::OnCheckOwnerOnTimeout,
                                 GetWeakPtr(), request->id));
  connection->Flush();
  timeout_timer_.Start(FROM_HERE, kRequestTimeout, this,
                       &SelectionRequester::OnRequestTimeout);
}

void SelectionRequester::OnCheckOwnerOnTimeout(
    uint64_t request_id,
    x11::GetSelectionOwnerResponse response) {
  Request* request = GetCurrentRequest();
  if (!request || request->id != request_id ||
      request->data_sent_incrementally) {
    return;
  }

  request->checking_owner = false;
  x11::Window current_owner = response ? response->owner : x11::Window::None;
  base::TimeTicks now = base::TimeTicks::Now();
  if (current_owner != request->owner ||
      now >= request->initial_response_deadline) {
    // Note: `this` may be deleted after this call.
    CompleteCurrentRequest(false);
    return;
  }

  base::TimeDelta next_timeout =
      std::min(kRequestTimeout, request->initial_response_deadline - now);
  timeout_timer_.Start(FROM_HERE, next_timeout, this,
                       &SelectionRequester::OnRequestTimeout);
}

void SelectionRequester::CompleteCurrentRequest(bool success) {
  CHECK(!requests_.empty());

  std::unique_ptr<Request> request = std::move(requests_.front());
  requests_.pop();

  std::vector<uint8_t> out_data;
  if (success) {
    out_data = CombineData(request->out_data);
  }

  // Start the next request before running the callback, since the callback
  // may delete `this`.
  ConvertSelectionForCurrentRequest();

  if (request->callback) {
    // Note: `this` may be deleted after this call.
    std::move(request->callback)
        .Run(success, std::move(out_data), request->out_type);
  }
}

void SelectionRequester::ConvertSelectionForCurrentRequest() {
  Request* request = GetCurrentRequest();
  if (!request) {
    timeout_timer_.Stop();
    return;
  }

  request->property = x_properties_[next_property_index_];
  next_property_index_ = (next_property_index_ + 1) % x_properties_.size();

  auto* connection = x11::Connection::Get();
  connection->GetSelectionOwner({request->selection})
      .OnResponse(base::BindOnce(&SelectionRequester::OnGetInitialOwner,
                                 GetWeakPtr(), request->id));
  connection->ConvertSelection({
      .requestor = x_window_,
      .selection = request->selection,
      .target = request->target,
      .property = request->property,
      .time = x11::Time::CurrentTime,
  });
  connection->Flush();

  request->initial_response_deadline =
      base::TimeTicks::Now() + kMaxInitialResponseTimeout;
  timeout_timer_.Start(FROM_HERE, kRequestTimeout, this,
                       &SelectionRequester::OnRequestTimeout);
}

SelectionRequester::Request* SelectionRequester::GetCurrentRequest() {
  return requests_.empty() ? nullptr : requests_.front().get();
}

SelectionRequester::Request::Request(uint64_t id,
                                     x11::Atom selection,
                                     x11::Atom target,
                                     ConvertSelectionCallback callback)
    : id(id),
      selection(selection),
      target(target),
      callback(std::move(callback)) {}

SelectionRequester::Request::~Request() = default;

}  // namespace ui
