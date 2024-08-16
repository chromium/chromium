// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "ui/display/types/display_color_management.h"
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

bool IsModeset(uint32_t flags) {
  return flags & DRM_MODE_ATOMIC_ALLOW_MODESET;
}

bool IsBlocking(uint32_t flags) {
  return !(flags & DRM_MODE_ATOMIC_NONBLOCK);
}

bool IsTestOnly(uint32_t flags) {
  return flags & DRM_MODE_ATOMIC_TEST_ONLY;
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
                       base::ScopedFD fd,
                       bool is_primary_device)
    : device_path_(device_path),
      drm_fd_(std::move(fd)),
      is_primary_device_(is_primary_device) {}

DrmWrapper::~DrmWrapper() = default;

bool DrmWrapper::Initialize() {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(drm_fd_.get())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

  // Set atomic capabilities. Note: we cache the outcome since there is no way
  // to retrieve this outcome later (i.e. it's impossible, and a common mistake,
  // to try and get DRM_CLIENT_CAP_ATOMIC capability.)
  is_atomic_ = SetCapability(DRM_CLIENT_CAP_ATOMIC, 1);

  // Expose all planes (overlay, primary, and cursor) to userspace.
  SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  return true;
}

/*******
 * CRTCs
 *******/

ScopedDrmCrtcPtr DrmWrapper::GetCrtc(uint32_t crtc_id) const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(drm_fd_.get(), crtc_id));
}

bool DrmWrapper::SetCrtc(uint32_t crtc_id,
                         uint32_t framebuffer,
                         std::vector<uint32_t> connectors,
                         const drmModeModeInfo& mode) {
  DCHECK(drm_fd_.is_valid());
  DCHECK(!connectors.empty());

  TRACE_EVENT2("drm", "DrmWrapper::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode.hdisplay, mode.vdisplay).ToString());

  return !drmModeSetCrtc(drm_fd_.get(), crtc_id, framebuffer, 0, 0,
                         connectors.data(), connectors.size(),
                         const_cast<drmModeModeInfo*>(&mode));
}

bool DrmWrapper::DisableCrtc(uint32_t crtc_id) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(drm_fd_.get(), crtc_id, 0, 0, 0, nullptr, 0, nullptr);
}

/**************
 * Capabilities
 **************/

bool DrmWrapper::GetCapability(uint64_t capability, uint64_t* value) const {
  DCHECK(drm_fd_.is_valid());
  return !drmGetCap(drm_fd_.get(), capability, value);
}

bool DrmWrapper::SetCapability(uint64_t capability, uint64_t value) {
  DCHECK(drm_fd_.is_valid());

  struct drm_set_client_cap cap = {capability, value};
  return !drmIoctl(drm_fd_.get(), DRM_IOCTL_SET_CLIENT_CAP, &cap);
}

/************
 * Connectors
 ************/

ScopedDrmConnectorPtr DrmWrapper::GetConnector(uint32_t connector_id) const {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(drm_fd_.get(), connector_id));
}

/********
 * Cursor
 ********/

bool DrmWrapper::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::MoveCursor", "crtc_id", crtc_id);
  return !drmModeMoveCursor(drm_fd_.get(), crtc_id, point.x(), point.y());
}

bool DrmWrapper::SetCursor(uint32_t crtc_id,
                           uint32_t handle,
                           const gfx::Size& size) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT2("drm", "DrmWrapper::SetCursor", "crtc_id", crtc_id, "handle",
               handle);
  return !drmModeSetCursor(drm_fd_.get(), crtc_id, handle, size.width(),
                           size.height());
}

/************
 * DRM Master
 ************/

bool DrmWrapper::SetMaster() {
  TRACE_EVENT1("drm", "DrmWrapper::SetMaster", "path", device_path_.value());
  DCHECK(drm_fd_.is_valid());
  return (drmSetMaster(drm_fd_.get()) == 0);
}

bool DrmWrapper::DropMaster() {
  TRACE_EVENT1("drm", "DrmWrapper::DropMaster", "path", device_path_.value());
  DCHECK(drm_fd_.is_valid());
  return (drmDropMaster(drm_fd_.get()) == 0);
}

/**************
 * Dumb Buffers
 **************/

bool DrmWrapper::CreateDumbBuffer(const SkImageInfo& info,
                                  uint32_t* handle,
                                  uint32_t* stride) {
  DCHECK(drm_fd_.is_valid());

  TRACE_EVENT0("drm", "DrmWrapper::CreateDumbBuffer");
  return DrmCreateDumbBuffer(drm_fd_.get(), info, handle, stride);
}

bool DrmWrapper::DestroyDumbBuffer(uint32_t handle) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::DestroyDumbBuffer", "handle", handle);
  return DrmDestroyDumbBuffer(drm_fd_.get(), handle);
}

