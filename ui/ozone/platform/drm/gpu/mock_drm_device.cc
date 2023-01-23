// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

#include <stdint.h>
#include <xf86drm.h>

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "skia/ext/legacy_display_globals.h"
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
  uint32_t cursor;
};

typedef drmModeAtomicReqItem* drmModeAtomicReqItemPtr;

struct _drmModeAtomicReq {
  uint32_t cursor;
  uint32_t size_items;
  drmModeAtomicReqItemPtr items;
};

namespace ui {
namespace {

constexpr uint32_t kTestModesetFlags =
    DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET;

constexpr uint32_t kCommitModesetFlags = DRM_MODE_ATOMIC_ALLOW_MODESET;

// Seamless modeset is defined by the lack of DRM_MODE_ATOMIC_ALLOW_MODESET.
// This also happens to be the same set of flags as would be used for a
// pageflip, or other atomic property changes that do not require modesetting.
constexpr uint32_t kSeamlessModesetFlags = 0;

template <class Object>
Object* DrmAllocator() {
  return static_cast<Object*>(drmMalloc(sizeof(Object)));
}

const std::map<uint32_t, std::string> kCrtcRequiredPropertyNames = {
    {kActivePropId, "ACTIVE"},
    {kModePropId, "MODE_ID"},
};

const std::map<uint32_t, std::string> kCrtcOptionalPropertyNames = {
    {kBackgroundColorPropId, "BACKGROUND_COLOR"},
    {kCtmPropId, "CTM"},
    {kGammaLutPropId, "GAMMA_LUT"},
    {kGammaLutSizePropId, "GAMMA_LUT_SIZE"},
    {kDegammaLutPropId, "DEGAMMA_LUT"},
    {kDegammaLutSizePropId, "DEGAMMA_LUT_SIZE"},
    {kOutFencePtrPropId, "OUT_FENCE_PTR"},
    {kVrrEnabledPropId, "VRR_ENABLED"},
};

const std::map<uint32_t, std::string> kConnectorRequiredPropertyNames = {
    {kCrtcIdPropId, "CRTC_ID"},
    {kLinkStatusPropId, "link-status"},
};

const std::map<uint32_t, std::string> kPlaneRequiredPropertyNames = {
    // Add all required properties.
    {kPlaneCrtcId, "CRTC_ID"},
    {kCrtcX, "CRTC_X"},
    {kCrtcY, "CRTC_Y"},
    {kCrtcW, "CRTC_W"},
    {kCrtcH, "CRTC_H"},
    {kPlaneFbId, "FB_ID"},
    {kSrcX, "SRC_X"},
    {kSrcY, "SRC_Y"},
    {kSrcW, "SRC_W"},
    {kSrcH, "SRC_H"},
    {kInFencePropId, "IN_FENCE_FD"},
    {kTypePropId, "type"},
    {kInFormatsPropId, "IN_FORMATS"},
    {kRotationPropId, "rotation"},
};

const std::map<uint32_t, std::string> kPlaneOptionalPropertyNames = {
    {kPlaneCtmId, "PLANE_CTM"},
};

template <class T>
uint32_t GetNextId(const std::vector<T>& collection, uint32_t base) {
  uint32_t max = 0;
  for (const auto t : collection) {
    max = std::max(t.id, max);
  }
  return max == 0 ? base : max + 1;
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
  auto it = base::ranges::find(properties, id, &Type::id);
  return it != properties.end() ? &(*it) : nullptr;
}

// TODO(dnicoara): Generate all IDs internal to MockDrmDevice.
// For now generate something with a high enough ID to be unique in tests.
uint32_t GetUniqueNumber() {
  static uint32_t value_generator = 0xff000000;
  return ++value_generator;
}

}  // namespace

MockDrmDevice::CrtcProperties::CrtcProperties() = default;
MockDrmDevice::CrtcProperties::CrtcProperties(const CrtcProperties&) = default;
MockDrmDevice::CrtcProperties::~CrtcProperties() = default;

MockDrmDevice::ConnectorProperties::ConnectorProperties() = default;
MockDrmDevice::ConnectorProperties::ConnectorProperties(
    const ConnectorProperties&) = default;
MockDrmDevice::ConnectorProperties::~ConnectorProperties() = default;

MockDrmDevice::PlaneProperties::PlaneProperties() = default;
MockDrmDevice::PlaneProperties::PlaneProperties(const PlaneProperties&) =
    default;
MockDrmDevice::PlaneProperties::~PlaneProperties() = default;

uint32_t MockDrmDevice::PlaneProperties::type() const {
  auto prop = GetProp(kTypePropId);
  CHECK(prop);
  return prop.value()->value;
}

absl::optional<const DrmDevice::Property*>
MockDrmDevice::PlaneProperties::GetProp(uint32_t prop_id) const {
  for (const auto& prop : properties) {
    if (prop.id == prop_id)
      return {&prop};
  }
  return absl::nullopt;
}

void MockDrmDevice::PlaneProperties::SetProp(uint32_t prop_id, uint32_t value) {
  for (auto& prop : properties) {
    if (prop.id == prop_id) {
      prop.value = value;
      return;
    }
  }
  properties.push_back({prop_id, value});
}

MockDrmDevice::MockDrmState::MockDrmState() = default;
MockDrmDevice::MockDrmState::MockDrmState(const MockDrmState&) = default;
MockDrmDevice::MockDrmState::~MockDrmState() = default;

MockDrmDevice::MockDrmState
MockDrmDevice::MockDrmState::CreateStateWithNoProperties() {
  return MockDrmState();
}

MockDrmDevice::MockDrmState
MockDrmDevice::MockDrmState::CreateStateWithAllProperties() {
  MockDrmState state;
  state.property_names.insert(kCrtcRequiredPropertyNames.begin(),
                              kCrtcRequiredPropertyNames.end());
  state.property_names.insert(kConnectorRequiredPropertyNames.begin(),
                              kConnectorRequiredPropertyNames.end());
  state.property_names.insert(kPlaneRequiredPropertyNames.begin(),
                              kPlaneRequiredPropertyNames.end());

  // Separately add optional properties that will be used in some tests, but the
  // tests will append the property to the planes on a case-by-case basis.
  state.property_names.insert(kPlaneOptionalPropertyNames.begin(),
                              kPlaneOptionalPropertyNames.end());
  state.property_names.insert(kCrtcOptionalPropertyNames.begin(),
                              kCrtcOptionalPropertyNames.end());

  return state;
}

MockDrmDevice::MockDrmState
MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
    size_t crtc_count,
    size_t planes_per_crtc) {
  MockDrmState state = CreateStateWithAllProperties();
  for (size_t i = 0; i < crtc_count; ++i) {
    const auto& crtc = state.AddCrtcAndConnector().first;

    state.AddPlane(crtc.id, DRM_PLANE_TYPE_PRIMARY);
    for (size_t j = 0; j < planes_per_crtc - 1; ++j) {
      state.AddPlane(crtc.id, DRM_PLANE_TYPE_OVERLAY);
    }
    state.AddPlane(crtc.id, DRM_PLANE_TYPE_CURSOR);
  }
  return state;
}

