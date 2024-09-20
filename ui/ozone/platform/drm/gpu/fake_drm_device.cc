// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "skia/ext/legacy_display_globals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

constexpr uint32_t kTestModesetFlags =
    DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET;

constexpr uint32_t kCommitModesetFlags = DRM_MODE_ATOMIC_ALLOW_MODESET;

// Seamless modeset is defined by the lack of DRM_MODE_ATOMIC_ALLOW_MODESET.
// This also happens to be the same set of flags as would be used for a
// pageflip, or other atomic property changes that do not require modesetting.
constexpr uint32_t kSeamlessModesetFlags = 0;

const std::vector<uint32_t> kBlobPropertyIds = {kEdidBlobPropId};

const ResolutionAndRefreshRate kStandardMode =
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 60u};

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
    {kEdidBlobPropId, "EDID"},
};

const std::map<uint32_t, std::string> kConnectorOptionalPropertyNames = {
    {kTileBlobPropId, "TILE"},
    {kVrrCapablePropId, "vrr_capable"},
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
    {kColorEncodingPropId, "COLOR_ENCODING"},
    {kColorRangePropId, "COLOR_RANGE"},
    {kSizeHintsPropId, "SIZE_HINTS"},
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

// The const version of FindObjectById().
template <class Type>
const Type* FindObjectById(uint32_t id, const std::vector<Type>& properties) {
  auto it = base::ranges::find(properties, id, &Type::id);
  return it != properties.end() ? &(*it) : nullptr;
}

// TODO(dnicoara): Generate all IDs internal to FakeDrmDevice.
// For now generate something with a high enough ID to be unique in tests.
uint32_t GetUniqueNumber() {
  static uint32_t value_generator = 0xff000000;
  return ++value_generator;
}

bool IsPropertyValueBlob(uint32_t prop_id) {
  return base::Contains(kBlobPropertyIds, prop_id);
}

}  // namespace

FakeDrmDevice::CrtcProperties::CrtcProperties() = default;
FakeDrmDevice::CrtcProperties::CrtcProperties(const CrtcProperties&) = default;
FakeDrmDevice::CrtcProperties::~CrtcProperties() = default;

FakeDrmDevice::ConnectorProperties::ConnectorProperties() = default;
FakeDrmDevice::ConnectorProperties::ConnectorProperties(
    const ConnectorProperties&) = default;
FakeDrmDevice::ConnectorProperties::~ConnectorProperties() = default;

FakeDrmDevice::EncoderProperties::EncoderProperties() = default;
FakeDrmDevice::EncoderProperties::EncoderProperties(const EncoderProperties&) =
    default;
FakeDrmDevice::EncoderProperties::~EncoderProperties() = default;

FakeDrmDevice::PlaneProperties::PlaneProperties() = default;
FakeDrmDevice::PlaneProperties::PlaneProperties(const PlaneProperties&) =
    default;
FakeDrmDevice::PlaneProperties::~PlaneProperties() = default;

FakeDrmDevice::FakeDrmState::FakeDrmState() = default;
FakeDrmDevice::FakeDrmState::~FakeDrmState() = default;

void FakeDrmDevice::ResetStateWithNoProperties() {
  allocated_blobs_.clear();
  plane_manager_.reset();

  drm_state_.crtc_properties.clear();
  drm_state_.connector_properties.clear();
  drm_state_.encoder_properties.clear();
  drm_state_.plane_properties.clear();
  drm_state_.property_names.clear();
}

void FakeDrmDevice::ResetStateWithAllProperties() {
  ResetStateWithNoProperties();
  drm_state_.property_names.insert(kCrtcRequiredPropertyNames.begin(),
                                   kCrtcRequiredPropertyNames.end());
  drm_state_.property_names.insert(kConnectorRequiredPropertyNames.begin(),
                                   kConnectorRequiredPropertyNames.end());
  drm_state_.property_names.insert(kPlaneRequiredPropertyNames.begin(),
                                   kPlaneRequiredPropertyNames.end());

  // Separately add optional properties that will be used in some tests, but the
  // tests will append the property to the planes on a case-by-case basis.
  drm_state_.property_names.insert(kCrtcOptionalPropertyNames.begin(),
                                   kCrtcOptionalPropertyNames.end());
  drm_state_.property_names.insert(kConnectorOptionalPropertyNames.begin(),
                                   kConnectorOptionalPropertyNames.end());
  drm_state_.property_names.insert(kPlaneOptionalPropertyNames.begin(),
                                   kPlaneOptionalPropertyNames.end());
}

