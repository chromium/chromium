// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>

#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kImmedVerstion = 3;
}

WaylandZwpLinuxDmabuf::WaylandZwpLinuxDmabuf(
    zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    WaylandConnection* connection)
    : zwp_linux_dmabuf_(zwp_linux_dmabuf), connection_(connection) {
  static const zwp_linux_dmabuf_v1_listener dmabuf_listener = {
      &WaylandZwpLinuxDmabuf::Format,
      &WaylandZwpLinuxDmabuf::Modifiers,
  };
  zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_.get(), &dmabuf_listener,
                                   this);

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  wl_display_roundtrip(connection_->display());
}

WaylandZwpLinuxDmabuf::~WaylandZwpLinuxDmabuf() = default;

void WaylandZwpLinuxDmabuf::CreateBuffer(base::ScopedFD fd,
                                         const gfx::Size& size,
                                         const std::vector<uint32_t>& strides,
                                         const std::vector<uint32_t>& offsets,
                                         const std::vector<uint64_t>& modifiers,
                                         uint32_t format,
                                         uint32_t planes_count,
                                         wl::OnRequestBufferCallback callback) {
  static const struct zwp_linux_buffer_params_v1_listener params_listener = {
      &WaylandZwpLinuxDmabuf::CreateSucceeded,
      &WaylandZwpLinuxDmabuf::CreateFailed};

  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf_.get());

  for (size_t i = 0; i < planes_count; i++) {
    zwp_linux_buffer_params_v1_add(params, fd.get(), i /* plane id */,
                                   offsets[i], strides[i], modifiers[i] >> 32,
                                   modifiers[i] & UINT32_MAX);
  }

  // It's possible to avoid waiting until the buffer is created and have it
  // immediately. This method is only available since the protocol version 3.
  if (zwp_linux_dmabuf_v1_get_version(zwp_linux_dmabuf_.get()) >=
      kImmedVerstion) {
    wl::Object<wl_buffer> buffer(zwp_linux_buffer_params_v1_create_immed(
        params, size.width(), size.height(), format, 0));
    std::move(callback).Run(std::move(buffer));
  } else {
    // Store the |params| with the corresponding |callback| to identify newly
    // created buffer and notify the client about it via the |callback|.
    pending_params_.emplace(params, std::move(callback));

    zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);
    zwp_linux_buffer_params_v1_create(params, size.width(), size.height(),
                                      format, 0);
  }
  connection_->ScheduleFlush();
}

void WaylandZwpLinuxDmabuf::AddSupportedFourCCFormatAndModifier(
    uint32_t fourcc_format,
    base::Optional<uint64_t> modifier) {
  // Return on not supported fourcc formats.
  if (!IsValidBufferFormat(fourcc_format))
    return;

  uint64_t format_modifier = modifier.value_or(DRM_FORMAT_MOD_INVALID);

  // If the buffer format has already been stored, it must be another supported
  // modifier sent by the Wayland compositor.
  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  auto it = supported_buffer_formats_with_modifiers_.find(format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    if (format_modifier != DRM_FORMAT_MOD_INVALID)
      it->second.emplace_back(format_modifier);
    return;
  } else {
    std::vector<uint64_t> modifiers;
    if (format_modifier != DRM_FORMAT_MOD_INVALID)
      modifiers.emplace_back(format_modifier);
    supported_buffer_formats_with_modifiers_.emplace(format,
                                                     std::move(modifiers));
  }
}

void WaylandZwpLinuxDmabuf::NotifyRequestCreateBufferDone(
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  auto it = pending_params_.find(params);
  DCHECK(it != pending_params_.end());

  std::move(it->second).Run(wl::Object<struct wl_buffer>(new_buffer));

  pending_params_.erase(it);
  zwp_linux_buffer_params_v1_destroy(params);

  connection_->ScheduleFlush();
}

// static
void WaylandZwpLinuxDmabuf::Modifiers(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self) {
    uint64_t modifier = static_cast<uint64_t>(modifier_hi) << 32 | modifier_lo;
    self->AddSupportedFourCCFormatAndModifier(format, {modifier});
  }
}

// static
void WaylandZwpLinuxDmabuf::Format(void* data,
                                   struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                   uint32_t format) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->AddSupportedFourCCFormatAndModifier(format, base::nullopt);
}

// static
void WaylandZwpLinuxDmabuf::CreateSucceeded(
    void* data,
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->NotifyRequestCreateBufferDone(params, new_buffer);
}

// static
void WaylandZwpLinuxDmabuf::CreateFailed(
    void* data,
    struct zwp_linux_buffer_params_v1* params) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->NotifyRequestCreateBufferDone(params, nullptr);
}

}  // namespace ui
