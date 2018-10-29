// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/ws/public/mojom/screen_provider_observer.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/aura_export.h"

namespace aura {

class PropertyConverter;
class Window;
class WindowTreeClient;
class WindowTreeHostMus;

// Interface implemented by an application using mus.
class AURA_EXPORT WindowTreeClientDelegate {
 public:
  // Called when the application implementing this interface is embedded.
  // NOTE: this is only invoked if the WindowTreeClient is created with an
  // InterfaceRequest.
  virtual void OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) = 0;

  // Sent when another app is embedded in |root| (one of the roots of the
  // connection). Afer this call |root| is deleted. If the associated
  // WindowTreeClient was created from a WindowTreeClientRequest then
  // OnEmbedRootDestroyed() is called after the window is deleted.
  virtual void OnUnembed(Window* root);

  // Called when the embed root has been destroyed on the server side (or
  // another client was embedded in the same window). This is called in two
  // distinct cases:
  // . If the WindowTreeClient was created from an InterfaceRequest. In this
  //   case the WindowTreeClient associated with the WindowTreeHost is no
  //   longer in a usable state and should be deleted (after deleting
  //   |window_tree_host|).
  // . When the window associated with a top level window (one created by way of
  //   directly creating a WindowTreeHostMus) is destroyed on the server side.
  //   In this case the |window_tree_host| should be destroyed, but the
  //   WindowTreeClient is still usable.
  virtual void OnEmbedRootDestroyed(WindowTreeHostMus* window_tree_host) = 0;

  // Called when the connection to the window server has been lost. After this
  // call the windows are still valid, and you can still do things, but they
  // have no real effect. Generally when this is called clients should delete
  // the corresponding WindowTreeClient.
  virtual void OnLostConnection(WindowTreeClient* client) = 0;

  virtual PropertyConverter* GetPropertyConverter() = 0;

  // See ws::mojom::ScreenProviderObserver for details on this.
  // TODO(sky): consider moving ScreenMus from views to aura.
  virtual void OnDisplaysChanged(
      std::vector<ws::mojom::WsDisplayPtr> ws_displays,
      int64_t primary_display_id,
      int64_t internal_display_id,
      int64_t display_id_for_new_windows) {}

 protected:
  virtual ~WindowTreeClientDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_