FakeDrmDevice::FakeDrmState& FakeDrmDevice::ResetStateWithDefaultObjects(
    size_t crtc_count,
    size_t planes_per_crtc,
    size_t movable_planes,
    std::vector<uint32_t> plane_supported_formats,
    std::vector<drm_format_modifier> plane_supported_format_modifiers) {
  ResetStateWithAllProperties();
  std::vector<uint32_t> crtc_ids;
  for (size_t i = 0; i < crtc_count; ++i) {
    const auto& props = AddCrtcAndConnector();

    // Add at least one mode, so the connector is not sterile.
    ConnectorProperties& connector = props.second;
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardMode};

    // Add CRTC planes.
    uint32_t crtc_id = props.first.id;
    crtc_ids.push_back(crtc_id);

    AddPlane(crtc_id, DRM_PLANE_TYPE_PRIMARY, plane_supported_formats,
             plane_supported_format_modifiers);
    for (size_t j = 0; j < planes_per_crtc - 1; ++j) {
      AddPlane(crtc_id, DRM_PLANE_TYPE_OVERLAY, plane_supported_formats,
               plane_supported_format_modifiers);
    }
    AddPlane(crtc_id, DRM_PLANE_TYPE_CURSOR, plane_supported_formats,
             plane_supported_format_modifiers);
  }

  for (size_t i = 0; i < movable_planes; ++i) {
    AddPlane(crtc_ids, DRM_PLANE_TYPE_OVERLAY, plane_supported_formats,
             plane_supported_format_modifiers);
  }

  return drm_state_;
}

FakeDrmDevice::ConnectorProperties& FakeDrmDevice::AddConnector() {
  DCHECK(!IsInitialized());
  uint32_t next_connector_id =
      GetNextId(drm_state_.connector_properties, kConnectorIdBase);
  auto& connector_property = drm_state_.connector_properties.emplace_back();
  connector_property.connection = false;
  connector_property.id = next_connector_id;
  for (const auto& pair : kConnectorRequiredPropertyNames) {
    connector_property.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(drm_state_.property_names, pair.first)) {
      drm_state_.property_names.emplace(pair.first, pair.second);
    }
  }

  return {connector_property};
}

FakeDrmDevice::EncoderProperties& FakeDrmDevice::AddEncoder() {
  DCHECK(!IsInitialized());
  uint32_t next_encoder_id =
      GetNextId(drm_state_.encoder_properties, kEncoderIdBase);
  auto& encoder_property = drm_state_.encoder_properties.emplace_back();
  encoder_property.id = next_encoder_id;

  return {encoder_property};
}

FakeDrmDevice::CrtcProperties& FakeDrmDevice::AddCrtc() {
  DCHECK(!IsInitialized());
  uint32_t next_crtc_id = GetNextId(drm_state_.crtc_properties, kCrtcIdBase);
  auto& crtc_property = drm_state_.crtc_properties.emplace_back();
  crtc_property.id = next_crtc_id;
  for (const auto& pair : kCrtcRequiredPropertyNames) {
    crtc_property.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(drm_state_.property_names, pair.first)) {
      drm_state_.property_names.emplace(pair.first, pair.second);
    }
  }

  return crtc_property;
}

std::pair<FakeDrmDevice::CrtcProperties&, FakeDrmDevice::ConnectorProperties&>
FakeDrmDevice::AddCrtcAndConnector() {
  DCHECK(!IsInitialized());
  return {AddCrtc(), AddConnector()};
}

