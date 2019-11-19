// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_LINUX_DMABUF_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_LINUX_DMABUF_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace gfx {
enum class BufferFormat;
class Size;
}  // namespace gfx

namespace ui {

class WaylandConnection;

// Wrapper around |zwp_linux_dmabuf_v1| Wayland factory, which creates
// |wl_buffer|s backed by dmabuf prime file descriptor.
class WaylandZwpLinuxDmabuf {
 public:
  WaylandZwpLinuxDmabuf(zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                        WaylandConnection* connection);
  ~WaylandZwpLinuxDmabuf();

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
    return supported_buffer_formats_with_modifiers_;
  }

 private:
  // Receives supported |fourcc_format| from either ::Modifers or ::Format call
  // (depending on the protocol version), and stores it as gfx::BufferFormat to
  // the |supported_buffer_formats_| container. Modifiers can also be passed to
  // this method to be stored as a map of the format and modifier.
  void AddSupportedFourCCFormatAndModifier(uint32_t fourcc_format,
                                           base::Optional<uint64_t> modifier);

  // Finds the stored callback corresponding to the |params| created in the
  // RequestBufferAsync call, and passes the wl_buffer to the client. The
  // |new_buffer| can be null.
  void NotifyRequestCreateBufferDone(struct zwp_linux_buffer_params_v1* params,
                                     struct wl_buffer* new_buffer);

  // zwp_linux_dmabuf_v1_listener
  static void Modifiers(void* data,
                        struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                        uint32_t format,
                        uint32_t modifier_hi,
                        uint32_t modifier_lo);
  static void Format(void* data,
                     struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                     uint32_t format);

  // zwp_linux_buffer_params_v1_listener
  static void CreateSucceeded(void* data,
                              struct zwp_linux_buffer_params_v1* params,
                              struct wl_buffer* new_buffer);
  static void CreateFailed(void* data,
                           struct zwp_linux_buffer_params_v1* params);

  // Holds pointer to the
  // zwp_linux_dmabuf_v1 Wayland
  // factory.
  const wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf_;

  // Non-owned.
  WaylandConnection* const connection_;

  // Holds supported DRM formats translated to gfx::BufferFormat.
  wl::BufferFormatsWithModifiersMap supported_buffer_formats_with_modifiers_;

  // Contains callbacks for requests to create |wl_buffer|s using
  // |zwp_linux_dmabuf_| factory.
  base::flat_map<struct zwp_linux_buffer_params_v1*,
                 wl::OnRequestBufferCallback>
      pending_params_;

  DISALLOW_COPY_AND_ASSIGN(WaylandZwpLinuxDmabuf);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_LINUX_DMABUF_H_
