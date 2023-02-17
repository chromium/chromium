// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_wrapper.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"

namespace ui {

namespace {

bool DrmCreateDumbBuffer(int fd,
                         const SkImageInfo& info,
                         uint32_t* handle,
                         uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VPLOG(2) << "Cannot create dumb buffer";
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.computeByteSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

bool DrmDestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  return !drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool CanQueryForResources(int fd) {
  drm_mode_card_res resources;
  memset(&resources, 0, sizeof(resources));
  // If there is no error getting DRM resources then assume this is a
  // modesetting device.
  return !drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources);
}

}  // namespace

DrmPropertyBlobMetadata::DrmPropertyBlobMetadata(DrmWrapper* drm, uint32_t id)
    : drm_(drm), id_(id) {}

DrmPropertyBlobMetadata::~DrmPropertyBlobMetadata() {
  DCHECK(drm_);
  DCHECK(id_);
  drm_->DestroyPropertyBlob(id_);
}

DrmWrapper::DrmWrapper(const base::FilePath& device_path,
                       base::File file,
                       bool is_primary_device)
    : device_path_(device_path),
      file_(std::move(file)),
      is_primary_device_(is_primary_device) {}

DrmWrapper::~DrmWrapper() = default;

bool DrmWrapper::Initialize() {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(file_.GetPlatformFile())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

  uint64_t value;
  allow_addfb2_modifiers_ =
      GetCapability(DRM_CAP_ADDFB2_MODIFIERS, &value) && value;

  return true;
}

/*******
 * CRTCs
 *******/

ScopedDrmCrtcPtr DrmWrapper::GetCrtc(uint32_t crtc_id) const {
  DCHECK(file_.IsValid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(file_.GetPlatformFile(), crtc_id));
}

bool DrmWrapper::SetCrtc(uint32_t crtc_id,
                         uint32_t framebuffer,
                         std::vector<uint32_t> connectors,
                         const drmModeModeInfo& mode) {
  DCHECK(file_.IsValid());
  DCHECK(!connectors.empty());

  TRACE_EVENT2("drm", "DrmWrapper::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode.hdisplay, mode.vdisplay).ToString());

  if (!drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, framebuffer, 0, 0,
                      connectors.data(), connectors.size(),
                      const_cast<drmModeModeInfo*>(&mode))) {
    ++modeset_sequence_id_;
    return true;
  }

  return false;
}

bool DrmWrapper::DisableCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, 0, 0, 0, nullptr, 0,
                         nullptr);
}

/**************
 * Capabilities
 **************/

bool DrmWrapper::GetCapability(uint64_t capability, uint64_t* value) const {
  DCHECK(file_.IsValid());
  return !drmGetCap(file_.GetPlatformFile(), capability, value);
}

bool DrmWrapper::SetCapability(uint64_t capability, uint64_t value) {
  DCHECK(file_.IsValid());

  struct drm_set_client_cap cap = {capability, value};
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_SET_CLIENT_CAP, &cap);
}

/************
 * Connectors
 ************/

ScopedDrmConnectorPtr DrmWrapper::GetConnector(uint32_t connector_id) const {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(file_.GetPlatformFile(), connector_id));
}

/********
 * Cursor
 ********/

bool DrmWrapper::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::MoveCursor", "crtc_id", crtc_id);
  return !drmModeMoveCursor(file_.GetPlatformFile(), crtc_id, point.x(),
                            point.y());
}

bool DrmWrapper::SetCursor(uint32_t crtc_id,
                           uint32_t handle,
                           const gfx::Size& size) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmWrapper::SetCursor", "crtc_id", crtc_id, "handle",
               handle);
  return !drmModeSetCursor(file_.GetPlatformFile(), crtc_id, handle,
                           size.width(), size.height());
}

/************
 * DRM Master
 ************/