FakeDrmDevice::PlaneProperties& FakeDrmDevice::AddPlane(
    uint32_t crtc_id,
    uint32_t type,
    std::vector<uint32_t> supported_formats,
    std::vector<drm_format_modifier> supported_format_modifiers) {
  DCHECK(!IsInitialized());
  return AddPlane(std::vector<uint32_t>{crtc_id}, type, supported_formats,
                  supported_format_modifiers);
}

FakeDrmDevice::PlaneProperties& FakeDrmDevice::AddPlane(
    const std::vector<uint32_t>& crtc_ids,
    uint32_t type,
    std::vector<uint32_t> supported_formats,
    std::vector<drm_format_modifier> supported_format_modifiers) {
  DCHECK(!IsInitialized());
  uint32_t next_plane_id = GetNextId(drm_state_.plane_properties, kPlaneOffset);

  size_t crtc_mask = 0u;
  for (size_t i = 0; i < drm_state_.crtc_properties.size(); ++i) {
    if (base::Contains(crtc_ids, drm_state_.crtc_properties[i].id)) {
      crtc_mask |= (1 << i);
    }
  }
  CHECK(crtc_mask != 0) << "Unable to create crtc_mask";

  auto& plane = drm_state_.plane_properties.emplace_back();
  plane.id = next_plane_id;
  plane.crtc_mask = crtc_mask;
  for (const auto& pair : kPlaneRequiredPropertyNames) {
    plane.properties.push_back({.id = pair.first, .value = 0});
    if (!base::Contains(drm_state_.property_names, pair.first)) {
      drm_state_.property_names.emplace(pair.first, pair.second);
    }
  }

  AddProperty(plane.id, {.id = kTypePropId, .value = type});

  auto in_formats_blob =
      CreateInFormatsBlob(supported_formats, supported_format_modifiers);
  AddProperty(plane.id,
              {.id = kInFormatsPropId, .value = in_formats_blob->id()});

  return plane;
}

bool FakeDrmDevice::FakeDrmState::HasResources() const {
  return !connector_properties.empty() || !crtc_properties.empty() ||
         !encoder_properties.empty();
}

FakeDrmDevice::CrtcProperties&
FakeDrmDevice::AddCrtcWithPrimaryAndCursorPlanes() {
  DCHECK(!IsInitialized());
  auto& crtc = AddCrtc();
  AddPlane(crtc.id, DRM_PLANE_TYPE_PRIMARY);
  AddPlane(crtc.id, DRM_PLANE_TYPE_CURSOR);
  return crtc;
}

FakeDrmDevice::FakeDrmDevice(std::unique_ptr<GbmDevice> gbm_device)
    : DrmDevice(base::FilePath(),
                base::ScopedFD(),
                true /* is_primary_device */,
                std::move(gbm_device)) {
}

FakeDrmDevice::FakeDrmDevice(const base::FilePath& path,
                             std::unique_ptr<GbmDevice> gbm_device,
                             bool is_primary_device)
    : DrmDevice(std::move(path),
                base::ScopedFD(),
                is_primary_device,
                std::move(gbm_device)) {
}

FakeDrmDevice::~FakeDrmDevice() {
  if (plane_manager_) {
    plane_manager_.reset();
  }
}

ScopedDrmPropertyBlob FakeDrmDevice::CreateInFormatsBlob(
    const std::vector<uint32_t>& supported_formats,
    const std::vector<drm_format_modifier>& supported_format_modifiers) {
  drm_format_modifier_blob header;
  header.count_formats = supported_formats.size();
  header.formats_offset = sizeof(header);
  header.count_modifiers = supported_format_modifiers.size();
  header.modifiers_offset =
      header.formats_offset + sizeof(uint32_t) * header.count_formats;

  std::vector<uint8_t> data(header.modifiers_offset +
                            sizeof(drm_format_modifier) *
                                header.count_modifiers);
  memcpy(data.data(), &header, sizeof(header));
  memcpy(data.data() + header.formats_offset, supported_formats.data(),
         sizeof(uint32_t) * header.count_formats);
  memcpy(data.data() + header.modifiers_offset,
         supported_format_modifiers.data(),
         sizeof(drm_format_modifier) * header.count_modifiers);

  return CreatePropertyBlob(data.data(), data.size());
}

