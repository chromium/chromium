// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

#include <xf86drm.h>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

// Private types defined in libdrm. Define them here so we can peek at the
// commit and ensure the expected state has been set correctly.
struct drmModeAtomicReqItem {
  uint32_t object_id;
  uint32_t property_id;
  uint64_t value;
};

typedef drmModeAtomicReqItem* drmModeAtomicReqItemPtr;

struct _drmModeAtomicReq {
  uint32_t cursor;
  uint32_t size_items;
  drmModeAtomicReqItemPtr items;
};

namespace ui {

namespace {

template <class Object>
Object* DrmAllocator() {
  return static_cast<Object*>(drmMalloc(sizeof(Object)));
}

ScopedDrmObjectPropertyPtr CreatePropertyObject(
    const std::vector<DrmDevice::Property>& properties) {
  ScopedDrmObjectPropertyPtr drm_properties(
      DrmAllocator<drmModeObjectProperties>());
  drm_properties->count_props = properties.size();
  drm_properties->props = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * drm_properties->count_props));
  drm_properties->prop_values = static_cast<uint64_t*>(
      drmMalloc(sizeof(uint64_t) * drm_properties->count_props));
  for (size_t i = 0; i < properties.size(); ++i) {
    drm_properties->props[i] = properties[i].id;
    drm_properties->prop_values[i] = properties[i].value;
  }

  return drm_properties;
}

template <class Type>
Type* FindObjectById(uint32_t id, std::vector<Type>& properties) {
  auto it = std::find_if(properties.begin(), properties.end(),
                         [id](const Type& p) { return p.id == id; });
  return it != properties.end() ? &(*it) : nullptr;
}

}  // namespace

MockDrmDevice::CrtcProperties::CrtcProperties() = default;
MockDrmDevice::CrtcProperties::CrtcProperties(const CrtcProperties&) = default;
MockDrmDevice::CrtcProperties::~CrtcProperties() = default;

MockDrmDevice::PlaneProperties::PlaneProperties() = default;
MockDrmDevice::PlaneProperties::PlaneProperties(const PlaneProperties&) =
    default;
MockDrmDevice::PlaneProperties::~PlaneProperties() = default;

MockDrmDevice::MockDrmDevice(std::unique_ptr<GbmDevice> gbm_device)
    : DrmDevice(base::FilePath(),
                base::File(),
                true /* is_primary_device */,
                std::move(gbm_device)),
      get_crtc_call_count_(0),
      set_crtc_call_count_(0),
      restore_crtc_call_count_(0),
      add_framebuffer_call_count_(0),
      remove_framebuffer_call_count_(0),
      page_flip_call_count_(0),
      overlay_clear_call_count_(0),
      allocate_buffer_count_(0),
      set_crtc_expectation_(true),
      add_framebuffer_expectation_(true),
      page_flip_expectation_(true),
      create_dumb_buffer_expectation_(true),
      current_framebuffer_(0) {
  plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
}

// static
ScopedDrmPropertyBlobPtr MockDrmDevice::AllocateInFormatsBlob(
    uint32_t id,
    const std::vector<uint32_t>& supported_formats,
    const std::vector<drm_format_modifier>& supported_format_modifiers) {
  drm_format_modifier_blob header;
  header.count_formats = supported_formats.size();
  header.formats_offset = sizeof(header);
  header.count_modifiers = supported_format_modifiers.size();
  header.modifiers_offset =
      header.formats_offset + sizeof(uint32_t) * header.count_formats;

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  blob->id = id;
  blob->length = header.modifiers_offset +
                 sizeof(drm_format_modifier) * header.count_modifiers;
  blob->data = drmMalloc(blob->length);

  memcpy(blob->data, &header, sizeof(header));
  memcpy(static_cast<uint8_t*>(blob->data) + header.formats_offset,
         supported_formats.data(), sizeof(uint32_t) * header.count_formats);
  memcpy(static_cast<uint8_t*>(blob->data) + header.modifiers_offset,
         supported_format_modifiers.data(),
         sizeof(drm_format_modifier) * header.count_modifiers);

  return blob;
}