bool DrmWrapper::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(drm_fd_.get(), DRM_IOCTL_MODE_MAP_DUMB, &map_request)) {
    PLOG(ERROR) << "Cannot prepare dumb buffer for mapping";
    return false;
  }

  *pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 drm_fd_.get(), map_request.offset);
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
  return !drmIoctl(drm_fd_.get(), DRM_IOCTL_GEM_CLOSE, &close_request);
}

/**********
 * Encoders
 **********/

ScopedDrmEncoderPtr DrmWrapper::GetEncoder(uint32_t encoder_id) const {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::GetEncoder", "encoder", encoder_id);
  return ScopedDrmEncoderPtr(drmModeGetEncoder(drm_fd_.get(), encoder_id));
}

/**************
 * Framebuffers
 **************/

ScopedDrmFramebufferPtr DrmWrapper::GetFramebuffer(uint32_t framebuffer) const {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(drmModeGetFB(drm_fd_.get(), framebuffer));
}

bool DrmWrapper::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(drm_fd_.get(), framebuffer);
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
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT1("drm", "DrmWrapper::AddFramebuffer", "handle", handles[0]);
  return !drmModeAddFB2WithModifiers(drm_fd_.get(), width, height, format,
                                     handles, strides, offsets, modifiers,
                                     framebuffer, flags);
}

/*******
 * Gamma
 *******/

bool DrmWrapper::SetGammaRamp(uint32_t crtc_id,
                              const display::GammaCurve& curve) {
  ScopedDrmCrtcPtr crtc = GetCrtc(crtc_id);
  size_t gamma_size = static_cast<size_t>(crtc->gamma_size);

  if (gamma_size == 0 && curve.IsDefaultIdentity()) {
    return true;
  }

  if (gamma_size == 0) {
    LOG(ERROR) << "Gamma table not supported";
    return false;
  }

  std::vector<uint16_t> r, g, b;
  r.resize(gamma_size);
  g.resize(gamma_size);
  b.resize(gamma_size);
  for (size_t i = 0; i < gamma_size; ++i) {
    curve.Evaluate(i / (gamma_size - 1.f), r[i], g[i], b[i]);
  }

  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT0("drm", "DrmWrapper::SetGamma");
  return (drmModeCrtcSetGamma(drm_fd_.get(), crtc_id, r.size(), &r[0], &g[0],
                              &b[0]) == 0);
}

/********
 * Planes
 ********/

ScopedDrmPlaneResPtr DrmWrapper::GetPlaneResources() const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmPlaneResPtr(drmModeGetPlaneResources(drm_fd_.get()));
}

ScopedDrmPlanePtr DrmWrapper::GetPlane(uint32_t plane_id) const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmPlanePtr(drmModeGetPlane(drm_fd_.get(), plane_id));
}

/************
 * Properties
 ************/

ScopedDrmObjectPropertyPtr DrmWrapper::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmObjectPropertyPtr(
      drmModeObjectGetProperties(drm_fd_.get(), object_id, object_type));
}

bool DrmWrapper::SetObjectProperty(uint32_t object_id,
                                   uint32_t object_type,
                                   uint32_t property_id,
                                   uint32_t property_value) {
  DCHECK(drm_fd_.is_valid());
  return !drmModeObjectSetProperty(drm_fd_.get(), object_id, object_type,
                                   property_id, property_value);
}

ScopedDrmPropertyPtr DrmWrapper::GetProperty(drmModeConnector* connector,
                                             const char* name) const {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT2("drm", "DrmWrapper::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(drm_fd_.get(), connector->props[i]));
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
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmPropertyPtr(drmModeGetProperty(drm_fd_.get(), id));
}

bool DrmWrapper::SetProperty(uint32_t connector_id,
                             uint32_t property_id,
                             uint64_t value) {
  DCHECK(drm_fd_.is_valid());
  return !drmModeConnectorSetProperty(drm_fd_.get(), connector_id, property_id,
                                      value);
}

/****************
 * Property Blobs
 ****************/

ScopedDrmPropertyBlob DrmWrapper::CreatePropertyBlob(const void* blob,
                                                     size_t size) {
  DCHECK(drm_fd_.is_valid());
  uint32_t id = 0;
  int ret = drmModeCreatePropertyBlob(drm_fd_.get(), blob, size, &id);
  DCHECK(!ret && id);

  return std::make_unique<DrmPropertyBlobMetadata>(this, id);
}

ScopedDrmPropertyBlob DrmWrapper::CreatePropertyBlobWithFlags(const void* blob,
                                                              size_t size,
                                                              uint32_t flags) {
// TODO(markyacoub): the flag requires being merged to libdrm then backported to
// CrOS. Remove the #if once that happens.
#if defined(DRM_MODE_CREATE_BLOB_WRITE_ONLY)
  DCHECK(drm_fd_.is_valid());
  uint32_t id = 0;
  int ret = -1;

  ret =
      drmModeCreatePropertyBlobWithFlags(drm_fd_.get(), blob, size, &id, flags);
  DCHECK(!ret && id);
  return std::make_unique<DrmPropertyBlobMetadata>(this, id);
#else
  return nullptr;
#endif
}

void DrmWrapper::DestroyPropertyBlob(uint32_t id) {
  DCHECK(drm_fd_.is_valid());
  drmModeDestroyPropertyBlob(drm_fd_.get(), id);
}

ScopedDrmPropertyBlobPtr DrmWrapper::GetPropertyBlob(
    uint32_t property_id) const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmPropertyBlobPtr(
      drmModeGetPropertyBlob(drm_fd_.get(), property_id));
}