ScopedDrmPropertyBlob FakeDrmDevice::CreateSizeHintsBlob(
    const std::vector<gfx::Size>& sizes) {
  std::vector<drm_plane_size_hint> hints(sizes.size());
  for (size_t i = 0; i < sizes.size(); i++) {
    hints[i].width = sizes[i].width();
    hints[i].height = sizes[i].height();
  }
  std::vector<uint8_t> data(sizeof(drm_plane_size_hint) * hints.size());
  memcpy(data.data(), hints.data(), sizeof(drm_plane_size_hint) * hints.size());
  return CreatePropertyBlob(data.data(), data.size());
}

void FakeDrmDevice::InitializeState(bool use_atomic) {
  CHECK(InitializeStateWithResult(use_atomic));
}

bool FakeDrmDevice::InitializeStateWithResult(bool use_atomic) {
  DCHECK(!plane_manager_);
  if (use_atomic) {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerAtomic>(this);
  } else {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
  }
  SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  // Update the connectors' link statuses to bad if they have no modes (probably
  // due to unsuccessful link-training.
  for (FakeDrmDevice::ConnectorProperties& connector :
       drm_state_.connector_properties) {
    if (connector.connection && connector.modes.empty()) {
      DrmWrapper::Property* connector_link_status =
          FindObjectById(kLinkStatusPropId, connector.properties);
      if (connector_link_status) {
        connector_link_status->value = DRM_MODE_LINK_STATUS_BAD;
      }
    }
  }

  // Set EDID blobs as property blobs so they can be fetched when needed via
  // GetPropertyBlob().
  for (auto& mock_connector : drm_state_.connector_properties) {
    const std::vector<uint8_t> edid_blob = mock_connector.edid_blob;
    if (edid_blob.empty()) {
      continue;
    }

    auto edid_property_blob =
        CreatePropertyBlob(edid_blob.data(), edid_blob.size());
    UpdateProperty(mock_connector.id, kEdidBlobPropId,
                   edid_property_blob->id());
  }

  return plane_manager_->Initialize();
}

void FakeDrmDevice::SetModifiersOverhead(
    base::flat_map<uint64_t /*modifier*/, int /*overhead*/>
        modifiers_overhead) {
  modifiers_overhead_ = modifiers_overhead;
}

void FakeDrmDevice::SetSystemLimitOfModifiers(uint64_t limit) {
  system_watermark_limitations_ = limit;
}

ScopedDrmResourcesPtr FakeDrmDevice::GetResources() const {
  if (!drm_state_.HasResources())
    return nullptr;

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

  resources->count_encoders = drm_state_.encoder_properties.size();
  resources->encoders = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_encoders));
  for (size_t i = 0; i < drm_state_.encoder_properties.size(); ++i)
    resources->encoders[i] = drm_state_.encoder_properties[i].id;

  return resources;
}

ScopedDrmPlaneResPtr FakeDrmDevice::GetPlaneResources() const {
  ScopedDrmPlaneResPtr resources(DrmAllocator<drmModePlaneRes>());
  resources->count_planes = drm_state_.plane_properties.size();
  resources->planes = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_planes));
  for (size_t i = 0; i < drm_state_.plane_properties.size(); ++i)
    resources->planes[i] = drm_state_.plane_properties[i].id;

  return resources;
}

ScopedDrmObjectPropertyPtr FakeDrmDevice::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) const {
  if (object_type == DRM_MODE_OBJECT_PLANE) {
    const PlaneProperties* properties =
        FindObjectById(object_id, drm_state_.plane_properties);
    if (properties)
      return CreatePropertyObject(properties->properties);
  } else if (object_type == DRM_MODE_OBJECT_CRTC) {
    const CrtcProperties* properties =
        FindObjectById(object_id, drm_state_.crtc_properties);
    if (properties)
      return CreatePropertyObject(properties->properties);
  } else if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
    const ConnectorProperties* properties =
        FindObjectById(object_id, drm_state_.connector_properties);
    if (properties)
      return CreatePropertyObject(properties->properties);
  }

  return nullptr;
}

