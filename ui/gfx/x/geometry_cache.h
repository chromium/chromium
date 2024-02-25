// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_GEOMETRY_CACHE_H_
#define UI_GFX_X_GEOMETRY_CACHE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Keeps track of the geometry of a window relative to the root window.
class COMPONENT_EXPORT(X11) GeometryCache final : public EventObserver {
 public:
  using BoundsChangedCallback =
      base::RepeatingCallback<void(const std::optional<gfx::Rect>&,
                                   const gfx::Rect&)>;

  GeometryCache(Connection* connection,
                Window window,
                BoundsChangedCallback bounds_changed_callback);

  GeometryCache(const GeometryCache&) = delete;
  GeometryCache& operator=(const GeometryCache&) = delete;

  ~GeometryCache() override;

  gfx::Rect GetBoundsPx();

 private:
  void OnQueryTreeResponse(QueryTreeResponse response);
  void OnGetGeometryResponse(GetGeometryResponse response);

  void OnParentChanged(Window parent, const gfx::Point& position);
  void OnGeometryChanged(const gfx::Rect& geometry);
  void OnPositionChanged(const gfx::Point& origin);

  bool Ready() const;

  void OnParentGeometryChanged(
      const std::optional<gfx::Rect>& old_parent_bounds,
      const gfx::Rect& new_parent_bounds);

  void NotifyGeometryChanged();

  // EventObserver:
  void OnEvent(const Event& xevent) override;

  const raw_ptr<Connection> connection_;
  const Window window_;
  const BoundsChangedCallback bounds_changed_callback_;

  Future<QueryTreeReply> parent_future_;
  Future<GetGeometryReply> geometry_future_;
  bool have_parent_ = false;
  bool have_geometry_ = false;
  std::unique_ptr<GeometryCache> parent_;
  gfx::Rect geometry_;
  std::optional<gfx::Rect> last_notified_geometry_ = std::nullopt;

  ScopedEventSelector window_events_;

  base::ScopedObservation<Connection, EventObserver> scoped_observation_{this};

  base::WeakPtrFactory<GeometryCache> weak_ptr_factory_{this};
};

}  // namespace x11

#endif  // UI_GFX_X_GEOMETRY_CACHE_H_
