// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/geometry_cache.h"

#include <memory>

#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

GeometryCache::GeometryCache(Connection* connection,
                             Window window,
                             BoundsChangedCallback bounds_changed_callback)
    : connection_(connection),
      window_(window),
      bounds_changed_callback_(bounds_changed_callback) {
  scoped_observation_.Observe(connection_);
  window_events_ =
      connection_->ScopedSelectEvent(window_, EventMask::StructureNotify);

  // Unretained is safe since we disconnect the futures in the destructor.
  parent_future_ = connection_->QueryTree(window_);
  parent_future_.OnResponse(base::BindOnce(&GeometryCache::OnQueryTreeResponse,
                                           base::Unretained(this)));
  geometry_future_ = connection_->GetGeometry(window_);
  geometry_future_.OnResponse(base::BindOnce(
      &GeometryCache::OnGetGeometryResponse, base::Unretained(this)));
}

GeometryCache::~GeometryCache() {
  if (!have_parent_) {
    parent_future_.IgnoreError();
  }
  if (!have_geometry_) {
    geometry_future_.IgnoreError();
  }
}

gfx::Rect GeometryCache::GetBoundsPx() {
  if (!have_parent_) {
    parent_future_.DispatchNow();
  }
  CHECK(have_parent_);
  if (!have_geometry_) {
    geometry_future_.DispatchNow();
  }
  CHECK(have_geometry_);

  if (!parent_) {
    return geometry_;
  }
  auto parent_bounds = parent_->GetBoundsPx();
  gfx::Vector2d offset(parent_bounds.x(), parent_bounds.y());
  return geometry_ + offset;
}

void GeometryCache::OnQueryTreeResponse(QueryTreeResponse response) {
  OnParentChanged(response ? response->parent : Window::None,
                  geometry_.origin());
}

void GeometryCache::OnGetGeometryResponse(GetGeometryResponse response) {
  OnGeometryChanged(response ? gfx::Rect(response->x, response->y,
                                         response->width, response->height)
                             : gfx::Rect());
}

void GeometryCache::OnParentChanged(Window parent, const gfx::Point& position) {
  const bool was_ready = Ready();
  bool parent_changed = true;

  have_parent_ = true;
  if (parent == Window::None) {
    parent_.reset();
  } else if (!parent_ || parent_->window_ != parent) {
    // Unretained is safe since we own `parent_`.
    parent_ = std::make_unique<GeometryCache>(
        connection_, parent,
        base::BindRepeating(&GeometryCache::OnParentGeometryChanged,
                            base::Unretained(this)));
  } else {
    parent_changed = false;
  }

  const bool position_changed = position != geometry_.origin();
  geometry_.set_origin(position);

  if (Ready() && (!was_ready || parent_changed || position_changed)) {
    bounds_changed_callback_.Run(geometry_);
  }
}

void GeometryCache::OnGeometryChanged(const gfx::Rect& geometry) {
  const bool was_ready = Ready();
  const bool geometry_changed = geometry_ != geometry;

  have_geometry_ = true;
  geometry_ = geometry;

  if (Ready() && (!was_ready || geometry_changed)) {
    bounds_changed_callback_.Run(geometry_);
  }
}

bool GeometryCache::Ready() const {
  return have_geometry_ && have_parent_ && (!parent_ || parent_->Ready());
}

void GeometryCache::OnParentGeometryChanged(const gfx::Rect& parent_bounds) {
  if (have_geometry_) {
    gfx::Vector2d offset(parent_bounds.x(), parent_bounds.y());
    bounds_changed_callback_.Run(geometry_ + offset);
  }
}

void GeometryCache::OnEvent(const Event& xevent) {
  // Ignore client events.
  if (xevent.send_event()) {
    return;
  }

  if (auto* configure = xevent.As<ConfigureNotifyEvent>()) {
    if (configure->window == window_) {
      OnGeometryChanged(gfx::Rect(configure->x, configure->y, configure->width,
                                  configure->height));
    }
  } else if (auto* reparent = xevent.As<ReparentNotifyEvent>()) {
    if (reparent->window == window_) {
      OnParentChanged(reparent->parent, gfx::Point(reparent->x, reparent->y));
    }
  }
}

}  // namespace x11