void MockDrmDevice::InitializeState(
    const std::vector<CrtcProperties>& crtc_properties,
    const std::vector<PlaneProperties>& plane_properties,
    const std::map<uint32_t, std::string>& property_names,
    bool use_atomic) {
  CHECK(InitializeStateWithResult(crtc_properties, plane_properties,
                                  property_names, use_atomic));
}

bool MockDrmDevice::InitializeStateWithResult(
    const std::vector<CrtcProperties>& crtc_properties,
    const std::vector<PlaneProperties>& plane_properties,
    const std::map<uint32_t, std::string>& property_names,
    bool use_atomic) {
  crtc_properties_ = crtc_properties;
  plane_properties_ = plane_properties;
  property_names_ = property_names;
  if (use_atomic) {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerAtomic>(this);
  } else {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
  }

  return plane_manager_->Initialize();
}

MockDrmDevice::~MockDrmDevice() {}

ScopedDrmResourcesPtr MockDrmDevice::GetResources() {
  ScopedDrmResourcesPtr resources(DrmAllocator<drmModeRes>());
  resources->count_crtcs = crtc_properties_.size();
  resources->crtcs = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_crtcs));
  for (size_t i = 0; i < crtc_properties_.size(); ++i)
    resources->crtcs[i] = crtc_properties_[i].id;

  return resources;
}

ScopedDrmPlaneResPtr MockDrmDevice::GetPlaneResources() {
  ScopedDrmPlaneResPtr resources(DrmAllocator<drmModePlaneRes>());
  resources->count_planes = plane_properties_.size();
  resources->planes = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_planes));
  for (size_t i = 0; i < plane_properties_.size(); ++i)
    resources->planes[i] = plane_properties_[i].id;

  return resources;
}

ScopedDrmObjectPropertyPtr MockDrmDevice::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) {
  if (object_type == DRM_MODE_OBJECT_PLANE) {
    PlaneProperties* properties = FindObjectById(object_id, plane_properties_);
    if (properties)
      return CreatePropertyObject(properties->properties);
  } else if (object_type == DRM_MODE_OBJECT_CRTC) {
    CrtcProperties* properties = FindObjectById(object_id, crtc_properties_);
    if (properties)
      return CreatePropertyObject(properties->properties);
  }

  return nullptr;
}

ScopedDrmCrtcPtr MockDrmDevice::GetCrtc(uint32_t crtc_id) {
  get_crtc_call_count_++;
  return ScopedDrmCrtcPtr(DrmAllocator<drmModeCrtc>());
}

bool MockDrmDevice::SetCrtc(uint32_t crtc_id,
                            uint32_t framebuffer,
                            std::vector<uint32_t> connectors,
                            drmModeModeInfo* mode) {
  crtc_fb_[crtc_id] = framebuffer;
  current_framebuffer_ = framebuffer;
  set_crtc_call_count_++;
  return set_crtc_expectation_;
}

bool MockDrmDevice::SetCrtc(drmModeCrtc* crtc,
                            std::vector<uint32_t> connectors) {
  restore_crtc_call_count_++;
  return true;
}

bool MockDrmDevice::DisableCrtc(uint32_t crtc_id) {
  current_framebuffer_ = 0;
  return true;
}

ScopedDrmConnectorPtr MockDrmDevice::GetConnector(uint32_t connector_id) {
  ScopedDrmConnectorPtr connector =
      ScopedDrmConnectorPtr(DrmAllocator<drmModeConnector>());
  connector->connector_id = connector_id;
  connector->connector_type = connector_type_;
  return connector;
}

bool MockDrmDevice::AddFramebuffer2(uint32_t width,
                                    uint32_t height,
                                    uint32_t format,
                                    uint32_t handles[4],
                                    uint32_t strides[4],
                                    uint32_t offsets[4],
                                    uint64_t modifiers[4],
                                    uint32_t* framebuffer,
                                    uint32_t flags) {
  add_framebuffer_call_count_++;
  *framebuffer = add_framebuffer_call_count_;
  framebuffer_ids_.insert(*framebuffer);
  return add_framebuffer_expectation_;
}

