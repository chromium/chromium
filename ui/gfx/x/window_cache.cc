// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/window_cache.h"

#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

const base::TimeDelta kDestroyTimerInterval = base::Seconds(3);

Window GetWindowAtPoint(const gfx::Point& point_px,
                        const base::flat_set<Window>* ignore) {
  auto* connection = Connection::Get();
  Window root = connection->default_root();

  if (!WindowCache::instance()) {
    auto instance =
        std::make_unique<WindowCache>(connection, connection->default_root());
    auto* cache = instance.get();
    cache->BeginDestroyTimer(std::move(instance));
  }

  auto* instance = WindowCache::instance();
  instance->WaitUntilReady();
  return instance->GetWindowAtPoint(point_px, root, ignore);
}

ScopedShapeEventSelector::ScopedShapeEventSelector(Connection* connection,
                                                   Window window)
    : connection_(connection), window_(window) {
  connection_->shape()
      .SelectInput({.destination_window = window_, .enable = true})
      .IgnoreError();
}

ScopedShapeEventSelector::~ScopedShapeEventSelector() {
  connection_->shape()
      .SelectInput({.destination_window = window_, .enable = false})
      .IgnoreError();
}

WindowCache::WindowInfo::WindowInfo() = default;

WindowCache::WindowInfo::~WindowInfo() = default;

// static
WindowCache* WindowCache::instance_ = nullptr;

WindowCache::WindowCache(Connection* connection, Window root)
    : connection_(connection),
      root_(root),
      gtk_frame_extents_(GetAtom("_GTK_FRAME_EXTENTS")) {
  CHECK(!instance_) << "Only one WindowCache should be active at a time";
  instance_ = this;

  connection_->AddEventObserver(this);

  // We select for SubstructureNotify events on all windows (to receive
  // CreateNotify events), which will cause events to be sent for all child
  // windows.  This means we need to additionally select for StructureNotify
  // changes for the root window.
  root_events_ =
      connection_->ScopedSelectEvent(root_, EventMask::StructureNotify);
  AddWindow(root_, Window::None);
}

WindowCache::~WindowCache() {
  connection_->RemoveEventObserver(this);

  CHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void WindowCache::WaitUntilReady() {
  auto& events = connection_->events();
  size_t event = 0;
  while (!pending_requests_.empty()) {
    connection_->Flush();
    for (size_t pending = pending_requests_.size(); pending;) {
      if (event < events.size() &&
          pending_requests_.front().AfterEvent(events[event])) {
        OnEvent(events[event++]);
      } else {
        pending_requests_.front().DispatchNow();
        --pending;
      }
    }
  }
  if (event) {
    last_processed_event_ = events[event - 1].sequence();
  }
}

void WindowCache::BeginDestroyTimer(std::unique_ptr<WindowCache> self) {
  CHECK_EQ(this, self.get());
  delete_when_destroy_timer_fires_ = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WindowCache::OnDestroyTimerExpired,
                     base::Unretained(this), std::move(self)),
      kDestroyTimerInterval);
}

void WindowCache::SyncForTest() {
  do {
    // Perform a blocking sync to prevent spinning while waiting for replies.
    connection_->Sync();
    connection_->DispatchAll();
  } while (!pending_requests_.empty());
}

Window WindowCache::GetWindowAtPoint(gfx::Point point_px,
                                     Window window,
                                     const base::flat_set<Window>* ignore) {
  delete_when_destroy_timer_fires_ = true;
  if (ignore && ignore->contains(window)) {
    return Window::None;
  }
  auto* info = GetInfo(window);
  if (!info || !info->mapped) {
    return Window::None;
  }

  gfx::Rect rect(info->x_px, info->y_px, info->width_px, info->height_px);
  rect.Outset(info->border_width_px);
  rect.Inset(info->gtk_frame_extents_px);
  if (!rect.Contains(point_px)) {
    return Window::None;
  }

  point_px -= gfx::Vector2d(info->x_px, info->y_px);
  if (info->bounding_rects_px && info->input_rects_px) {
    for (const auto& rects : {info->bounding_rects_px, info->input_rects_px}) {
      if (!base::ranges::any_of(*rects, [&point_px](const Rectangle& x_rect) {
            gfx::Rect rect{x_rect.x, x_rect.y, x_rect.width, x_rect.height};
            return rect.Contains(point_px);
          })) {
        return Window::None;
      }
    }
  }

  for (Window child : base::Reversed(info->children)) {
    Window ret = GetWindowAtPoint(point_px, child, ignore);
    if (ret != Window::None) {
      return ret;
    }
  }
  if (info->has_wm_name) {
    return window;
  }
  return Window::None;
}

