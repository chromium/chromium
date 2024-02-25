// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/os_exchange_data_provider_x11.h"

#include <utility>

#include "base/check.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

OSExchangeDataProviderX11::OSExchangeDataProviderX11(
    x11::Window x_window,
    x11::Window source_window,
    const SelectionFormatMap& selection)
    : XOSExchangeDataProvider(x_window, source_window, selection) {}

OSExchangeDataProviderX11::OSExchangeDataProviderX11() {
  x11::Connection::Get()->AddEventObserver(this);
}

OSExchangeDataProviderX11::~OSExchangeDataProviderX11() {
  if (own_window())
    x11::Connection::Get()->RemoveEventObserver(this);
}

std::unique_ptr<OSExchangeDataProvider> OSExchangeDataProviderX11::Clone()
    const {
  std::unique_ptr<OSExchangeDataProviderX11> ret(
      new OSExchangeDataProviderX11());
  ret->set_format_map(format_map());
  return std::move(ret);
}

void OSExchangeDataProviderX11::OnEvent(const x11::Event& xev) {
  auto* selection = xev.As<x11::SelectionRequestEvent>();
  if (selection && selection->owner == x_window())
    selection_owner().OnSelectionRequest(*selection);
}

void OSExchangeDataProviderX11::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {}

DataTransferEndpoint* OSExchangeDataProviderX11::GetSource() const {
  return nullptr;
}

}  // namespace ui