ScopedDrmCrtcPtr FakeDrmDevice::GetCrtc(uint32_t crtc_id) const {
  const CrtcProperties* mock_crtc =
      FindObjectById(crtc_id, drm_state_.crtc_properties);
  if (!mock_crtc)
    return nullptr;

  ScopedDrmCrtcPtr crtc(DrmAllocator<drmModeCrtc>());
  crtc->crtc_id = mock_crtc->id;

  return crtc;
}

bool FakeDrmDevice::SetCrtc(uint32_t crtc_id,
                            uint32_t framebuffer,
                            std::vector<uint32_t> connectors,
                            const drmModeModeInfo& mode) {
  crtc_fb_[crtc_id] = framebuffer;
  current_framebuffer_ = framebuffer;
  set_crtc_call_count_++;
  return set_crtc_expectation_;
}

bool FakeDrmDevice::DisableCrtc(uint32_t crtc_id) {
  current_framebuffer_ = 0;
  return true;
}

ScopedDrmConnectorPtr FakeDrmDevice::GetConnector(uint32_t connector_id) const {
  const ConnectorProperties* mock_connector =
      FindObjectById(connector_id, drm_state_.connector_properties);
  if (!mock_connector)
    return nullptr;

  ScopedDrmConnectorPtr connector(DrmAllocator<drmModeConnector>());
  connector->connector_id = mock_connector->id;
  connector->connection =
      mock_connector->connection ? DRM_MODE_CONNECTED : DRM_MODE_DISCONNECTED;

  // Copy props.
  const uint32_t count_props = mock_connector->properties.size();
  connector->count_props = count_props;
  connector->props = DrmAllocator<uint32_t>(count_props);
  connector->prop_values = DrmAllocator<uint64_t>(count_props);
  for (uint32_t i = 0; i < count_props; ++i) {
    connector->props[i] = mock_connector->properties[i].id;
    connector->prop_values[i] = mock_connector->properties[i].value;
  }

  // Copy modes.
  const uint32_t count_modes = mock_connector->modes.size();
  connector->count_modes = count_modes;
  connector->modes = DrmAllocator<drmModeModeInfo>(count_modes);
  for (uint32_t i = 0; i < count_modes; ++i) {
    const gfx::Size resolution = mock_connector->modes[i].first;
    const uint32_t vrefresh = mock_connector->modes[i].second;
    connector->modes[i].clock = resolution.GetArea() * vrefresh / 1000;
    connector->modes[i].hdisplay = resolution.width();
    connector->modes[i].htotal = resolution.width();
    connector->modes[i].vdisplay = resolution.height();
    connector->modes[i].vtotal = resolution.height();
    connector->modes[i].vrefresh = vrefresh;
  }

  // Copy associated encoders.
  const uint32_t count_encoders = mock_connector->encoders.size();
  connector->count_encoders = count_encoders;
  connector->encoders = DrmAllocator<uint32_t>(count_encoders);
  for (uint32_t i = 0; i < count_encoders; ++i)
    connector->encoders[i] = mock_connector->encoders[i];

  return connector;
}

ScopedDrmEncoderPtr FakeDrmDevice::GetEncoder(uint32_t encoder_id) const {
  const EncoderProperties* mock_encoder =
      FindObjectById(encoder_id, drm_state_.encoder_properties);
  if (!mock_encoder)
    return nullptr;

  ScopedDrmEncoderPtr encoder(DrmAllocator<drmModeEncoder>());
  encoder->encoder_id = mock_encoder->id;
  encoder->possible_crtcs = mock_encoder->possible_crtcs;

  return encoder;
}