std::pair<MockDrmDevice::CrtcProperties&, MockDrmDevice::ConnectorProperties&>
MockDrmDevice::MockDrmState::AddCrtcAndConnector() {
  uint32_t next_connector_id =
      GetNextId(connector_properties, kConnectorIdBase);
  uint32_t next_crtc_id = GetNextId(crtc_properties, kCrtcIdBase);

  auto& connector_property = connector_properties.emplace_back();
  connector_property.id = next_connector_id;
  for (const auto& pair : kConnectorRequiredPropertyNames) {
    connector_property.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(property_names, pair.first))
      property_names.emplace(pair.first, pair.second);
  }

  auto& crtc_property = crtc_properties.emplace_back();
  crtc_property.id = next_crtc_id;
  for (const auto& pair : kCrtcRequiredPropertyNames) {
    crtc_property.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(property_names, pair.first))
      property_names.emplace(pair.first, pair.second);
  }

  return {crtc_property, connector_property};
}

MockDrmDevice::PlaneProperties& MockDrmDevice::MockDrmState::AddPlane(
    uint32_t crtc_id,
    uint32_t type) {
  uint32_t next_plane_id = GetNextId(plane_properties, kPlaneOffset);

  size_t crtc_idx = UINT32_MAX;
  for (size_t i = 0; i < crtc_properties.size(); ++i) {
    if (crtc_properties[i].id == crtc_id) {
      crtc_idx = i;
      break;
    }
  }
  CHECK(crtc_idx != UINT32_MAX) << "Could not find crtc with id " << crtc_id;

  auto& plane = plane_properties.emplace_back();
  plane.id = next_plane_id;
  plane.crtc_mask = (1 << crtc_idx);
  for (const auto& pair : kPlaneRequiredPropertyNames) {
    plane.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(property_names, pair.first))
      property_names.emplace(pair.first, pair.second);
  }

  plane.SetProp(kTypePropId, type);
  plane.SetProp(kInFormatsPropId, kInFormatsBlobIdBase);

  return plane;
}

