// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

// Handles clipboard operations for X11.
// Registers to receive standard X11 events, as well as
// XFixesSetSelectionOwnerNotify.  When the remote owner changes, TARGETS and
// text/plain are preemptively fetched.  They can then be provided immediately
// to GetAvailableMimeTypes, and RequestClipboardData when mime_type is
// text/plain.  Otherwise GetAvailableMimeTypes and RequestClipboardData call
// the appropriate X11 functions and invoke callbacks when the associated events
// are received.
class X11ClipboardOzone : public PlatformClipboard, public XEventDispatcher {
 public:
  X11ClipboardOzone();
  ~X11ClipboardOzone() override;

  // PlatformClipboard:
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner(ClipboardBuffer buffer) override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

 private:
  struct SelectionState;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

  bool OnSelectionRequest(const XSelectionRequestEvent& event);
  bool OnSelectionNotify(const XSelectionEvent& event);
  bool OnSetSelectionOwnerNotify(XEvent* xev);

  // Returns an X atom for a clipboard buffer type.
  XAtom SelectionAtomForBuffer(ClipboardBuffer buffer) const;

  // Returns a clipboard buffer type for an X atom for a selection name of the
  // system clipboard buffer.
  ClipboardBuffer BufferForSelectionAtom(XAtom selection) const;

  // Returns the state for the given selection;
  SelectionState& GetSelectionState(XAtom selection);

  // Queries the current clipboard owner for what mime types are available by
  // sending XConvertSelection with target=TARGETS.  After sending this, we
  // will receive a SelectionNotify event with xselection.target=TARGETS which
  // is processed in |OnSelectionNotify|.
  void QueryTargets(XAtom selection);

  // Reads the contents of the remote clipboard by sending XConvertSelection
  // with target=<mime-type>.  After sending this, we will receive a
  // SelectionNotify event with xselection.target=<mime-type> which is processed
  // in |OnSelectionNotify|.
  void ReadRemoteClipboard(XAtom selection);

  // Local cache of atoms.
  const XAtom atom_clipboard_;
  const XAtom atom_targets_;
  const XAtom atom_timestamp_;

  // The property on |x_window_| which will receive remote clipboard contents.
  const XAtom x_property_;

  // Our X11 state.
  Display* const x_display_;

  // Input-only window used as a selection owner.
  const XID x_window_;

  // If XFixes is unavailable, this clipboard window will not register to
  // receive events and no processing will take place.
  // TODO(joelhockey): Make clipboard work without xfixes.
  bool using_xfixes_ = false;

  // The event base returned by XFixesQueryExtension().
  int xfixes_event_base_;

  // Notifies whenever clipboard sequence number is changed.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // State of selections served by this instance.
  base::flat_map<XAtom, std::unique_ptr<SelectionState>> selection_state_;

  DISALLOW_COPY_AND_ASSIGN(X11ClipboardOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