void WindowCache::OnEvent(const Event& event) {
  // Ignore events that we've already processed.
  if (last_processed_event_ &&
      CompareSequenceIds(event.sequence(), *last_processed_event_) <= 0) {
    return;
  }
  last_processed_event_ = std::nullopt;

  // Ignore events sent by clients since the server will send everything
  // we need and client events may have different semantics (eg.
  // ConfigureNotifyEvents are parent-relative if sent by the server but
  // root-relative when sent by the WM).
  if (event.send_event()) {
    return;
  }

  if (auto* configure = event.As<ConfigureNotifyEvent>()) {
    if (auto* info = GetInfo(configure->window)) {
      info->x_px = configure->x;
      info->y_px = configure->y;
      info->width_px = configure->width;
      info->height_px = configure->height;
      info->border_width_px = configure->border_width;
      if (auto* siblings = GetChildren(info->parent)) {
        Window window = configure->window;
        Window above = configure->above_sibling;
        auto src = base::ranges::find(*siblings, window);
        auto dst = base::ranges::find(*siblings, above);
        auto end = siblings->end();
        if (src != end && (dst != end || above == Window::None)) {
          dst = above == Window::None ? siblings->begin() : ++dst;
          if (src < dst) {
            std::rotate(src, src + 1, dst);
          } else if (src > dst) {
            std::rotate(dst, src, src + 1);
          }
        }
      }
    }
  } else if (auto* property = event.As<PropertyNotifyEvent>()) {
    if (auto* info = GetInfo(property->window)) {
      if (property->atom == Atom::WM_NAME) {
        info->has_wm_name = property->state != Property::Delete;
      } else if (property->atom == gtk_frame_extents_) {
        if (property->state == Property::Delete) {
          info->gtk_frame_extents_px = gfx::Insets();
        } else {
          GetProperty(property->window, gtk_frame_extents_, 4);
        }
      }
    }
  } else if (auto* create = event.As<CreateNotifyEvent>()) {
    if (auto* info = GetInfo(create->parent)) {
      info->children.push_back(create->window);
      AddWindow(create->window, create->parent);
    }
  } else if (auto* destroy = event.As<DestroyNotifyEvent>()) {
    if (auto* info = GetInfo(destroy->window)) {
      if (auto* siblings = GetChildren(info->parent)) {
        std::erase(*siblings, destroy->window);
      }
      windows_.erase(destroy->window);
    }
  } else if (auto* map = event.As<MapNotifyEvent>()) {
    if (auto* info = GetInfo(map->window)) {
      info->mapped = true;
    }
  } else if (auto* unmap = event.As<UnmapNotifyEvent>()) {
    if (auto* info = GetInfo(unmap->window)) {
      info->mapped = false;
    }
  } else if (auto* reparent = event.As<ReparentNotifyEvent>()) {
    if (auto* info = GetInfo(reparent->window)) {
      if (auto* old_siblings = GetChildren(info->parent)) {
        std::erase(*old_siblings, reparent->window);
      }
      if (auto* new_siblings = GetChildren(reparent->parent)) {
        new_siblings->push_back(reparent->window);
      }
      info->parent = reparent->parent;
    }
  } else if (auto* gravity = event.As<GravityNotifyEvent>()) {
    if (auto* info = GetInfo(gravity->window)) {
      info->x_px = gravity->x;
      info->y_px = gravity->y;
    }
  } else if (auto* circulate = event.As<CirculateEvent>()) {
    if (auto* info = GetInfo(circulate->window)) {
      if (auto* siblings = GetChildren(info->parent)) {
        std::erase(*siblings, circulate->window);
        if (circulate->place == Place::OnTop) {
          siblings->push_back(circulate->window);
        } else {
          siblings->insert(siblings->begin(), circulate->window);
        }
      }
    }
  } else if (auto* shape = event.As<Shape::NotifyEvent>()) {
    Window window = shape->affected_window;
    Shape::Sk kind = shape->shape_kind;
    if (kind != Shape::Sk::Clip && base::Contains(windows_, window)) {
      AddRequest(connection_->shape().GetRectangles(window, kind),
                 &WindowCache::OnGetRectanglesResponse, window, kind);
    }
  }
}

