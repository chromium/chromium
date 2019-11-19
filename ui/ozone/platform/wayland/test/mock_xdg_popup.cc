// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"

namespace wl {

namespace {

void Grab(struct wl_client* client,
          struct wl_resource* resource,
          struct wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgPopup>(resource)->Grab(serial);
}

}  // namespace

const struct xdg_popup_interface kXdgPopupImpl = {
    &DestroyResource,  // destroy
};

const struct zxdg_popup_v6_interface kZxdgPopupV6Impl = {
    &DestroyResource,  // destroy
    &Grab,             // grab
};

MockXdgPopup::MockXdgPopup(wl_resource* resource, const void* implementation)
    : ServerObject(resource) {
  SetImplementationUnretained(resource, implementation, this);
}

MockXdgPopup::~MockXdgPopup() {}

}  // namespace wl
