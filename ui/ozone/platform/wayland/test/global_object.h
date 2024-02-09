// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_GLOBAL_OBJECT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_GLOBAL_OBJECT_H_

#include <memory>

#include "base/memory/raw_ptr.h"

struct wl_client;
struct wl_display;
struct wl_global;
struct wl_interface;
struct wl_resource;

namespace wl {

// Base class for managing the lifecycle of global objects.
// Represents a global object used to emit global events to all clients.
class GlobalObject {
 public:
  GlobalObject(const wl_interface* interface,
               const void* implementation,
               uint32_t version);

  GlobalObject(const GlobalObject&) = delete;
  GlobalObject& operator=(const GlobalObject&) = delete;

  virtual ~GlobalObject();

  // Creates a global object.
  bool Initialize(wl_display* display);

  // Can be used by clients to explicitly destroy global objects and send
  // global_remove event.
  virtual void DestroyGlobal();

  // Called from Bind() to send additional information to clients.
  virtual void OnBind() {}

  const wl_global* global() const { return global_.get(); }

  // The first resource bound to this global, which is usually all that is
  // useful when testing a simple client.
  wl_resource* resource() { return resource_; }

  // Sends the global event to clients.
  static void Bind(wl_client* client,
                   void* data,
                   uint32_t version,
                   uint32_t id);
  static void OnResourceDestroyed(wl_resource* resource);

 private:
  struct Deleter {
    void operator()(wl_global* global);
  };

  std::unique_ptr<wl_global, Deleter> global_;

  raw_ptr<const wl_interface> interface_;
  raw_ptr<const void> implementation_;
  const uint32_t version_;
  raw_ptr<wl_resource> resource_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_GLOBAL_OBJECT_H_