void WindowCache::AddWindow(Window window, Window parent) {
  if (base::Contains(windows_, window)) {
    return;
  }
  WindowInfo& info = windows_[window];
  info.parent = parent;
  // Events must be selected before getting the initial window info to
  // prevent race conditions.
  info.events = connection_->ScopedSelectEvent(
      window, EventMask::SubstructureNotify | EventMask::PropertyChange);

  AddRequest(connection_->GetWindowAttributes(window),
             &WindowCache::OnGetWindowAttributesResponse, window);
  AddRequest(connection_->GetGeometry(window),
             &WindowCache::OnGetGeometryResponse, window);
  AddRequest(connection_->QueryTree(window), &WindowCache::OnQueryTreeResponse,
             window);

  GetProperty(window, Atom::WM_NAME, 1);
  GetProperty(window, gtk_frame_extents_, 4);

  auto& shape = connection_->shape();
  if (shape.present()) {
    info.shape_events =
        std::make_unique<ScopedShapeEventSelector>(connection_, window);

    for (auto kind : {Shape::Sk::Bounding, Shape::Sk::Input}) {
      AddRequest(shape.GetRectangles(window, kind),
                 &WindowCache::OnGetRectanglesResponse, window, kind);
    }
  }
}

WindowCache::WindowInfo* WindowCache::GetInfo(Window window) {
  auto it = windows_.find(window);
  if (it == windows_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::vector<Window>* WindowCache::GetChildren(Window window) {
  if (auto* info = GetInfo(window)) {
    return &info->children;
  }
  return nullptr;
}

void WindowCache::GetProperty(Window window, Atom property, uint32_t length) {
  AddRequest(
      connection_->GetProperty(
          {.window = window, .property = property, .long_length = length}),
      &WindowCache::OnGetPropertyResponse, window, property);
}

WindowCache::WindowInfo* WindowCache::OnResponse(Window window,
                                                 bool has_reply) {
  pending_requests_.pop_front();
  if (!has_reply) {
    windows_.erase(window);
    return nullptr;
  }
  auto it = windows_.find(window);
  if (it == windows_.end()) {
    return nullptr;
  }
  return &it->second;
}

void WindowCache::OnGetWindowAttributesResponse(
    Window window,
    GetWindowAttributesResponse response) {
  if (auto* info = OnResponse(window, response.reply.get())) {
    info->mapped = response->map_state != MapState::Unmapped;
  }
}

void WindowCache::OnGetGeometryResponse(Window window,
                                        GetGeometryResponse response) {
  if (auto* info = OnResponse(window, response.reply.get())) {
    info->x_px = response->x;
    info->y_px = response->y;
    info->width_px = response->width;
    info->height_px = response->height;
  }
}

void WindowCache::OnQueryTreeResponse(Window window,
                                      QueryTreeResponse response) {
  if (auto* info = OnResponse(window, response.reply.get())) {
    info->parent = response->parent;
    info->children = std::move(response->children);
    for (auto child : info->children) {
      AddWindow(child, window);
    }
  }
}

void WindowCache::OnGetPropertyResponse(Window window,
                                        Atom atom,
                                        GetPropertyResponse response) {
  if (auto* info = OnResponse(window, response.reply.get())) {
    if (atom == Atom::WM_NAME) {
      info->has_wm_name = response->format;
    } else if (atom == gtk_frame_extents_) {
      if (response->format == CHAR_BIT * sizeof(int32_t) &&
          response->value_len == 4) {
        const int32_t* frame_extents = response->value->cast_to<int32_t>();
        // This is safe: we've checked (in the condition above) that the
        // response contains four int32_ts. It would be nice if instead
        // GetPropertyResponse had a way to convert its value safely into a
        // span<T> for some T.
        UNSAFE_BUFFERS(info->gtk_frame_extents_px = gfx::Insets::TLBR(
                           frame_extents[2], frame_extents[0], frame_extents[3],
                           frame_extents[1]));
      } else {
        info->gtk_frame_extents_px = gfx::Insets();
      }
    }
  }
}

void WindowCache::OnGetRectanglesResponse(
    Window window,
    Shape::Sk kind,
    Shape::GetRectanglesResponse response) {
  if (auto* info = OnResponse(window, response.reply.get())) {
    switch (kind) {
      case Shape::Sk::Bounding:
        info->bounding_rects_px = std::move(response->rectangles);
        break;
      case Shape::Sk::Clip:
        NOTREACHED_IN_MIGRATION();
        break;
      case Shape::Sk::Input:
        info->input_rects_px = std::move(response->rectangles);
        break;
    }
  }
}

void WindowCache::OnDestroyTimerExpired(std::unique_ptr<WindowCache> self) {
  if (!delete_when_destroy_timer_fires_) {
    return;  // destroy `this`
  }

  BeginDestroyTimer(std::move(self));
}

}  // namespace x11