bool MockDrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  {
    auto it = framebuffer_ids_.find(framebuffer);
    CHECK(it != framebuffer_ids_.end());
    framebuffer_ids_.erase(it);
  }
  remove_framebuffer_call_count_++;
  std::vector<uint32_t> crtcs_to_clear;
  for (auto crtc_fb : crtc_fb_) {
    if (crtc_fb.second == framebuffer)
      crtcs_to_clear.push_back(crtc_fb.first);
  }
  for (auto crtc : crtcs_to_clear)
    crtc_fb_[crtc] = 0;
  return true;
}

ScopedDrmFramebufferPtr MockDrmDevice::GetFramebuffer(uint32_t framebuffer) {
  return ScopedDrmFramebufferPtr();
}

bool MockDrmDevice::PageFlip(uint32_t crtc_id,
                             uint32_t framebuffer,
                             scoped_refptr<PageFlipRequest> page_flip_request) {
  page_flip_call_count_++;
  DCHECK(page_flip_request);
  crtc_fb_[crtc_id] = framebuffer;
  current_framebuffer_ = framebuffer;
  if (page_flip_expectation_)
    callbacks_.push(page_flip_request->AddPageFlip());
  return page_flip_expectation_;
}

ScopedDrmPlanePtr MockDrmDevice::GetPlane(uint32_t plane_id) {
  PlaneProperties* properties = FindObjectById(plane_id, plane_properties_);
  if (!properties)
    return nullptr;

  ScopedDrmPlanePtr plane(DrmAllocator<drmModePlane>());
  plane->possible_crtcs = properties->crtc_mask;
  return plane;
}

ScopedDrmPropertyPtr MockDrmDevice::GetProperty(drmModeConnector* connector,
                                                const char* name) {
  return ScopedDrmPropertyPtr(DrmAllocator<drmModePropertyRes>());
}

ScopedDrmPropertyPtr MockDrmDevice::GetProperty(uint32_t id) {
  auto it = property_names_.find(id);
  if (it == property_names_.end())
    return nullptr;

  ScopedDrmPropertyPtr property(DrmAllocator<drmModePropertyRes>());
  property->prop_id = id;
  strcpy(property->name, it->second.c_str());
  return property;
}

bool MockDrmDevice::SetProperty(uint32_t connector_id,
                                uint32_t property_id,
                                uint64_t value) {
  return true;
}

ScopedDrmPropertyBlob MockDrmDevice::CreatePropertyBlob(void* blob,
                                                        size_t size) {
  uint32_t id = ++property_id_generator_;
  allocated_property_blobs_.insert(id);
  return ScopedDrmPropertyBlob(new DrmPropertyBlobMetadata(this, id));
}

void MockDrmDevice::DestroyPropertyBlob(uint32_t id) {
  EXPECT_TRUE(allocated_property_blobs_.erase(id));
}

bool MockDrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  return true;
}

ScopedDrmPropertyBlobPtr MockDrmDevice::GetPropertyBlob(uint32_t property_id) {
  auto it = blob_property_map_.find(property_id);
  if (it == blob_property_map_.end())
    return nullptr;

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  blob->id = property_id;
  blob->length = it->second->length;
  blob->data = drmMalloc(blob->length);
  memcpy(blob->data, it->second->data, blob->length);

  return blob;
}

ScopedDrmPropertyBlobPtr MockDrmDevice::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) {
  return ScopedDrmPropertyBlobPtr(DrmAllocator<drmModePropertyBlobRes>());
}

bool MockDrmDevice::SetObjectProperty(uint32_t object_id,
                                      uint32_t object_type,
                                      uint32_t property_id,
                                      uint32_t property_value) {
  set_object_property_count_++;
  return true;
}

bool MockDrmDevice::SetCursor(uint32_t crtc_id,
                              uint32_t handle,
                              const gfx::Size& size) {
  crtc_cursor_map_[crtc_id] = handle;
  return true;
}

bool MockDrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  return true;
}

