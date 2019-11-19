// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_OFFER_BASE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_OFFER_BASE_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"

namespace ui {
namespace internal {

// Implements common part of WaylandDataOffer and GtkPrimarySelectionOffer
// (which is handling of the clipboard data).
class WaylandDataOfferBase {
 public:
  WaylandDataOfferBase();
  virtual ~WaylandDataOfferBase();

  const std::vector<std::string>& mime_types() const { return mime_types_; }
  bool text_plain_mime_type_inserted() const {
    return text_plain_mime_type_inserted_;
  }

  // Some X11 applications on Gnome/Wayland (running through XWayland)
  // do not send the "text/plain" MIME type that Chrome relies on, but
  // instead they send types like "text/plain;charset=utf-8".
  // When it happens, this method forcibly injects "text/plain" into the
  // list of provided MIME types so that Chrome clipboard's machinery
  // works fine.
  void EnsureTextMimeTypeIfNeeded();

  // Inserts the specified MIME type into the internal list.
  void AddMimeType(const char* mime_type);

  // Creates a pipe (read & write FDs), passes the write end of the pipe
  // to the compositor and returns the read end.
  virtual base::ScopedFD Receive(const std::string& mime_type) = 0;

 private:
  // MIME types provided in this offer.
  std::vector<std::string> mime_types_;

  // Whether "text/plain" had been inserted forcibly.
  bool text_plain_mime_type_inserted_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOfferBase);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_OFFER_BASE_H_
