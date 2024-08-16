// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_PROPERTY_CACHE_H_
#define UI_GFX_X_PROPERTY_CACHE_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Watches properties on an X11 window.  Property values are obtained once upon
// creation and are refreshed after each property change.
class COMPONENT_EXPORT(X11) PropertyCache : public EventObserver {
 public:
  using OnChangeCallback =
      base::RepeatingCallback<void(Atom, const GetPropertyResponse&)>;

  PropertyCache(Connection* connection,
                Window window,
                const std::vector<Atom>& properties,
                OnChangeCallback on_change = {});

  PropertyCache(const PropertyCache&) = delete;
  PropertyCache& operator=(const PropertyCache&) = delete;

  ~PropertyCache() override;

  const GetPropertyResponse& Get(Atom atom);

  template <typename T>
  static const T* GetAs(const GetPropertyResponse& response,
                        size_t* size = nullptr) {
    if (size) {
      *size = 0;
    }
    if (!response || response->format != CHAR_BIT * sizeof(T) ||
        !response->value_len) {
      return nullptr;
    }
    if (size) {
      *size = response->value_len;
    }
    return response->value->cast_to<T>();
  }

  template <typename T>
  const T* GetAs(Atom atom, size_t* size = nullptr) {
    return GetAs<T>(Get(atom), size);
  }

  template <typename T>
  static base::span<const T> GetAsSpan(const GetPropertyResponse& response) {
    size_t len;
    const T* const data = GetAs<const T>(response, &len);
    if (data && len > 0) {
      UNSAFE_TODO(return base::span<const T>(data, len));
    } else {
      return base::span<const T>();
    }
  }

  template <typename T>
  base::span<const T> GetAsSpan(Atom atom) {
    return GetAsSpan<T>(Get(atom));
  }

 private:
  friend class PropertyCacheTest;

  struct PropertyValue {
    PropertyValue();

    PropertyValue(PropertyValue&&);
    PropertyValue& operator=(PropertyValue&&);

    ~PropertyValue();

    Future<GetPropertyReply> future;
    // |response| is nullopt if the request hasn't yet finished.
    std::optional<GetPropertyResponse> response = std::nullopt;
  };

  using PropertiesIterator = base::flat_map<Atom, PropertyValue>::iterator;

  // EventObserver:
  void OnEvent(const Event& xev) override;

  void FetchProperty(PropertiesIterator it);

  void OnGetPropertyResponse(PropertiesIterator it,
                             GetPropertyResponse response);

  raw_ptr<Connection> connection_;
  Window window_;
  ScopedEventSelector event_selector_;
  base::flat_map<Atom, PropertyValue> properties_;
  OnChangeCallback on_change_;

  base::WeakPtrFactory<PropertyCache> weak_factory_{this};
};

}  // namespace x11

#endif  // UI_GFX_X_PROPERTY_CACHE_H_