bool MockDrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                     uint32_t* handle,
                                     uint32_t* stride) {
  if (!create_dumb_buffer_expectation_)
    return false;

  *handle = allocate_buffer_count_++;
  *stride = info.minRowBytes();
  void* pixels = new char[info.computeByteSize(*stride)];
  buffers_.push_back(SkSurface::MakeRasterDirectReleaseProc(
      info, pixels, *stride,
      [](void* pixels, void* context) { delete[] static_cast<char*>(pixels); },
      /*context=*/nullptr));
  buffers_[*handle]->getCanvas()->clear(SK_ColorBLACK);

  return true;
}

bool MockDrmDevice::DestroyDumbBuffer(uint32_t handle) {
  if (handle >= buffers_.size() || !buffers_[handle])
    return false;

  buffers_[handle].reset();
  return true;
}

bool MockDrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  if (handle >= buffers_.size() || !buffers_[handle])
    return false;

  SkPixmap pixmap;
  buffers_[handle]->peekPixels(&pixmap);
  *pixels = const_cast<void*>(pixmap.addr());
  return true;
}

bool MockDrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return true;
}

bool MockDrmDevice::CloseBufferHandle(uint32_t handle) {
  return true;
}

bool MockDrmDevice::CommitProperties(
    drmModeAtomicReq* request,
    uint32_t flags,
    uint32_t crtc_count,
    scoped_refptr<PageFlipRequest> page_flip_request) {
  commit_count_++;
  if (!commit_expectation_)
    return false;

  for (uint32_t i = 0; i < request->cursor; ++i) {
    EXPECT_TRUE(ValidatePropertyValue(request->items[i].property_id,
                                      request->items[i].value));
  }

  if (page_flip_request)
    callbacks_.push(page_flip_request->AddPageFlip());

  if (flags & DRM_MODE_ATOMIC_TEST_ONLY)
    return true;

  // Only update values if not testing.
  for (uint32_t i = 0; i < request->cursor; ++i) {
    EXPECT_TRUE(UpdateProperty(request->items[i].object_id,
                               request->items[i].property_id,
                               request->items[i].value));
  }

  return true;
}

bool MockDrmDevice::SetGammaRamp(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& lut) {
  set_gamma_ramp_count_++;
  return legacy_gamma_ramp_expectation_;
}

bool MockDrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  return true;
}

uint32_t MockDrmDevice::GetFramebufferForCrtc(uint32_t crtc_id) const {
  auto it = crtc_fb_.find(crtc_id);
  return it != crtc_fb_.end() ? it->second : 0u;
}

void MockDrmDevice::RunCallbacks() {
  while (!callbacks_.empty()) {
    PageFlipCallback callback = std::move(callbacks_.front());
    callbacks_.pop();
    std::move(callback).Run(0, base::TimeTicks());
  }
}

void MockDrmDevice::SetPropertyBlob(ScopedDrmPropertyBlobPtr blob) {
  blob_property_map_[blob->id] = std::move(blob);
}

bool MockDrmDevice::UpdateProperty(
    uint32_t id,
    uint64_t value,
    std::vector<DrmDevice::Property>* properties) {
  DrmDevice::Property* property = FindObjectById(id, *properties);
  if (!property)
    return false;

  property->value = value;
  return true;
}

bool MockDrmDevice::UpdateProperty(uint32_t object_id,
                                   uint32_t property_id,
                                   uint64_t value) {
  PlaneProperties* plane_properties =
      FindObjectById(object_id, plane_properties_);
  if (plane_properties)
    return UpdateProperty(property_id, value, &plane_properties->properties);

  CrtcProperties* crtc_properties = FindObjectById(object_id, crtc_properties_);
  if (crtc_properties)
    return UpdateProperty(property_id, value, &crtc_properties->properties);

  return false;
}

bool MockDrmDevice::ValidatePropertyValue(uint32_t id, uint64_t value) {
  auto it = property_names_.find(id);
  if (it == property_names_.end())
    return false;

  if (value == 0)
    return true;

  std::vector<std::string> blob_properties = {"CTM", "DEGAMMA_LUT", "GAMMA_LUT",
                                              "PLANE_CTM"};
  if (base::Contains(blob_properties, it->second))
    return base::Contains(allocated_property_blobs_, value);

  return true;
}

}  // namespace ui