bool DrmWrapper::SetMaster() {
  TRACE_EVENT1("drm", "DrmWrapper::SetMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmSetMaster(file_.GetPlatformFile()) == 0);
}

bool DrmWrapper::DropMaster() {
  TRACE_EVENT1("drm", "DrmWrapper::DropMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmDropMaster(file_.GetPlatformFile()) == 0);
}

/**************
 * Dumb Buffers
 **************/

bool DrmWrapper::CreateDumbBuffer(const SkImageInfo& info,
                                  uint32_t* handle,
                                  uint32_t* stride) {
  DCHECK(file_.IsValid());

  TRACE_EVENT0("drm", "DrmWrapper::CreateDumbBuffer");
  return DrmCreateDumbBuffer(file_.GetPlatformFile(), info, handle, stride);
}

bool DrmWrapper::DestroyDumbBuffer(uint32_t handle) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::DestroyDumbBuffer", "handle", handle);
  return DrmDestroyDumbBuffer(file_.GetPlatformFile(), handle);
}

bool DrmWrapper::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_MODE_MAP_DUMB,
               &map_request)) {
    PLOG(ERROR) << "Cannot prepare dumb buffer for mapping";
    return false;
  }

  *pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 file_.GetPlatformFile(), map_request.offset);
  if (*pixels == MAP_FAILED) {
    PLOG(ERROR) << "Cannot mmap dumb buffer";
    return false;
  }

  return true;
}

bool DrmWrapper::UnmapDumbBuffer(void* pixels, size_t size) {
  return !munmap(pixels, size);
}

bool DrmWrapper::CloseBufferHandle(uint32_t handle) {
  struct drm_gem_close close_request;
  memset(&close_request, 0, sizeof(close_request));
  close_request.handle = handle;
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_GEM_CLOSE,
                   &close_request);
}

/**************
 * Framebuffers
 **************/

ScopedDrmFramebufferPtr DrmWrapper::GetFramebuffer(uint32_t framebuffer) const {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(
      drmModeGetFB(file_.GetPlatformFile(), framebuffer));
}

bool DrmWrapper::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(file_.GetPlatformFile(), framebuffer);
}

bool DrmWrapper::AddFramebuffer2(uint32_t width,
                                 uint32_t height,
                                 uint32_t format,
                                 uint32_t handles[4],
                                 uint32_t strides[4],
                                 uint32_t offsets[4],
                                 uint64_t modifiers[4],
                                 uint32_t* framebuffer,
                                 uint32_t flags) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmWrapper::AddFramebuffer", "handle", handles[0]);
  return !drmModeAddFB2WithModifiers(file_.GetPlatformFile(), width, height,
                                     format, handles, strides, offsets,
                                     modifiers, framebuffer, flags);
}

/*******
 * Gamma
 *******/

bool DrmWrapper::SetGammaRamp(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& lut) {
  ScopedDrmCrtcPtr crtc = GetCrtc(crtc_id);
  size_t gamma_size = static_cast<size_t>(crtc->gamma_size);

  if (gamma_size == 0 && lut.empty()) {
    return true;
  }

  if (gamma_size == 0) {
    LOG(ERROR) << "Gamma table not supported";
    return false;
  }

  // TODO(robert.bradford) resample the incoming ramp to match what the kernel
  // expects.
  if (!lut.empty() && gamma_size != lut.size()) {
    LOG(ERROR) << "Gamma table size mismatch: supplied " << lut.size()
               << " expected " << gamma_size;
    return false;
  }

  std::vector<uint16_t> r, g, b;
  r.reserve(gamma_size);
  g.reserve(gamma_size);
  b.reserve(gamma_size);

  if (lut.empty()) {
    // Create a linear gamma ramp table to deactivate the feature.
    for (size_t i = 0; i < gamma_size; ++i) {
      uint16_t value = (i * ((1 << 16) - 1)) / (gamma_size - 1);
      r.push_back(value);
      g.push_back(value);
      b.push_back(value);
    }
  } else {
    for (size_t i = 0; i < gamma_size; ++i) {
      r.push_back(lut[i].r);
      g.push_back(lut[i].g);
      b.push_back(lut[i].b);
    }
  }

  DCHECK(file_.IsValid());
  TRACE_EVENT0("drm", "DrmWrapper::SetGamma");
  return (drmModeCrtcSetGamma(file_.GetPlatformFile(), crtc_id, r.size(), &r[0],
                              &g[0], &b[0]) == 0);
}

