// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/property_cache.h"

#include <limits>

#include "base/check_op.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

PropertyCache::PropertyCache(x11::Connection* connection,
                             x11::Window window,
                             const std::vector<x11::Atom>& properties)
    : connection_(connection),
      window_(window),
      event_selector_(window_, x11::EventMask::PropertyChange) {
  connection_->AddEventObserver(this);

  std::vector<std::pair<x11::Atom, PropertyValue>> mem(properties.size());
  for (size_t i = 0; i < properties.size(); i++) {
    mem[i].first = properties[i];
    FetchProperty(properties[i], &mem[i].second);
  }
  properties_ = base::flat_map<x11::Atom, PropertyValue>(std::move(mem));
}

PropertyCache::~PropertyCache() {
  connection_->RemoveEventObserver(this);
}

const x11::GetPropertyResponse& PropertyCache::GetProperty(x11::Atom atom) {
  auto it = properties_.find(atom);
  DCHECK(it != properties_.end());

  if (!it->second.response.has_value())
    it->second.future.Wait();
  DCHECK(it->second.response.has_value());

  return it->second.response.value();
}

void PropertyCache::OnEvent(const x11::Event& xev) {
  auto* prop = xev.As<x11::PropertyNotifyEvent>();
  if (!prop)
    return;
  if (prop->window != window_)
    return;
  auto it = properties_.find(prop->atom);
  if (it == properties_.end())
    return;
  FetchProperty(it->first, &it->second);
}

void PropertyCache::FetchProperty(x11::Atom property, PropertyValue* value) {
  value->future = connection_->GetProperty({
      .window = window_,
      .property = property,
      .long_length = std::numeric_limits<uint32_t>::max(),
  });
  value->future.OnResponse(base::BindOnce(&PropertyCache::OnGetPropertyResponse,
                                          weak_factory_.GetWeakPtr(), value));
}

void PropertyCache::OnGetPropertyResponse(PropertyValue* value,
                                          x11::GetPropertyResponse response) {
  value->response = std::move(response);
}

PropertyCache::PropertyValue::PropertyValue() = default;

PropertyCache::PropertyValue::PropertyValue(PropertyValue&&) = default;
PropertyCache::PropertyValue& PropertyCache::PropertyValue::operator=(
    PropertyValue&&) = default;

PropertyCache::PropertyValue::~PropertyValue() = default;

}  // namespace x11
