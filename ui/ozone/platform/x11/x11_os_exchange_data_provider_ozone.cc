// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_os_exchange_data_provider_ozone.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/base/x/selection_utils.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

X11OSExchangeDataProviderOzone::X11OSExchangeDataProviderOzone(
    x11::Window x_window,
    const SelectionFormatMap& selection)
    : XOSExchangeDataProvider(x_window, selection) {}

X11OSExchangeDataProviderOzone::X11OSExchangeDataProviderOzone() {
  DCHECK(own_window());
  x11::Connection::Get()->AddEventObserver(this);
}

X11OSExchangeDataProviderOzone::~X11OSExchangeDataProviderOzone() {
  if (own_window())
    x11::Connection::Get()->RemoveEventObserver(this);
}

std::unique_ptr<OSExchangeDataProvider> X11OSExchangeDataProviderOzone::Clone()
    const {
  std::unique_ptr<X11OSExchangeDataProviderOzone> ret(
      new X11OSExchangeDataProviderOzone());
  ret->set_format_map(format_map());
  return std::move(ret);
}

void X11OSExchangeDataProviderOzone::OnEvent(const x11::Event& xev) {
  auto* selection_request = xev.As<x11::SelectionRequestEvent>();
  if (selection_request && selection_request->owner == x_window())
    selection_owner().OnSelectionRequest(*selection_request);
}

}  // namespace ui
