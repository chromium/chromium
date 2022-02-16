// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_WINDOW_CACHE_H_
#define UI_GFX_X_WINDOW_CACHE_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;
class XScopedEventSelector;

class ScopedShapeEventSelector {
 public:
  ScopedShapeEventSelector(Connection* connection, Window window);
  ~ScopedShapeEventSelector();

 private:
  Connection* const connection_;
  const Window window_;
};

// Maintains a cache of the state of all X11 windows.
class COMPONENT_EXPORT(X11) WindowCache : public EventObserver {
 public:
  struct WindowInfo {
    WindowInfo();
    ~WindowInfo();

    Window parent = Window::None;
    bool mapped = false;

    int16_t x_px = 0;
    int16_t y_px = 0;
    uint16_t width_px = 0;
    uint16_t height_px = 0;
    uint16_t border_width_px = 0;

    // Child windows listed in lowest-to-highest stacking order.
    // Although it is possible to restack windows, it is uncommon to do so,
    // so we store children in a vector instead of a node-based structure.
    std::vector<Window> children;

    absl::optional<std::vector<Rectangle>> bounding_rects_px;
    absl::optional<std::vector<Rectangle>> input_rects_px;

    std::unique_ptr<XScopedEventSelector> events;
    std::unique_ptr<ScopedShapeEventSelector> shape_events;
  };

  static WindowCache* instance() { return instance_; }

  WindowCache(Connection* connection, Window root);
  WindowCache(const WindowCache&) = delete;
  WindowCache& operator=(const WindowCache&) = delete;
  ~WindowCache() override;

  void SyncForTest();

  const std::unordered_map<Window, WindowInfo>& windows() const {
    return windows_;
  }

 private:
  // EventObserver:
  void OnEvent(const Event& event) override;

  // Start caching the window tree starting at `window`.  `parent` is set as
  // the initial parent in the cache state.
  void AddWindow(Window window, Window parent);

  // Returns the WindowInfo for `window` or nullptr if `window` is not cached.
  WindowInfo* GetInfo(Window window);

  // Returns a vector of child windows or nullptr if `window` is not cached.
  std::vector<Window>* GetChildren(Window window);

  // Common response handler that's called at the beginning of each On*Response.
  // Returns the WindowInfo for `window` or nullptr if `window` is not cached
  // or `has_reply` is false.
  WindowInfo* OnResponse(Window window, bool has_reply);

  void OnGetWindowAttributesResponse(Window window,
                                     GetWindowAttributesResponse response);

  void OnGetGeometryResponse(Window window, GetGeometryResponse response);

  void OnQueryTreeResponse(Window window, QueryTreeResponse response);

  void OnGetRectanglesResponse(Window window,
                               Shape::Sk kind,
                               Shape::GetRectanglesResponse response);

  static WindowCache* instance_;

  Connection* const connection_;
  const Window root_;
  std::unique_ptr<XScopedEventSelector> root_events_;

  std::unordered_map<Window, WindowInfo> windows_;

  unsigned int pending_requests_ = 0;

  // Although only one instance of WindowCache may be created at a time, the
  // instance will be created and destroyed as needed, so WeakPtrs are still
  // necessary.
  base::WeakPtrFactory<WindowCache> weak_factory_{this};
};

}  // namespace x11

#endif  // UI_GFX_X_WINDOW_CACHE_H_