bool FakeDrmDevice::AddFramebuffer2(uint32_t width,
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

bool FakeDrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
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

ScopedDrmFramebufferPtr FakeDrmDevice::GetFramebuffer(
    uint32_t framebuffer) const {
  return ScopedDrmFramebufferPtr();
}

bool FakeDrmDevice::PageFlip(uint32_t crtc_id,
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

ScopedDrmPlanePtr FakeDrmDevice::GetPlane(uint32_t plane_id) const {
  const PlaneProperties* properties =
      FindObjectById(plane_id, drm_state_.plane_properties);
  if (!properties)
    return nullptr;

  ScopedDrmPlanePtr plane(DrmAllocator<drmModePlane>());
  plane->possible_crtcs = properties->crtc_mask;
  return plane;
}

ScopedDrmPropertyPtr FakeDrmDevice::GetProperty(drmModeConnector* connector,
                                                const char* name) const {
  return ScopedDrmPropertyPtr(DrmAllocator<drmModePropertyRes>());
}

ScopedDrmPropertyPtr FakeDrmDevice::GetProperty(uint32_t id) const {
  auto it = drm_state_.property_names.find(id);
  if (it == drm_state_.property_names.end())
    return nullptr;

  ScopedDrmPropertyPtr property(DrmAllocator<drmModePropertyRes>());
  property->prop_id = id;
  strcpy(property->name, it->second.c_str());

  if (IsPropertyValueBlob(property->prop_id)) {
    property->flags = DRM_MODE_PROP_BLOB;
  } else if (IsPropertyValueEnum(property->prop_id)) {
    FillPossibleValuesForEnumProperty(property.get());
    property->flags = DRM_MODE_PROP_ENUM;
  }

  return property;
}

bool FakeDrmDevice::SetProperty(uint32_t connector_id,
                                uint32_t property_id,
                                uint64_t value) {
  return true;
}

ScopedDrmPropertyBlob FakeDrmDevice::CreatePropertyBlob(const void* blob,
                                                        size_t size) {
  uint32_t id = GetUniqueNumber();
  auto& blob_state = allocated_blobs_[id];
  DCHECK(blob_state.ref_count == 0);
  blob_state.ref_count = 1;
  const uint8_t* blob_uint8 = reinterpret_cast<const uint8_t*>(blob);
  blob_state.data.assign(blob_uint8, blob_uint8 + size);
  return std::make_unique<DrmPropertyBlobMetadata>(this, id);
}

void FakeDrmDevice::DestroyPropertyBlob(uint32_t id) {
  bool did_release = ReleaseBlob(id);
  // ReleaseBlob will return true if there exists a blob with `id`. It should
  // never be the case that we are are destroying a blob that does not exist.
  DCHECK(did_release);
}

FakeDrmDevice::BlobState::BlobState() = default;

FakeDrmDevice::BlobState::BlobState(const BlobState&) = default;

FakeDrmDevice::BlobState::~BlobState() = default;

bool FakeDrmDevice::RetainBlob(uint32_t id) {
  auto it = allocated_blobs_.find(id);
  if (it == allocated_blobs_.end()) {
    return false;
  }
  DCHECK(it->second.ref_count > 0);
  it->second.ref_count += 1;
  return true;
}

bool FakeDrmDevice::ReleaseBlob(uint32_t id) {
  auto it = allocated_blobs_.find(id);
  if (it == allocated_blobs_.end()) {
    return false;
  }
  DCHECK(it->second.ref_count > 0);
  it->second.ref_count -= 1;
  if (it->second.ref_count == 0) {
    allocated_blobs_.erase(it);
  }
  return true;
}

bool FakeDrmDevice::GetCapability(uint64_t capability, uint64_t* value) const {
  const auto it = capabilities_.find(capability);
  if (it == capabilities_.end())
    return false;

  *value = it->second;
  return true;
}

ScopedDrmPropertyBlobPtr FakeDrmDevice::GetPropertyBlob(
    uint32_t property_id) const {
  auto it = allocated_blobs_.find(property_id);
  if (it == allocated_blobs_.end()) {
    return nullptr;
  }

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  blob->id = property_id;
  blob->length = it->second.data.size();
  blob->data = drmMalloc(blob->length);
  memcpy(blob->data, it->second.data.data(), blob->length);

  return blob;
}

ScopedDrmPropertyBlobPtr FakeDrmDevice::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) const {
  const ConnectorProperties* mock_connector =
      FindObjectById(connector->connector_id, drm_state_.connector_properties);
  if (!mock_connector)
    return nullptr;

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  for (const auto& prop : mock_connector->properties) {
    auto prop_name_it = drm_state_.property_names.find(prop.id);
    if (prop_name_it == drm_state_.property_names.end())
      continue;

    if (prop_name_it->second.compare(name) != 0)
      continue;

    return GetPropertyBlob(prop.value);
  }

  return nullptr;
}

bool FakeDrmDevice::SetObjectProperty(uint32_t object_id,
                                      uint32_t object_type,
                                      uint32_t property_id,
                                      uint32_t property_value) {
  UpdateProperty(object_id, property_id, property_value,
                 /*add_property_if_needed=*/false);
  set_object_property_count_++;
  return true;
}

bool FakeDrmDevice::SetCursor(uint32_t crtc_id,
                              uint32_t handle,
                              const gfx::Size& size) {
  crtc_cursor_map_[crtc_id] = handle;
  return true;
}

bool FakeDrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  crtc_cursor_location_map_[crtc_id] = point;
  return true;
}