MockDrmDevice::MockDrmDevice(std::unique_ptr<GbmDevice> gbm_device)
    : DrmDevice(base::FilePath(),
                base::File(),
                true /* is_primary_device */,
                std::move(gbm_device)),
      get_crtc_call_count_(0),
      set_crtc_call_count_(0),
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

MockDrmDevice::~MockDrmDevice() = default;

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

void MockDrmDevice::InitializeState(const MockDrmState& state,
                                    bool use_atomic) {
  CHECK(InitializeStateWithResult(state, use_atomic));
}

bool MockDrmDevice::InitializeStateWithResult(const MockDrmState& state,
                                              bool use_atomic) {
  UpdateStateBesidesPlaneManager(state);

  if (use_atomic) {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerAtomic>(this);
  } else {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
  }

  return plane_manager_->Initialize();
}

void MockDrmDevice::UpdateStateBesidesPlaneManager(const MockDrmState& state) {
  drm_state_ = state;
}

void MockDrmDevice::SetModifiersOverhead(
    base::flat_map<uint64_t /*modifier*/, int /*overhead*/>
        modifiers_overhead) {
  modifiers_overhead_ = modifiers_overhead;
}

void MockDrmDevice::SetSystemLimitOfModifiers(uint64_t limit) {
  system_watermark_limitations_ = limit;
}

ScopedDrmResourcesPtr MockDrmDevice::GetResources() {
  ScopedDrmResourcesPtr resources(DrmAllocator<drmModeRes>());
  resources->count_crtcs = drm_state_.crtc_properties.size();
  resources->crtcs = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_crtcs));
  for (size_t i = 0; i < drm_state_.crtc_properties.size(); ++i)
    resources->crtcs[i] = drm_state_.crtc_properties[i].id;

  resources->count_connectors = drm_state_.connector_properties.size();
  resources->connectors = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_connectors));
  for (size_t i = 0; i < drm_state_.connector_properties.size(); ++i)
    resources->connectors[i] = drm_state_.connector_properties[i].id;

  return resources;
}

ScopedDrmPlaneResPtr MockDrmDevice::GetPlaneResources() {
  ScopedDrmPlaneResPtr resources(DrmAllocator<drmModePlaneRes>());
  resources->count_planes = drm_state_.plane_properties.size();
  resources->planes = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_planes));
  for (size_t i = 0; i < drm_state_.plane_properties.size(); ++i)
    resources->planes[i] = drm_state_.plane_properties[i].id;

  return resources;
}

ScopedDrmObjectPropertyPtr MockDrmDevice::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) {
  if (object_type == DRM_MODE_OBJECT_PLANE) {
    PlaneProperties* properties =
        FindObjectById(object_id, drm_state_.plane_properties);
    if (properties)
      return CreatePropertyObject(properties->properties);
  } else if (object_type == DRM_MODE_OBJECT_CRTC) {
    CrtcProperties* properties =
        FindObjectById(object_id, drm_state_.crtc_properties);
    if (properties)
      return CreatePropertyObject(properties->properties);
  } else if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
    ConnectorProperties* properties =
        FindObjectById(object_id, drm_state_.connector_properties);
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
                            const drmModeModeInfo& mode) {
  crtc_fb_[crtc_id] = framebuffer;
  current_framebuffer_ = framebuffer;
  set_crtc_call_count_++;
  return set_crtc_expectation_;
}

bool MockDrmDevice::DisableCrtc(uint32_t crtc_id) {
  current_framebuffer_ = 0;
  return true;
}

ScopedDrmConnectorPtr MockDrmDevice::GetConnector(uint32_t connector_id) {
  return ScopedDrmConnectorPtr(DrmAllocator<drmModeConnector>());
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
  *framebuffer = GetUniqueNumber();
  framebuffer_ids_.insert(*framebuffer);
  fb_props_[*framebuffer] = {width, height, modifiers[0]};
  return add_framebuffer_expectation_;
}

