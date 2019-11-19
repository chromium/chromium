// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_H_

#include <drm_fourcc.h>
#include <stdint.h>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class DrmDevice;
class GbmBuffer;

// Abstraction for a DRM buffer that can be scanned-out of.
class DrmFramebuffer : public base::RefCountedThreadSafe<DrmFramebuffer> {
 public:
  struct AddFramebufferParams {
    AddFramebufferParams();
    AddFramebufferParams(const AddFramebufferParams& other);
    ~AddFramebufferParams();

    uint32_t flags = 0;
    uint32_t format = DRM_FORMAT_XRGB8888;
    uint64_t modifier = DRM_FORMAT_MOD_INVALID;
    std::vector<uint64_t> preferred_modifiers;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t num_planes = 0;
    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
  };

  static scoped_refptr<DrmFramebuffer> AddFramebuffer(
      scoped_refptr<DrmDevice> drm_device,
      AddFramebufferParams params);

  static scoped_refptr<DrmFramebuffer> AddFramebuffer(
      scoped_refptr<DrmDevice> drm_device,
      const GbmBuffer* buffer,
      std::vector<uint64_t> preferred_modifiers = std::vector<uint64_t>());

  DrmFramebuffer(scoped_refptr<DrmDevice> drm_device,
                 uint32_t framebuffer_id,
                 uint32_t framebuffer_pixel_format,
                 uint32_t opaque_framebuffer_id,
                 uint32_t opaque_framebuffer_pixel_format,
                 uint64_t format_modifier,
                 std::vector<uint64_t> preferred_modifiers,
                 const gfx::Size& size);

  // ID allocated by the KMS API when the buffer is registered (via the handle).
  uint32_t framebuffer_id() const { return framebuffer_id_; }

  // ID allocated if the buffer is also registered with a different pixel format
  // so that it can be scheduled as an opaque buffer.
  uint32_t opaque_framebuffer_id() const {
    return opaque_framebuffer_id_ ? opaque_framebuffer_id_ : framebuffer_id_;
  }

  // Returns FourCC format representing the way pixel data has been encoded in
  // memory for the registered framebuffer. This can be used to check if frame
  // buffer is compatible with a given hardware plane.
  uint32_t framebuffer_pixel_format() const {
    return framebuffer_pixel_format_;
  }

  // Returns FourCC format that should be used to schedule this buffer for
  // scanout when used as an opaque buffer.
  uint32_t opaque_framebuffer_pixel_format() const {
    return opaque_framebuffer_pixel_format_;
  }

  // Returns format modifier for buffer.
  uint64_t format_modifier() const { return format_modifier_; }

  const std::vector<uint64_t>& preferred_modifiers() const {
    return preferred_modifiers_;
  }

  // Size of the buffer.
  gfx::Size size() const { return size_; }

  // Device on which the buffer was created.
  const scoped_refptr<DrmDevice>& drm_device() const { return drm_device_; }

 private:
  ~DrmFramebuffer();

  const scoped_refptr<DrmDevice> drm_device_;

  const uint32_t framebuffer_id_;
  const uint32_t framebuffer_pixel_format_;
  // If |opaque_framebuffer_pixel_format_| differs from
  // |framebuffer_pixel_format_| the following member is set to a valid fb,
  // otherwise it is set to 0.
  const uint32_t opaque_framebuffer_id_;
  const uint32_t opaque_framebuffer_pixel_format_;
  const uint64_t format_modifier_;
  // List of modifiers passed at the creation of a bo with modifiers. If the bo
  // was created without modifiers, the vector is empty.
  const std::vector<uint64_t> preferred_modifiers_;
  const gfx::Size size_;

  friend class base::RefCountedThreadSafe<DrmFramebuffer>;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_H_
