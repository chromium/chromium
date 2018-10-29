// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_MUS_H_
#define UI_AURA_MUS_WINDOW_MUS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "services/ws/public/mojom/cursor/cursor.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/mus/mus_types.h"

namespace gfx {
class Rect;
class Transform;
}

namespace ws {
namespace mojom {
enum class OrderDirection;
}
}

namespace viz {
class FrameSinkId;
class LocalSurfaceId;
}

namespace aura {

class Window;
class WindowTreeClient;

// See PrepareForServerBoundsChange() for details on this.
struct AURA_EXPORT WindowMusChangeData {
  virtual ~WindowMusChangeData() {}
};

// WindowMus defines the interface used by WindowTreeClient to modify
// the underlying Window. It's defined as a separate interface to make it clear
// that any changes that WindowTreeClient makes must be propagated through
// this interface so that they don't result in bouncing back to
// WindowTreeClient. For example, if the server change the bounds care must be
// taken that when the change is applied to the Window the server isn't asked to
// change the bounds too. See WindowPortMus for details.
class AURA_EXPORT WindowMus {
 public:
  enum class ChangeSource {
    // The change was made locally.
    LOCAL,
    // The change originated from the server.
    SERVER,
  };

  explicit WindowMus(WindowMusType window_mus_type)
      : window_mus_type_(window_mus_type) {}
  virtual ~WindowMus() {}

  // Returns the WindowMus associated with |window|.
  static const WindowMus* Get(const Window* window) {
    return const_cast<const WindowMus*>(Get(const_cast<Window*>(window)));
  }
  static WindowMus* Get(Window* window);

  ws::Id server_id() const { return server_id_; }

  WindowMusType window_mus_type() const { return window_mus_type_; }

  virtual Window* GetWindow() = 0;

  // These functions are called in response to a change from the server. The
  // expectation is that in calling these WindowTreeClient is not called
  // back. For example, SetBoundsFromServer() should not result in calling back
  // to WindowTreeClient::OnWindowMusBoundsChanged().
  virtual void AddChildFromServer(WindowMus* window) = 0;
  virtual void RemoveChildFromServer(WindowMus* child) = 0;
  virtual void ReorderFromServer(WindowMus* child,
                                 WindowMus* relative,
                                 ws::mojom::OrderDirection) = 0;
  virtual void SetBoundsFromServer(
      const gfx::Rect& bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) = 0;
  virtual void SetTransformFromServer(const gfx::Transform& transform) = 0;
  virtual void SetVisibleFromServer(bool visible) = 0;
  virtual void SetOpacityFromServer(float opacity) = 0;
  virtual void SetCursorFromServer(const ui::CursorData& cursor) = 0;
  virtual void SetPropertyFromServer(const std::string& property_name,
                                     const std::vector<uint8_t>* data) = 0;
  virtual void SetFrameSinkIdFromServer(
      const viz::FrameSinkId& frame_sink_id) = 0;
  virtual const viz::LocalSurfaceId& GetOrAllocateLocalSurfaceId(
      const gfx::Size& new_size) = 0;
  // The window was deleted on the server side. DestroyFromServer() should
  // result in deleting |this|.
  virtual void DestroyFromServer() = 0;
  virtual void AddTransientChildFromServer(WindowMus* child) = 0;
  virtual void RemoveTransientChildFromServer(WindowMus* child) = 0;
  // Called when a window was added/removed as a transient child.
  virtual ChangeSource OnTransientChildAdded(WindowMus* child) = 0;
  virtual ChangeSource OnTransientChildRemoved(WindowMus* child) = 0;

  // Returns the currently used viz::LocalSurfaceId to embed this Window. Local
  // windows or windows that have not been embedded yet will have an invalid
  // viz::LocalSurfaceId.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() = 0;

  // Called in the rare case when WindowTreeClient needs to change state and
  // can't go through one of the SetFooFromServer() functions above. Generally
  // because it needs to call another function that as a side effect changes the
  // window. Once the call to the underlying window has completed the returned
  // object should be destroyed.
  virtual std::unique_ptr<WindowMusChangeData> PrepareForServerBoundsChange(
      const gfx::Rect& bounds) = 0;
  virtual std::unique_ptr<WindowMusChangeData> PrepareForServerVisibilityChange(
      bool value) = 0;

  // Called when the window on the server has been destroyed and the client code
  // is going to delete the window. The client may not immediately delete the
  // window (as compared to DestroyFromServer()).
  virtual void PrepareForDestroy() = 0;

  virtual void NotifyEmbeddedAppDisconnected() = 0;

  virtual bool HasLocalLayerTreeFrameSink() = 0;

  virtual float GetDeviceScaleFactor() = 0;

 private:
  // Just for set_server_id(), which other places should not call.
  friend class WindowTreeClient;

  void set_server_id(ws::Id id) { server_id_ = id; }

  ws::Id server_id_ = kInvalidServerId;
  const WindowMusType window_mus_type_;
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_MUS_H_
