// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_PROPERTY_CACHE_H_
#define UI_BASE_X_PROPERTY_CACHE_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// Watches properties on an X11 window.  Property values are obtained once upon
// creation and are refreshed after each property change.
class COMPONENT_EXPORT(UI_BASE_X) PropertyCache : public XEventObserver {
 public:
  PropertyCache(x11::Connection* connection,
                x11::Window window,
                const std::vector<x11::Atom>& properties);

  PropertyCache(const PropertyCache&) = delete;
  PropertyCache& operator=(const PropertyCache&) = delete;

  ~PropertyCache() override;

  const x11::GetPropertyResponse& GetProperty(x11::Atom atom);

 private:
  struct PropertyValue {
    PropertyValue();

    PropertyValue(PropertyValue&&);
    PropertyValue& operator=(PropertyValue&&);

    ~PropertyValue();

    x11::Future<x11::GetPropertyReply> future;
    // |response| is nullopt if the request hasn't yet finished.
    base::Optional<x11::GetPropertyResponse> response = base::nullopt;
  };

  // ui::XEventObserver:
  void WillProcessXEvent(x11::Event* xev) override;

  void FetchProperty(x11::Atom property, PropertyValue* value);

  void OnGetPropertyResponse(PropertyValue* value,
                             x11::GetPropertyResponse response);

  x11::Connection* connection_;
  x11::Window window_;
  XScopedEventSelector event_selector_;
  base::flat_map<x11::Atom, PropertyValue> properties_;

  base::WeakPtrFactory<PropertyCache> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_PROPERTY_CACHE_H_
