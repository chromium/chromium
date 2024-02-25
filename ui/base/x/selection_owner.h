// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_OWNER_H_
#define UI_BASE_X_SELECTION_OWNER_H_

#include <stddef.h>

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/x/selection_utils.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {
class ScopedEventSelector;
}

namespace ui {

COMPONENT_EXPORT(UI_BASE_X) extern const char kIncr[];
COMPONENT_EXPORT(UI_BASE_X) extern const char kSaveTargets[];
COMPONENT_EXPORT(UI_BASE_X) extern const char kTargets[];

// Owns a specific X11 selection on an X window.
//
// The selection owner object keeps track of which xwindow is the current
// owner, and when its |xwindow_|, offers different data types to other
// processes.
class COMPONENT_EXPORT(UI_BASE_X) SelectionOwner {
 public:
  SelectionOwner(x11::Connection& connection,
                 x11::Window xwindow,
                 x11::Atom selection_name);

  SelectionOwner(const SelectionOwner&) = delete;
  SelectionOwner& operator=(const SelectionOwner&) = delete;

  ~SelectionOwner();

  // Returns the current selection data. Useful for fast paths.
  const SelectionFormatMap& selection_format_map() { return format_map_; }

  // Appends a list of types we're offering to |targets|.
  void RetrieveTargets(std::vector<x11::Atom>* targets);

  // Attempts to take ownership of the selection. If we're successful, present
  // |data| to other windows.
  void TakeOwnershipOfSelection(const SelectionFormatMap& data);

  // Clears our internal format map and clears the selection owner, whether we
  // own the selection or not.
  void ClearSelectionOwner();

  // It is our owner's responsibility to plumb X11 events on |xwindow_| to us.
  void OnSelectionRequest(const x11::SelectionRequestEvent& event);
  void OnSelectionClear(const x11::SelectionClearEvent& event);

  // Returns true if SelectionOwner can process the XPropertyEvent event,
  // |event|.
  bool CanDispatchPropertyEvent(const x11::PropertyNotifyEvent& event);

  void OnPropertyEvent(const x11::PropertyNotifyEvent& event);

 private:
  // Holds state related to an incremental data transfer.
  struct IncrementalTransfer {
    IncrementalTransfer(x11::Window window,
                        x11::Atom target,
                        x11::Atom property,
                        x11::ScopedEventSelector event_selector,
                        const scoped_refptr<base::RefCountedMemory>& data,
                        int offset,
                        base::TimeTicks timeout);

    IncrementalTransfer(const IncrementalTransfer&) = delete;
    IncrementalTransfer& operator=(const IncrementalTransfer&) = delete;

    ~IncrementalTransfer();

    // Move-only class.
    IncrementalTransfer(IncrementalTransfer&&);
    IncrementalTransfer& operator=(IncrementalTransfer&&);

    // Parameters from the XSelectionRequest. The data is transferred over
    // |property| on |window|.
    x11::Window window;
    x11::Atom target;
    x11::Atom property;

    // Selects events on |window|.
    x11::ScopedEventSelector event_selector;

    // The data to be transferred.
    scoped_refptr<base::RefCountedMemory> data;

    // The offset from the beginning of |data| of the first byte to be
    // transferred in the next chunk.
    size_t offset;

    // Time when the transfer should be aborted because the selection requestor
    // is taking too long to notify us that we can send the next chunk.
    base::TimeTicks timeout;
  };

  // Attempts to convert the selection to |target|. If the conversion is
  // successful, true is returned and the result is stored in the |property|
  // of |requestor|.
  bool ProcessTarget(x11::Atom target,
                     x11::Window requestor,
                     x11::Atom property);

  // Sends the next chunk of data for given the incremental data transfer.
  void ProcessIncrementalTransfer(IncrementalTransfer* transfer);

  // Aborts any incremental data transfers which have timed out.
  void AbortStaleIncrementalTransfers();

  // Called when the transfer at |it| has completed to do cleanup.
  void CompleteIncrementalTransfer(
      std::vector<IncrementalTransfer>::iterator it);

  // Returns the incremental data transfer, if any, which was waiting for
  // |event|.
  std::vector<IncrementalTransfer>::iterator FindIncrementalTransferForEvent(
      const x11::PropertyNotifyEvent& event);

  raw_ref<x11::Connection> connection_;

  // Our X11 state.
  x11::Window x_window_;

  // The X11 selection that this instance communicates on.
  x11::Atom selection_name_;

  // The time that this instance took ownership of its selection.
  x11::Time acquired_selection_timestamp_;

  // The data we are currently serving.
  SelectionFormatMap format_map_;

  std::vector<IncrementalTransfer> incremental_transfers_;

  // Used to abort stale incremental data transfers.
  base::RepeatingTimer incremental_transfer_abort_timer_;
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_OWNER_H_