bool FakeDrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                     uint32_t* handle,
                                     uint32_t* stride) {
  if (!create_dumb_buffer_expectation_)
    return false;

  // |handle| should start from 1. 0 is considered an invalid handle.
  *handle = ++allocate_buffer_count_;
  *stride = info.minRowBytes();
  void* pixels = new char[info.computeByteSize(*stride)];
  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  buffers_[*handle] = SkSurfaces::WrapPixels(
      info, pixels, *stride,
      [](void* pixels, void* context) { delete[] static_cast<char*>(pixels); },
      /*context=*/nullptr, &props);
  buffers_[*handle]->getCanvas()->clear(SK_ColorBLACK);

  return true;
}

bool FakeDrmDevice::DestroyDumbBuffer(uint32_t handle) {
  if (handle > buffers_.size() || !buffers_[handle]) {
    return false;
  }

  buffers_[handle].reset();
  return true;
}

bool FakeDrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  if (handle > buffers_.size() || !buffers_[handle]) {
    return false;
  }

  SkPixmap pixmap;
  buffers_[handle]->peekPixels(&pixmap);
  *pixels = const_cast<void*>(pixmap.addr());
  return true;
}

bool FakeDrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return true;
}

bool FakeDrmDevice::CloseBufferHandle(uint32_t handle) {
  return true;
}

bool FakeDrmDevice::CommitProperties(
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

  // Increment modeset sequence ID upon success.
  if (flags == DRM_MODE_ATOMIC_ALLOW_MODESET)
    ++modeset_sequence_id_;

  // Count all committed planes at the end just before returning true to
  // reflect the number of planes that have successfully been committed.
  last_planes_committed_count_ = 0;
  for (const auto& planes_counter : crtc_planes_counter)
    last_planes_committed_count_ += planes_counter.second;

  return true;
}

bool FakeDrmDevice::SetGammaRamp(uint32_t crtc_id,
                                 const display::GammaCurve& curve) {
  set_gamma_ramp_count_++;
  return legacy_gamma_ramp_expectation_;
}

std::optional<std::string> FakeDrmDevice::GetDriverName() const {
  return driver_name_;
}

void FakeDrmDevice::SetDriverName(std::optional<std::string> name) {
  driver_name_ = name;
}

bool FakeDrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  capabilities_.insert({capability, value});
  return true;
}

uint32_t FakeDrmDevice::GetFramebufferForCrtc(uint32_t crtc_id) const {
  auto it = crtc_fb_.find(crtc_id);
  return it != crtc_fb_.end() ? it->second : 0u;
}

void FakeDrmDevice::RunCallbacks() {
  while (!callbacks_.empty()) {
    PageFlipCallback callback = std::move(callbacks_.front());
    callbacks_.pop();
    std::move(callback).Run(0, base::TimeTicks());
  }
}

