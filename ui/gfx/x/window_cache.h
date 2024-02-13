// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_WINDOW_CACHE_H_
#define UI_GFX_X_WINDOW_CACHE_H_

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

COMPONENT_EXPORT(X11)
Window GetWindowAtPoint(const gfx::Point& point_px,
                        const base::flat_set<Window>* ignore = nullptr);

class Connection;

class ScopedShapeEventSelector {
 public:
  ScopedShapeEventSelector(Connection* connection, Window window);
  ~ScopedShapeEventSelector();

 private:
  const raw_ptr<Connection> connection_;
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

    // Properties
    bool has_wm_name = false;
    gfx::Insets gtk_frame_extents_px;

    int16_t x_px = 0;
    int16_t y_px = 0;
    uint16_t width_px = 0;
    uint16_t height_px = 0;
    uint16_t border_width_px = 0;

    // Child windows listed in lowest-to-highest stacking order.
    // Although it is possible to restack windows, it is uncommon to do so,
    // so we store children in a vector instead of a node-based structure.
    std::vector<Window> children;

    std::optional<std::vector<Rectangle>> bounding_rects_px;
    std::optional<std::vector<Rectangle>> input_rects_px;

    ScopedEventSelector events;
    std::unique_ptr<ScopedShapeEventSelector> shape_events;
  };

  static WindowCache* instance() { return instance_; }

  // If `track_events` is true, the WindowCache will keep the cache state synced
  // with the server's state over time. It may be set to false if the cache is
  // short-lived, if only a single GetWindowAtPoint call is made.
  WindowCache(Connection* connection, Window root);
  WindowCache(const WindowCache&) = delete;
  WindowCache& operator=(const WindowCache&) = delete;
  ~WindowCache() override;

  // Returns the window at the specified point or Window::None if no match could
  // be found. `point_px` is in coordinates of the parent of `window`.
  Window GetWindowAtPoint(gfx::Point point_px,
                          Window window,
                          const base::flat_set<Window>* ignore = nullptr);

  // Blocks until all outstanding requests are processed.
  void WaitUntilReady();

  // Destroys |self| if no calls to GetWindowAtPoint() are made within
  // a time window.
  void BeginDestroyTimer(std::unique_ptr<WindowCache> self);

  void SyncForTest();

  const std::unordered_map<Window, WindowInfo>& windows() const {
    return windows_;
  }

 private:
  // This helper reduces boilerplate when adding requests.
  template <typename Future, typename Callback, typename... Args>
  void AddRequest(Future&& future, Callback&& callback, Args&&... args) {
    future.OnResponse(base::BindOnce(callback, weak_factory_.GetWeakPtr(),
                                     std::forward<Args>(args)...));
    pending_requests_.push_back(std::move(future));
  }

  // EventObserver:
  void OnEvent(const Event& event) override;

  // Start caching the window tree starting at `window`.  `parent` is set as
  // the initial parent in the cache state.
  void AddWindow(Window window, Window parent);

  // Returns the WindowInfo for `window` or nullptr if `window` is not cached.
  WindowInfo* GetInfo(Window window);

  // Returns a vector of child windows or nullptr if `window` is not cached.
  std::vector<Window>* GetChildren(Window window);

  // Makes a GetProperty request with a callback to OnGetPropertyResponse().
  void GetProperty(Window window, Atom property, uint32_t length);

  // Common response handler that's called at the beginning of each On*Response.
  // Returns the WindowInfo for `window` or nullptr if `window` is not cached
  // or `has_reply` is false.
  WindowInfo* OnResponse(Window window, bool has_reply);

  void OnGetWindowAttributesResponse(Window window,
                                     GetWindowAttributesResponse response);

  void OnGetGeometryResponse(Window window, GetGeometryResponse response);

  void OnQueryTreeResponse(Window window, QueryTreeResponse response);

  void OnGetPropertyResponse(Window window,
                             Atom atom,
                             GetPropertyResponse response);

  void OnGetRectanglesResponse(Window window,
                               Shape::Sk kind,
                               Shape::GetRectanglesResponse response);

  void OnDestroyTimerExpired(std::unique_ptr<WindowCache> self);

  static WindowCache* instance_;

  const raw_ptr<Connection> connection_;
  const Window root_;
  const Atom gtk_frame_extents_;
  ScopedEventSelector root_events_;

  std::unordered_map<Window, WindowInfo> windows_;

  base::circular_deque<FutureBase> pending_requests_;

  // The latest event processed out-of-order, or nullopt if the latest event was
  // processed in order.
  std::optional<uint32_t> last_processed_event_;

  // True iff GetWindowAtPoint() was called since the last timer interval.
  bool delete_when_destroy_timer_fires_ = false;

  // Although only one instance of WindowCache may be created at a time, the
  // instance will be created and destroyed as needed, so WeakPtrs are still
  // necessary.
  base::WeakPtrFactory<WindowCache> weak_factory_{this};
};

}  // namespace x11

#endif  // UI_GFX_X_WINDOW_CACHE_H_