ScopedDrmPropertyBlobPtr DrmWrapper::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) const {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT2("drm", "DrmWrapper::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(drm_fd_.get(), connector->props[i]));
    if (!property) {
      continue;
    }

    if (strcmp(property->name, name) == 0 &&
        (property->flags & DRM_MODE_PROP_BLOB)) {
      return ScopedDrmPropertyBlobPtr(
          drmModeGetPropertyBlob(drm_fd_.get(), connector->prop_values[i]));
    }
  }

  return ScopedDrmPropertyBlobPtr();
}

/***********
 * Resources
 ***********/

ScopedDrmResourcesPtr DrmWrapper::GetResources() const {
  DCHECK(drm_fd_.is_valid());
  return ScopedDrmResourcesPtr(drmModeGetResources(drm_fd_.get()));
}

/*********
 * Utility
 *********/

void DrmWrapper::WriteIntoTrace(perfetto::TracedDictionary dict) const {
  dict.Add("device_path", device_path_.value());
}

std::optional<std::string> DrmWrapper::GetDriverName() const {
  DCHECK(drm_fd_.is_valid());
  ScopedDrmVersionPtr version(drmGetVersion(drm_fd_.get()));
  if (!version) {
    LOG(ERROR) << "Failed to query DRM version";
    return std::nullopt;
  }

  return std::string(version->name, version->name_len);
}

display::DrmFormatsAndModifiers DrmWrapper::GetFormatsAndModifiersForCrtc(
    uint32_t crtc_id) const {
  return display::DrmFormatsAndModifiers();
}

base::ScopedFD DrmWrapper::ToScopedFD(std::unique_ptr<DrmWrapper> drm) {
  return std::move(drm->drm_fd_);
}

// Protected

bool DrmWrapper::CommitProperties(drmModeAtomicReq* properties,
                                  uint32_t flags,
                                  uint64_t page_flip_id) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT("drm", "DrmWrapper::CommitProperties", "test", IsTestOnly(flags),
              "modeset", IsModeset(flags), "blocking", IsBlocking(flags),
              "flags", flags, "page_flip_id", page_flip_id);
  int result = drmModeAtomicCommit(drm_fd_.get(), properties, flags,
                                   reinterpret_cast<void*>(page_flip_id));

  // TODO(gildekel): Revisit b/174844386 and see if this case is still relevant,
  // given significant work has been done around failing pageflips.
  if (result && errno == EBUSY && (flags & DRM_MODE_ATOMIC_NONBLOCK)) {
    VLOG(1) << "Nonblocking atomic commit failed with EBUSY, retry without "
               "nonblock";
    // There have been cases where we get back EBUSY when attempting a
    // non-blocking atomic commit. If we return false from here, that will cause
    // the GPU process to CHECK itself. These are likely due to kernel bugs,
    // which should be fixed, but rather than crashing we should retry the
    // commit without the non-blocking flag and then it should work. This will
    // cause a slight delay, but that should be imperceptible and better than
    // crashing. We still do want the underlying driver bugs fixed, but this
    // provide a better user experience.
    flags &= ~DRM_MODE_ATOMIC_NONBLOCK;
    TRACE_EVENT("drm", "DrmWrapper::CommitProperties(retry)", "test",
                IsTestOnly(flags), "modeset", IsModeset(flags), "blocking",
                IsBlocking(flags), "flags", flags, "page_flip_id",
                page_flip_id);
    result = drmModeAtomicCommit(drm_fd_.get(), properties, flags,
                                 reinterpret_cast<void*>(page_flip_id));
  }

  return !result;
}

bool DrmWrapper::PageFlip(uint32_t crtc_id,
                          uint32_t framebuffer,
                          uint64_t page_flip_id) {
  DCHECK(drm_fd_.is_valid());
  TRACE_EVENT2("drm", "DrmWrapper::PageFlip", "crtc", crtc_id, "framebuffer",
               framebuffer);

  return !drmModePageFlip(drm_fd_.get(), crtc_id, framebuffer,
                          DRM_MODE_PAGE_FLIP_EVENT,
                          reinterpret_cast<void*>(page_flip_id));
}

}  // namespace ui