void FakeDrmDevice::AddProperty(uint32_t object_id,
                                const DrmWrapper::Property& property) {
  DCHECK(!IsInitialized());
  UpdateProperty(object_id, property.id, property.value,
                 /*add_property_if_needed=*/true);
}

void FakeDrmDevice::SetPossibleValuesForEnumProperty(
    uint32_t property_id,
    std::vector<std::pair<uint64_t /* value */, std::string /* name */>>
        values) {
  DCHECK(!IsInitialized());
  DCHECK(!values.empty());
  drm_state_.enum_values[property_id] = std::move(values);
}

bool FakeDrmDevice::IsPropertyValueEnum(uint32_t prop_id) const {
  return drm_state_.enum_values.find(prop_id) != drm_state_.enum_values.end();
}

void FakeDrmDevice::FillPossibleValuesForEnumProperty(
    drmModePropertyRes* property) const {
  DCHECK(IsPropertyValueEnum(property->prop_id));

  const std::vector<std::pair<uint64_t, std::string>>& enum_values =
      drm_state_.enum_values.find(property->prop_id)->second;

  property->count_enums = enum_values.size();
  property->enums = DrmAllocator<drm_mode_property_enum>(enum_values.size());

  for (size_t i = 0; i < enum_values.size(); i++) {
    property->enums[i].value = enum_values[i].first;
    strcpy(property->enums[i].name, enum_values[i].second.c_str());
  }
}

bool FakeDrmDevice::UpdateProperty(uint32_t id,
                                   uint64_t value,
                                   std::vector<DrmDevice::Property>* properties,
                                   bool add_property_if_needed) {
  DrmDevice::Property* property = FindObjectById(id, *properties);
  if (!property) {
    if (!add_property_if_needed) {
      return false;
    }

    properties->push_back({.id = id, .value = 0});
    property = &properties->back();
  }

  // Retain the blob corresponding to the new value (if one exists). This
  // ensures that the blob will remain valid even if DestroyPropertyBlob is
  // subsequently called on it.
  RetainBlob(value);

  // Release the blob corresponding to the old value (if one exists). This may
  // trigger the destruction of the blob, if DestroyPropertyBlob has already
  // been called on it.
  ReleaseBlob(property->value);

  property->value = value;
  return true;
}

bool FakeDrmDevice::UpdateProperty(uint32_t object_id,
                                   uint32_t property_id,
                                   uint64_t value,
                                   bool add_property_if_needed) {
  PlaneProperties* plane_properties =
      FindObjectById(object_id, drm_state_.plane_properties);
  if (plane_properties) {
    return UpdateProperty(property_id, value, &plane_properties->properties,
                          add_property_if_needed);
  }

  CrtcProperties* crtc_properties =
      FindObjectById(object_id, drm_state_.crtc_properties);
  if (crtc_properties) {
    return UpdateProperty(property_id, value, &crtc_properties->properties,
                          add_property_if_needed);
  }

  ConnectorProperties* connector_properties =
      FindObjectById(object_id, drm_state_.connector_properties);
  if (connector_properties) {
    return UpdateProperty(property_id, value, &connector_properties->properties,
                          add_property_if_needed);
  }

  return false;
}

bool FakeDrmDevice::ValidatePropertyValue(uint32_t id, uint64_t value) {
  auto it = drm_state_.property_names.find(id);
  if (it == drm_state_.property_names.end())
    return false;

  if (value == 0)
    return true;

  std::vector<std::string> blob_properties = {"CTM", "DEGAMMA_LUT", "GAMMA_LUT",
                                              "PLANE_CTM"};
  if (base::Contains(blob_properties, it->second))
    return base::Contains(allocated_blobs_, value);

  return true;
}

int FakeDrmDevice::modeset_sequence_id() const {
  return modeset_sequence_id_;
}

void FakeDrmDevice::ResetPlaneManagerForTesting() {
  plane_manager_.reset();
}

void FakeDrmDevice::ClearCallbacks() {
  base::queue<PageFlipCallback> empty;
  callbacks_.swap(empty);
}

}  // namespace ui