bool MockDrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  {
    auto it = framebuffer_ids_.find(framebuffer);
    CHECK(it != framebuffer_ids_.end());
    framebuffer_ids_.erase(it);
  }
  {
    auto it = fb_props_.find(framebuffer);
    CHECK(it != fb_props_.end());
    fb_props_.erase(it);
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
  PlaneProperties* properties =
      FindObjectById(plane_id, drm_state_.plane_properties);
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
  auto it = drm_state_.property_names.find(id);
  if (it == drm_state_.property_names.end())
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

ScopedDrmPropertyBlob MockDrmDevice::CreatePropertyBlob(const void* blob,
                                                        size_t size) {
  uint32_t id = GetUniqueNumber();
  allocated_property_blobs_.insert(id);
  return std::make_unique<DrmPropertyBlobMetadata>(this, id);
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
  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  buffers_.push_back(SkSurface::MakeRasterDirectReleaseProc(
      info, pixels, *stride,
      [](void* pixels, void* context) { delete[] static_cast<char*>(pixels); },
      /*context=*/nullptr, &props));
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

bool MockDrmDevice::CommitPropertiesInternal(
    drmModeAtomicReq* request,
    uint32_t flags,
    uint32_t crtc_count,
    scoped_refptr<PageFlipRequest> page_flip_request) {
  commit_count_++;
  const bool test_only = flags & DRM_MODE_ATOMIC_TEST_ONLY;
  switch (flags) {
    case kTestModesetFlags:
      ++test_modeset_count_;
      break;
    case kCommitModesetFlags:
      ++commit_modeset_count_;
      break;
    case kSeamlessModesetFlags:
      ++seamless_modeset_count_;
      break;
  }

  if ((!test_only && !set_crtc_expectation_) ||
      (flags & DRM_MODE_ATOMIC_NONBLOCK && !commit_expectation_)) {
    return false;
  }

  uint64_t requested_resources = 0;
  base::flat_map<uint64_t, int> crtc_planes_counter;

  for (uint32_t i = 0; i < request->cursor; ++i) {
    const drmModeAtomicReqItem& item = request->items[i];
    if (!ValidatePropertyValue(item.property_id, item.value))
      return false;

    if (fb_props_.find(item.value) != fb_props_.end()) {
      const FramebufferProps& props = fb_props_[item.value];
      requested_resources += modifiers_overhead_[props.modifier];
    }

    if (item.property_id == kPlaneCrtcId) {
      if (++crtc_planes_counter[item.value] > 1 &&
          !modeset_with_overlays_expectation_)
        return false;
    }
  }

  if (requested_resources > system_watermark_limitations_) {
    LOG(ERROR) << "Requested display configuration exceeds system watermark "
                  "limitations";
    return false;
  }

  if (page_flip_request)
    callbacks_.push(page_flip_request->AddPageFlip());

  if (test_only)
    return true;

  // Only update values if not testing.
  for (uint32_t i = 0; i < request->cursor; ++i) {
    bool res =
        UpdateProperty(request->items[i].object_id,
                       request->items[i].property_id, request->items[i].value);
    if (!res)
      return false;
  }

  // Count all committed planes at the end just before returning true to
  // reflect the number of planes that have successfully been committed.
  last_planes_committed_count_ = 0;
  for (const auto& planes_counter : crtc_planes_counter)
    last_planes_committed_count_ += planes_counter.second;

  return true;
}

bool MockDrmDevice::SetGammaRamp(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& lut) {
  set_gamma_ramp_count_++;
  return legacy_gamma_ramp_expectation_;
}

absl::optional<std::string> MockDrmDevice::GetDriverName() const {
  return driver_name_;
}

void MockDrmDevice::SetDriverName(absl::optional<std::string> name) {
  driver_name_ = name;
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
      FindObjectById(object_id, drm_state_.plane_properties);
  if (plane_properties)
    return UpdateProperty(property_id, value, &plane_properties->properties);

  CrtcProperties* crtc_properties =
      FindObjectById(object_id, drm_state_.crtc_properties);
  if (crtc_properties)
    return UpdateProperty(property_id, value, &crtc_properties->properties);

  ConnectorProperties* connector_properties =
      FindObjectById(object_id, drm_state_.connector_properties);
  if (connector_properties) {
    return UpdateProperty(property_id, value,
                          &connector_properties->properties);
  }

  return false;
}

bool MockDrmDevice::ValidatePropertyValue(uint32_t id, uint64_t value) {
  auto it = drm_state_.property_names.find(id);
  if (it == drm_state_.property_names.end())
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
