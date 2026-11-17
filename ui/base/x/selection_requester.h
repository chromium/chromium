// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_REQUESTER_H_
#define UI_BASE_X_SELECTION_REQUESTER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace ui {
class SelectionData;
class XClipboardHelper;

// Requests and later receives data from the X11 server through the selection
// system.
//
// X11 uses a system called "selections" to implement clipboards and drag and
// drop. This class interprets messages from the stateful selection request
// API. SelectionRequester should only deal with the X11 details; it does not
// implement per-component fast-paths.
class COMPONENT_EXPORT(UI_BASE_X) SelectionRequester {
 public:
  using ConvertSelectionCallback = base::OnceCallback<
      void(bool success, std::vector<uint8_t> out_data, x11::Atom out_type)>;

  SelectionRequester(x11::Window xwindow, XClipboardHelper* helper);
  SelectionRequester(const SelectionRequester&) = delete;
  SelectionRequester& operator=(const SelectionRequester&) = delete;
  ~SelectionRequester();

  base::WeakPtr<SelectionRequester> GetWeakPtr();

  // Does the work of requesting |target| from |selection|. The result is
  // returned via |callback|.
  void PerformConvertSelectionAsync(x11::Atom selection,
                                    x11::Atom target,
                                    ConvertSelectionCallback callback);

  // Asynchronously returns the first of |types| offered by the current owner of
  // |selection|. Returns an empty SelectionData object if none of |types| are
  // available.
  void RequestTypesAsync(x11::Atom selection,
                         const std::vector<x11::Atom>& types,
                         base::OnceCallback<void(SelectionData)> callback);

  // It is our owner's responsibility to plumb X11 SelectionNotify events on
  // |xwindow_| to us.
  void OnSelectionNotify(const x11::SelectionNotifyEvent& event);

  // Returns true if SelectionOwner can process the XChangeProperty event,
  // |event|.
  bool CanDispatchPropertyEvent(const x11::PropertyNotifyEvent& event);

  void OnPropertyEvent(const x11::PropertyNotifyEvent& event);

 private:
  friend class SelectionRequesterTest;

  // A request that has been issued.
  struct Request {
    Request(uint64_t id,
            x11::Atom selection,
            x11::Atom target,
            ConvertSelectionCallback callback);
    ~Request();

    const uint64_t id;

    // The target and selection requested in the XConvertSelection() request.
    // Used for error detection.
    x11::Atom selection;
    x11::Atom target;

    // The property on `x_window_` passed into the XConvertSelection()
    // request. Set when the request is sent to the selection owner.
    x11::Atom property = x11::Atom::None;

    // Window owning `selection` when ConvertSelection was issued, if any.
    x11::Window owner = x11::Window::None;

    // Deadline for receiving the initial SelectionNotify while `owner` still
    // owns `selection`.
    base::TimeTicks initial_response_deadline;

    // Whether an async GetSelectionOwner check is in flight after a timeout
    // interval elapsed.
    bool checking_owner = false;

    // Whether the result of the XConvertSelection() request is being sent
    // incrementally.
    bool data_sent_incrementally = false;

    // The result data for the XConvertSelection() request.
    std::vector<scoped_refptr<base::RefCountedMemory>> out_data;
    x11::Atom out_type = x11::Atom::None;

    ConvertSelectionCallback callback;
  };

  void OnGetInitialOwner(uint64_t request_id,
                         x11::GetSelectionOwnerResponse response);

  // Called when the in-flight request has not made progress within the
  // timeout. Aborts the request or extends the wait if the same selection owner
  // is still active before sending its initial SelectionNotify.
  void OnRequestTimeout();

  void OnCheckOwnerOnTimeout(uint64_t request_id,
                             x11::GetSelectionOwnerResponse response);

  // Removes the current request from the queue, converts the selection for the
  // next request, and runs the completed request's callback.
  void CompleteCurrentRequest(bool success);

  // Converts the selection for the request at the front of `requests_`.
  void ConvertSelectionForCurrentRequest();

  // Returns the request at the front of `requests_` or nullptr if there isn't
  // any.
  Request* GetCurrentRequest();

  void RequestTypesRecursive(x11::Atom selection,
                             std::vector<x11::Atom> types,
                             base::OnceCallback<void(SelectionData)> callback);

  void OnRequestTypesAsyncResponse(
      x11::Atom selection,
      std::vector<x11::Atom> remaining_types,
      base::OnceCallback<void(SelectionData)> callback,
      bool success,
      std::vector<uint8_t> data,
      x11::Atom type);

  // Our X11 state.
  const x11::Window x_window_;

  // Not owned.
  const raw_ptr<XClipboardHelper> helper_;

  // The properties on `x_window_` set by the selection owner with the value of
  // the selection. Consecutive requests alternate between them, so that a
  // stale SelectionNotify for one request can't be mistaken for the reply to
  // the next one, even if both have the same selection and target.
  const std::array<x11::Atom, 2> x_properties_;

  // Index into `x_properties_` of the property to use for the next request.
  size_t next_property_index_ = 0;

  // Pending requests. Only the request at the front has been sent to the
  // selection owner.
  base::queue<std::unique_ptr<Request>> requests_;

  // Aborts the in-flight request if the selection owner stops responding.
  base::OneShotTimer timeout_timer_;

  uint64_t next_request_id_ = 1;

  base::WeakPtrFactory<SelectionRequester> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_REQUESTER_H_
