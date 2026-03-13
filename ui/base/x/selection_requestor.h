// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_REQUESTOR_H_
#define UI_BASE_X_SELECTION_REQUESTOR_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/x/connection.h"

namespace ui {
class SelectionData;
class XClipboardHelper;

// Requests and later receives data from the X11 server through the selection
// system.
//
// X11 uses a system called "selections" to implement clipboards and drag and
// drop. This class interprets messages from the stateful selection request
// API. SelectionRequestor should only deal with the X11 details; it does not
// implement per-component fast-paths.
class COMPONENT_EXPORT(UI_BASE_X) SelectionRequestor {
 public:
  using ConvertSelectionCallback = base::OnceCallback<
      void(bool success, std::vector<uint8_t> out_data, x11::Atom out_type)>;

  SelectionRequestor(x11::Window xwindow, XClipboardHelper* helper);
  SelectionRequestor(const SelectionRequestor&) = delete;
  SelectionRequestor& operator=(const SelectionRequestor&) = delete;
  ~SelectionRequestor();

  base::WeakPtr<SelectionRequestor> GetWeakPtr();

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
  friend class SelectionRequestorTest;

  // A request that has been issued.
  struct Request {
    Request(x11::Atom selection,
            x11::Atom target,
            base::TimeTicks timeout,
            ConvertSelectionCallback callback);
    ~Request();

    // The target and selection requested in the XConvertSelection() request.
    // Used for error detection.
    x11::Atom selection;
    x11::Atom target;

    // Whether the result of the XConvertSelection() request is being sent
    // incrementally.
    bool data_sent_incrementally;

    // The result data for the XConvertSelection() request.
    std::vector<scoped_refptr<base::RefCountedMemory>> out_data;
    x11::Atom out_type;

    // Whether the XConvertSelection() request was successful.
    bool success;

    // The time when the request should be aborted.
    base::TimeTicks timeout;

    // True if the request is complete.
    bool completed;

    ConvertSelectionCallback callback;
  };

  // Aborts requests which have timed out.
  void AbortStaleRequests();

  // Mark |request| as completed. If the current request is completed, converts
  // the selection for the next request.
  void CompleteRequest(size_t index, bool success);

  // Converts the selection for the request at |current_request_index_|.
  void ConvertSelectionForCurrentRequest();

  // Returns the request at |current_request_index_| or NULL if there isn't any.
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

  // The property on |x_window_| set by the selection owner with the value of
  // the selection.
  const x11::Atom x_property_;

  // In progress requests.
  std::vector<std::unique_ptr<Request>> requests_;

  base::OneShotTimer abort_timer_;

  base::WeakPtrFactory<SelectionRequestor> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_REQUESTOR_H_
