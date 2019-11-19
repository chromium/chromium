// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DRM_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DRM_H_

#include <wayland-drm-client-protocol.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

struct wl_drm;

namespace gfx {
enum class BufferFormat;
class Size;
}  // namespace gfx

namespace ui {

class WaylandConnection;

// Wrapper around |wl_drm| Wayland factory, which creates
// |wl_buffer|s backed by dmabuf prime file descriptors.
class WaylandDrm {
 public:
  WaylandDrm(wl_drm* drm, WaylandConnection* connection);
  ~WaylandDrm();

  // Says if can create dmabuf based wl_buffers.
  bool SupportsDrmPrime() const;

  // Requests to create a wl_buffer backed by the dmabuf prime |fd| descriptor.
  // The result is sent back via the |callback|. If buffer creation failed,
  // nullptr is sent back via the callback. Otherwise, a pointer to the
  // |wl_buffer| is sent.
  void CreateBuffer(base::ScopedFD fd,
                    const gfx::Size& size,
                    const std::vector<uint32_t>& strides,
                    const std::vector<uint32_t>& offsets,
                    const std::vector<uint64_t>& modifiers,
                    uint32_t format,
                    uint32_t planes_count,
                    wl::OnRequestBufferCallback callback);

  // Returns supported buffer formats received from the Wayland compositor.
  wl::BufferFormatsWithModifiersMap supported_buffer_formats() const {
    return supported_buffer_formats_;
  }

 private:
  // Resets the |wl_drm| and prints the error.
  void HandleDrmFailure(const std::string& error);

  // Receives supported |fourcc_format| from either ::Format call.
  void AddSupportedFourCCFormat(uint32_t fourcc_format);

  // Authenticates the drm device passed in the |drm_device_path|.
  void Authenticate(const char* drm_device_path);

  // Completes the drm device authentication.
  void DrmDeviceAuthenticated(struct wl_drm* wl_drm);

  // Checks the capabilities of the drm device.
  void HandleCapabilities(uint32_t value);

  // wl_drm_listener:
  static void Device(void* data,
                     struct wl_drm* wl_drm,
                     const char* drm_device_path);
  static void Format(void* data, struct wl_drm* wl_drm, uint32_t format);
  static void Authenticated(void* data, struct wl_drm* wl_drm);
  static void Capabilities(void* data, struct wl_drm* wl_drm, uint32_t value);

  // Holds pointer to the wl_drm Wayland factory.
  wl::Object<wl_drm> wl_drm_;

  // Non-owned.
  WaylandConnection* const connection_;

  // Holds supported DRM formats translated to gfx::BufferFormat. Note that
  // |wl_drm| neither announces modifiers nor allows to create buffers with
  // modifiers. Thus, they are always empty.
  wl::BufferFormatsWithModifiersMap supported_buffer_formats_;

  // Says if the drm device passed by the Wayland compositor authenticates this
  // client.
  bool authenticated_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandDrm);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DRM_H_
