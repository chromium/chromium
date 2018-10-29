// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PORT_H_
#define UI_AURA_WINDOW_PORT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/aura/aura_export.h"
#include "ui/base/class_property.h"

namespace cc {
class LayerTreeFrameSink;
}

namespace gfx {
class Rect;
class Transform;
}

namespace aura {

class Window;
class WindowObserver;

// WindowPort defines an interface to enable Window to be used with or without
// mus. WindowPort is owned by Window and called at key points in Windows
// lifetime that enable Window to be used in both environments.
//
// If a Window is created without an explicit WindowPort then
// Env::CreateWindowPort() is used to create the WindowPort.
class AURA_EXPORT WindowPort {
 public:
  // Corresponds to the concrete implementation of this interface.
  enum class Type {
    // WindowPortLocal.
    kLocal,

    // WindowPortMus.
    kMus,

    // WindowPortForShutdown.
    kShutdown,
  };

  virtual ~WindowPort() {}

  Type type() const { return type_; }

  // Called from Window::Init().
  virtual void OnPreInit(Window* window) = 0;

  virtual void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                          float new_device_scale_factor) = 0;

  // Called when a window is being added as a child. |child| may already have
  // a parent, but its parent is not the Window this WindowPort is associated
  // with.
  virtual void OnWillAddChild(Window* child) = 0;

  virtual void OnWillRemoveChild(Window* child) = 0;

  // Called to move the child at |current_index| to |dest_index|. |dest_index|
  // is calculated assuming the window at |current_index| has been removed, e.g.
  //   Window* child = children_[current_index];
  //   children_.erase(children_.begin() + current_index);
  //   children_.insert(children_.begin() + dest_index, child);
  virtual void OnWillMoveChild(size_t current_index, size_t dest_index) = 0;

  virtual void OnVisibilityChanged(bool visible) = 0;

  virtual void OnDidChangeBounds(const gfx::Rect& old_bounds,
                                 const gfx::Rect& new_bounds) = 0;

  virtual void OnDidChangeTransform(const gfx::Transform& old_transform,
                                    const gfx::Transform& new_transform) = 0;

  // Called before a property is changed. The return value from this is supplied
  // into OnPropertyChanged() so that WindowPort may pass data between the two
  // calls.
  virtual std::unique_ptr<ui::PropertyData> OnWillChangeProperty(
      const void* key) = 0;

  // Called after a property changes, but before observers are notified. |data|
  // is the return value from OnWillChangeProperty().
  virtual void OnPropertyChanged(const void* key,
                                 int64_t old_value,
                                 std::unique_ptr<ui::PropertyData> data) = 0;

  // Called for creating a cc::LayerTreeFrameSink for the window.
  virtual std::unique_ptr<cc::LayerTreeFrameSink>
  CreateLayerTreeFrameSink() = 0;

  // Forces the window to allocate a new viz::LocalSurfaceId for the next
  // CompositorFrame submission in anticipation of a synchronization operation
  // that does not involve a resize or a device scale factor change.
  virtual void AllocateLocalSurfaceId() = 0;

  // When a ScopedSurfaceIdAllocator is alive, it prevents the
  // allocator from actually allocating. Instead, it triggers its
  // |allocation_task| upon destruction. This allows us to issue only one
  // allocation during the lifetime. This is used to continue routing and
  // processing when a child allocates its own LocalSurfaceId.
  virtual viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task) = 0;

  virtual void UpdateLocalSurfaceIdFromEmbeddedClient(
      const viz::LocalSurfaceId& embedded_client_local_surface_id,
      base::TimeTicks embedded_client_local_surface_id_allocation_time) = 0;

  // Gets the current viz::LocalSurfaceId. The viz::LocalSurfaceId is allocated
  // lazily on call, and will be updated on changes to size or device scale
  // factor.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() = 0;

  // Gets the time at which the current viz::LocalSurfaceId was allocated.
  virtual base::TimeTicks GetLocalSurfaceIdAllocationTime() const = 0;

  virtual void OnEventTargetingPolicyChanged() = 0;

  // See description of function with same name in transient_window_client.
  virtual bool ShouldRestackTransientChildren() = 0;

  // Called to register/unregister an embedded FramesSinkId. This is only called
  // if SetEmbedFrameSinkId() is called on the associated Window.
  virtual void RegisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) {}
  virtual void UnregisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) {}

  // Called to start occlusion state tracking.
  virtual void TrackOcclusionState() {}

 protected:
  explicit WindowPort(Type type);

  // Returns the WindowPort associated with a Window.
  static WindowPort* Get(Window* window);

  // Returns the ObserverList of a Window.
  static base::ObserverList<WindowObserver, true>::Unchecked* GetObservers(
      Window* window);

 private:
  const Type type_;
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_PORT_H_
