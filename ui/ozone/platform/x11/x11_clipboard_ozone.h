// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"
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
class X11ClipboardOzone : public PlatformClipboard, public x11::EventObserver {
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
  bool IsSelectionBufferAvailable() const override;

 private:
  struct SelectionState;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  void OnSelectionRequest(const x11::SelectionRequestEvent& event);
  void OnSelectionNotify(const x11::SelectionNotifyEvent& event);
  void OnSetSelectionOwnerNotify(
      const x11::XFixes::SelectionNotifyEvent& event);

  // Returns an X atom for a clipboard buffer type.
  x11::Atom SelectionAtomForBuffer(ClipboardBuffer buffer) const;

  // Returns a clipboard buffer type for an X atom for a selection name of the
  // system clipboard buffer.
  ClipboardBuffer BufferForSelectionAtom(x11::Atom selection) const;

  // Returns the state for the given selection;
  SelectionState& GetSelectionState(x11::Atom selection);

  // Queries the current clipboard owner for what mime types are available by
  // sending XConvertSelection with target=TARGETS.  After sending this, we
  // will receive a SelectionNotify event with xselection.target=TARGETS which
  // is processed in |OnSelectionNotify|.
  void QueryTargets(x11::Atom selection);

  // Reads the contents of the remote clipboard by sending XConvertSelection
  // with target=<mime-type>.  After sending this, we will receive a
  // SelectionNotify event with xselection.target=<mime-type> which is processed
  // in |OnSelectionNotify|.
  void ReadRemoteClipboard(x11::Atom selection);

  // Local cache of atoms.
  const x11::Atom atom_clipboard_;
  const x11::Atom atom_targets_;
  const x11::Atom atom_timestamp_;

  // The property on |x_window_| which will receive remote clipboard contents.
  const x11::Atom x_property_;

  // Our X11 state.
  x11::Connection* connection_;

  // Input-only window used as a selection owner.
  const x11::Window x_window_;

  // If XFixes is unavailable, this clipboard window will not register to
  // receive events and no processing will take place.
  // TODO(joelhockey): Make clipboard work without xfixes.
  bool using_xfixes_ = false;

  // Notifies whenever clipboard sequence number is changed.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // State of selections served by this instance.
  base::flat_map<x11::Atom, std::unique_ptr<SelectionState>> selection_state_;

  DISALLOW_COPY_AND_ASSIGN(X11ClipboardOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
