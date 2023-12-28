// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/property_cache.h"

#include <limits>

#include "base/check_op.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

PropertyCache::PropertyCache(Connection* connection,
                             Window window,
                             const std::vector<Atom>& properties,
                             OnChangeCallback on_change)
    : connection_(connection),
      window_(window),
      event_selector_(
          connection->ScopedSelectEvent(window_, EventMask::PropertyChange)),
      on_change_(std::move(on_change)) {
  connection_->AddEventObserver(this);

  std::vector<std::pair<Atom, PropertyValue>> mem(properties.size());
  for (size_t i = 0; i < properties.size(); i++) {
    mem[i].first = properties[i];
  }
  properties_ = base::flat_map<Atom, PropertyValue>(std::move(mem));
  for (auto it = properties_.begin(); it != properties_.end(); ++it) {
    FetchProperty(it);
  }
}

PropertyCache::~PropertyCache() {
  connection_->RemoveEventObserver(this);
}

const GetPropertyResponse& PropertyCache::Get(Atom atom) {
  auto it = properties_.find(atom);
  CHECK(it != properties_.end());

  if (!it->second.response.has_value()) {
    it->second.future.DispatchNow();
  }
  CHECK(it->second.response.has_value());

  return it->second.response.value();
}

void PropertyCache::OnEvent(const Event& xev) {
  auto* prop = xev.As<PropertyNotifyEvent>();
  if (!prop) {
    return;
  }
  if (prop->window != window_) {
    return;
  }
  auto it = properties_.find(prop->atom);
  if (it == properties_.end()) {
    return;
  }
  if (prop->state == Property::NewValue) {
    FetchProperty(it);
  } else {
    CHECK_EQ(prop->state, Property::Delete);
    // When the property is deleted, a GetPropertyRequest will result in a
    // zeroed GetPropertyReply, so set the reply state now to avoid making an
    // unnecessary request.
    OnGetPropertyResponse(
        it, GetPropertyResponse{std::make_unique<GetPropertyReply>(), nullptr});
  }
}

void PropertyCache::FetchProperty(PropertiesIterator it) {
  it->second.future =
      static_cast<XProto*>(connection_)
          ->GetProperty({
              .window = window_,
              .property = it->first,
              .long_length = std::numeric_limits<uint32_t>::max(),
          });
  it->second.future.OnResponse(base::BindOnce(
      &PropertyCache::OnGetPropertyResponse, weak_factory_.GetWeakPtr(), it));
}

void PropertyCache::OnGetPropertyResponse(PropertiesIterator it,
                                          GetPropertyResponse response) {
  it->second.response = std::move(response);
  if (on_change_) {
    on_change_.Run(it->first, it->second.response.value());
  }
}

PropertyCache::PropertyValue::PropertyValue() = default;

PropertyCache::PropertyValue::PropertyValue(PropertyValue&&) = default;
PropertyCache::PropertyValue& PropertyCache::PropertyValue::operator=(
    PropertyValue&&) = default;

PropertyCache::PropertyValue::~PropertyValue() = default;

}  // namespace x11