/********
 * Planes
 ********/

ScopedDrmPlaneResPtr DrmWrapper::GetPlaneResources() const {
  DCHECK(file_.IsValid());
  return ScopedDrmPlaneResPtr(
      drmModeGetPlaneResources(file_.GetPlatformFile()));
}

ScopedDrmPlanePtr DrmWrapper::GetPlane(uint32_t plane_id) const {
  DCHECK(file_.IsValid());
  return ScopedDrmPlanePtr(drmModeGetPlane(file_.GetPlatformFile(), plane_id));
}

/************
 * Properties
 ************/

ScopedDrmObjectPropertyPtr DrmWrapper::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) const {
  DCHECK(file_.IsValid());
  return ScopedDrmObjectPropertyPtr(drmModeObjectGetProperties(
      file_.GetPlatformFile(), object_id, object_type));
}

bool DrmWrapper::SetObjectProperty(uint32_t object_id,
                                   uint32_t object_type,
                                   uint32_t property_id,
                                   uint32_t property_value) {
  DCHECK(file_.IsValid());
  return !drmModeObjectSetProperty(file_.GetPlatformFile(), object_id,
                                   object_type, property_id, property_value);
}

ScopedDrmPropertyPtr DrmWrapper::GetProperty(drmModeConnector* connector,
                                             const char* name) const {
  TRACE_EVENT2("drm", "DrmWrapper::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property) {
      continue;
    }

    if (strcmp(property->name, name) == 0) {
      return property;
    }
  }

  return ScopedDrmPropertyPtr();
}

ScopedDrmPropertyPtr DrmWrapper::GetProperty(uint32_t id) const {
  return ScopedDrmPropertyPtr(drmModeGetProperty(file_.GetPlatformFile(), id));
}

bool DrmWrapper::SetProperty(uint32_t connector_id,
                             uint32_t property_id,
                             uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmModeConnectorSetProperty(file_.GetPlatformFile(), connector_id,
                                      property_id, value);
}

/****************
 * Property Blobs
 ****************/

ScopedDrmPropertyBlob DrmWrapper::CreatePropertyBlob(const void* blob,
                                                     size_t size) {
  uint32_t id = 0;
  int ret = drmModeCreatePropertyBlob(file_.GetPlatformFile(), blob, size, &id);
  DCHECK(!ret && id);

  return std::make_unique<DrmPropertyBlobMetadata>(this, id);
}

void DrmWrapper::DestroyPropertyBlob(uint32_t id) {
  drmModeDestroyPropertyBlob(file_.GetPlatformFile(), id);
}

ScopedDrmPropertyBlobPtr DrmWrapper::GetPropertyBlob(
    uint32_t property_id) const {
  DCHECK(file_.IsValid());
  return ScopedDrmPropertyBlobPtr(
      drmModeGetPropertyBlob(file_.GetPlatformFile(), property_id));
}

ScopedDrmPropertyBlobPtr DrmWrapper::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) const {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmWrapper::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property) {
      continue;
    }

    if (strcmp(property->name, name) == 0 &&
        (property->flags & DRM_MODE_PROP_BLOB)) {
      return ScopedDrmPropertyBlobPtr(drmModeGetPropertyBlob(
          file_.GetPlatformFile(), connector->prop_values[i]));
    }
  }

  return ScopedDrmPropertyBlobPtr();
}

/***********
 * Resources
 ***********/

ScopedDrmResourcesPtr DrmWrapper::GetResources() const {
  DCHECK(file_.IsValid());
  return ScopedDrmResourcesPtr(drmModeGetResources(file_.GetPlatformFile()));
}

/*********
 * Utility
 *********/

void DrmWrapper::WriteIntoTrace(perfetto::TracedDictionary dict) const {
  dict.Add("device_path", device_path_.value());
}

absl::optional<std::string> DrmWrapper::GetDriverName() const {
  return GetDrmDriverNameFromFd(file_.GetPlatformFile());
}

}  // namespace ui
