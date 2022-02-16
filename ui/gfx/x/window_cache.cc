// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/window_cache.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/x11_window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

ScopedShapeEventSelector::ScopedShapeEventSelector(Connection* connection,
                                                   Window window)
    : connection_(connection), window_(window) {
  connection_->shape().SelectInput(
      {.destination_window = window_, .enable = true});
}

ScopedShapeEventSelector::~ScopedShapeEventSelector() {
  connection_->shape().SelectInput(
      {.destination_window = window_, .enable = false});
}

WindowCache::WindowInfo::WindowInfo() = default;

WindowCache::WindowInfo::~WindowInfo() = default;

// static
WindowCache* WindowCache::instance_ = nullptr;

WindowCache::WindowCache(Connection* connection, Window root)
    : connection_(connection), root_(root) {
  DCHECK(!instance_) << "Only one WindowCache should be active at a time";
  instance_ = this;

  connection_->AddEventObserver(this);

  // We select for SubstructureNotify events on all windows (to receive
  // CreateNotify events), which will cause events to be sent for all child
  // windows.  This means we need to additionally select for StructureNotify
  // changes for the root window.
  root_events_ =
      std::make_unique<XScopedEventSelector>(root_, EventMask::StructureNotify);
  AddWindow(root_, Window::None);
}

WindowCache::~WindowCache() {
  connection_->RemoveEventObserver(this);

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void WindowCache::SyncForTest() {
  do {
    // Perform a blocking sync to prevent spinning while waiting for replies.
    connection_->Sync();
    connection_->DispatchAll();
  } while (pending_requests_);
}

void WindowCache::OnEvent(const Event& event) {
  // Ignore events sent by clients since the server will send everything
  // we need and client events may have different semantics (eg.
  // ConfigureNotifyEvents are parent-relative if sent by the server but
  // root-relative when sent by the WM).
  if (event.send_event())
    return;

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
        auto src = std::find(siblings->begin(), siblings->end(), window);
        auto dst = std::find(siblings->begin(), siblings->end(), above);
        auto end = siblings->end();
        if (src != end && (dst != end || above == Window::None)) {
          dst = above == Window::None ? siblings->begin() : ++dst;
          if (src < dst)
            std::rotate(src, src + 1, dst);
          else if (src > dst)
            std::rotate(dst, src, src + 1);
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
      if (auto* siblings = GetChildren(info->parent))
        base::Erase(*siblings, destroy->window);
      windows_.erase(destroy->window);
    }
  } else if (auto* map = event.As<MapNotifyEvent>()) {
    if (auto* info = GetInfo(map->window))
      info->mapped = true;
  } else if (auto* unmap = event.As<UnmapNotifyEvent>()) {
    if (auto* info = GetInfo(unmap->window))
      info->mapped = false;
  } else if (auto* reparent = event.As<ReparentNotifyEvent>()) {
    if (auto* info = GetInfo(reparent->window)) {
      if (auto* old_siblings = GetChildren(info->parent))
        base::Erase(*old_siblings, reparent->window);
      if (auto* new_siblings = GetChildren(reparent->parent))
        new_siblings->push_back(reparent->window);
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
        base::Erase(*siblings, circulate->window);
        if (circulate->place == Place::OnTop)
          siblings->push_back(circulate->window);
        else
          siblings->insert(siblings->begin(), circulate->window);
      }
    }
  } else if (auto* shape = event.As<Shape::NotifyEvent>()) {
    Window window = shape->affected_window;
    Shape::Sk kind = shape->shape_kind;
    if (base::Contains(windows_, window)) {
      connection_->shape()
          .GetRectangles(window, kind)
          .OnResponse(base::BindOnce(&WindowCache::OnGetRectanglesResponse,
                                     weak_factory_.GetWeakPtr(), window, kind));
      pending_requests_++;
    }
  }
}

void WindowCache::AddWindow(Window window, Window parent) {
  if (base::Contains(windows_, window))
    return;
  WindowInfo& info = windows_[window];
  info.parent = parent;
  // Events must be selected before getting the initial window info to prevent
  // race conditions.
  info.events = std::make_unique<XScopedEventSelector>(
      window, EventMask::SubstructureNotify);

  connection_->GetWindowAttributes(window).OnResponse(
      base::BindOnce(&WindowCache::OnGetWindowAttributesResponse,
                     weak_factory_.GetWeakPtr(), window));
  connection_->GetGeometry(window).OnResponse(base::BindOnce(
      &WindowCache::OnGetGeometryResponse, weak_factory_.GetWeakPtr(), window));
  connection_->QueryTree(window).OnResponse(base::BindOnce(
      &WindowCache::OnQueryTreeResponse, weak_factory_.GetWeakPtr(), window));
  pending_requests_ += 3;

  auto& shape = connection_->shape();
  if (shape.present()) {
    info.shape_events =
        std::make_unique<ScopedShapeEventSelector>(connection_, window);

    for (auto kind : {Shape::Sk::Bounding, Shape::Sk::Input}) {
      shape.GetRectangles(window, kind)
          .OnResponse(base::BindOnce(&WindowCache::OnGetRectanglesResponse,
                                     weak_factory_.GetWeakPtr(), window, kind));
      pending_requests_++;
    }
  }
}

WindowCache::WindowInfo* WindowCache::GetInfo(Window window) {
  auto it = windows_.find(window);
  if (it == windows_.end())
    return nullptr;
  return &it->second;
}

std::vector<Window>* WindowCache::GetChildren(Window window) {
  if (auto* info = GetInfo(window))
    return &info->children;
  return nullptr;
}

WindowCache::WindowInfo* WindowCache::OnResponse(Window window,
                                                 bool has_reply) {
  pending_requests_--;
  if (!has_reply) {
    windows_.erase(window);
    return nullptr;
  }
  auto it = windows_.find(window);
  if (it == windows_.end())
    return nullptr;
  return &it->second;
}

void WindowCache::OnGetWindowAttributesResponse(
    Window window,
    GetWindowAttributesResponse response) {
  if (auto* info = OnResponse(window, response.reply.get()))
    info->mapped = response->map_state != MapState::Unmapped;
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
    for (auto child : info->children)
      AddWindow(child, window);
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
        NOTREACHED();
        break;
      case Shape::Sk::Input:
        info->input_rects_px = std::move(response->rectangles);
        break;
    }
  }
}

}  // namespace x11
