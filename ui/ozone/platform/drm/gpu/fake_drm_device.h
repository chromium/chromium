// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_H_

#include <drm_fourcc.h>
#include <drm_mode.h>
#include <stddef.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

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

using ResolutionAndRefreshRate = std::pair<gfx::Size, uint32_t>;

template <class Object>
Object* DrmAllocator(size_t num_of_objects = 1) {
  return static_cast<Object*>(drmMalloc(num_of_objects * sizeof(Object)));
}

// DRM Object Base IDs:
inline constexpr uint32_t kPlaneOffset = 100;
inline constexpr uint32_t kCrtcIdBase = 200;
inline constexpr uint32_t kConnectorIdBase = 300;
inline constexpr uint32_t kEncoderIdBase = 400;

// Required Connector Property IDs:
inline constexpr uint32_t kCrtcIdPropId = 1000;
inline constexpr uint32_t kLinkStatusPropId = 1001;
inline constexpr uint32_t kEdidBlobPropId = 1002;

// Optional Connector Property IDs:
inline constexpr uint32_t kTileBlobPropId = 1500;
inline constexpr uint32_t kVrrCapablePropId = 1501;

// Required CRTC Property IDs:
inline constexpr uint32_t kActivePropId = 2000;
inline constexpr uint32_t kModePropId = 2001;

// Optional CRTC Property IDs:
inline constexpr uint32_t kBackgroundColorPropId = 3000;
inline constexpr uint32_t kCtmPropId = 3001;
inline constexpr uint32_t kDegammaLutPropId = 3002;
inline constexpr uint32_t kDegammaLutSizePropId = 3003;
inline constexpr uint32_t kGammaLutPropId = 3004;
inline constexpr uint32_t kGammaLutSizePropId = 3005;
inline constexpr uint32_t kInFencePropId = 3006;
inline constexpr uint32_t kOutFencePtrPropId = 3007;
inline constexpr uint32_t kVrrEnabledPropId = 3008;

// Required Plane Property IDs:
inline constexpr uint32_t kCrtcH = 4001;
inline constexpr uint32_t kCrtcW = 4002;
inline constexpr uint32_t kCrtcX = 4003;
inline constexpr uint32_t kCrtcY = 4004;
inline constexpr uint32_t kPlaneCrtcId = 4005;
inline constexpr uint32_t kPlaneFbId = 4006;
inline constexpr uint32_t kSrcH = 4007;
inline constexpr uint32_t kSrcW = 4008;
inline constexpr uint32_t kSrcX = 4009;
inline constexpr uint32_t kSrcY = 4010;

// Optional Plane Property IDs:
inline constexpr uint32_t kTypePropId = 5000;
inline constexpr uint32_t kInFormatsPropId = 5001;
inline constexpr uint32_t kPlaneCtmId = 5002;
inline constexpr uint32_t kRotationPropId = 5003;
inline constexpr uint32_t kColorEncodingPropId = 5004;
inline constexpr uint32_t kColorRangePropId = 5005;
inline constexpr uint32_t kSizeHintsPropId = 5006;

// Blob IDs:
inline constexpr uint32_t kBaseBlobId = 6000;
inline constexpr uint32_t kTileBlobId = 6100;

// The real DrmDevice makes actual DRM calls which we can't use in unit tests.
class FakeDrmDevice : public DrmDevice {
 public:
  struct CrtcProperties {
    CrtcProperties();
    CrtcProperties(const CrtcProperties&);
    ~CrtcProperties();

    uint32_t id;

    std::vector<DrmWrapper::Property> properties;
  };

  struct ConnectorProperties {
    ConnectorProperties();
    ConnectorProperties(const ConnectorProperties&);
    ~ConnectorProperties();

    uint32_t id;
    bool connection;
    std::vector<ResolutionAndRefreshRate> modes;
    std::vector<uint32_t> encoders;
    std::vector<uint8_t> edid_blob;

    std::vector<DrmWrapper::Property> properties;
  };

  struct EncoderProperties {
    EncoderProperties();
    EncoderProperties(const EncoderProperties&);
    ~EncoderProperties();

    uint32_t id;
    uint32_t possible_crtcs;
  };

  struct PlaneProperties {
    PlaneProperties();
    PlaneProperties(const PlaneProperties&);
    ~PlaneProperties();

    uint32_t id;
    uint32_t crtc_mask;

    std::vector<DrmWrapper::Property> properties;
  };

  struct FakeDrmState {
    FakeDrmState();
    FakeDrmState(const FakeDrmState&) = delete;
    FakeDrmState& operator=(const FakeDrmState&) = delete;
    ~FakeDrmState();

    bool HasResources() const;

    std::vector<CrtcProperties> crtc_properties;
    std::vector<ConnectorProperties> connector_properties;
    std::vector<EncoderProperties> encoder_properties;
    std::vector<PlaneProperties> plane_properties;
    std::map<uint32_t, std::string> property_names;
    std::map<uint32_t, std::vector<std::pair<uint64_t, std::string>>>
        enum_values;
  };

  explicit FakeDrmDevice(std::unique_ptr<GbmDevice> gbm_device);
  explicit FakeDrmDevice(const base::FilePath& path,
                         std::unique_ptr<GbmDevice> gbm_device,
                         bool is_primary_device);

  FakeDrmDevice(const FakeDrmDevice&) = delete;
  FakeDrmDevice& operator=(const FakeDrmDevice&) = delete;

  ScopedDrmPropertyBlob CreateInFormatsBlob(
      const std::vector<uint32_t>& supported_formats,
      const std::vector<drm_format_modifier>& supported_format_modifiers);
  ScopedDrmPropertyBlob CreateSizeHintsBlob(
      const std::vector<gfx::Size>& sizes);
  int get_set_crtc_call_count() const { return set_crtc_call_count_; }
  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }
  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }
  int get_page_flip_call_count() const { return page_flip_call_count_; }
  int get_overlay_clear_call_count() const { return overlay_clear_call_count_; }
  int get_test_modeset_count() const { return test_modeset_count_; }
  int get_commit_modeset_count() const { return commit_modeset_count_; }
  int get_seamless_modeset_count() const { return seamless_modeset_count_; }
  int get_commit_count() const { return commit_count_; }
  int get_set_object_property_count() const {
    return set_object_property_count_;
  }
  int get_set_gamma_ramp_count() const { return set_gamma_ramp_count_; }
  void set_set_crtc_expectation(bool state) { set_crtc_expectation_ = state; }
  void set_page_flip_expectation(bool state) { page_flip_expectation_ = state; }
  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }
  void set_create_dumb_buffer_expectation(bool state) {
    create_dumb_buffer_expectation_ = state;
  }
  void set_legacy_gamma_ramp_expectation(bool state) {
    legacy_gamma_ramp_expectation_ = state;
  }
  void set_commit_expectation(bool state) { commit_expectation_ = state; }
  void set_overlay_modeset_expectation(bool state) {
    modeset_with_overlays_expectation_ = state;
  }

  uint32_t current_framebuffer() const { return current_framebuffer_; }

  const base::flat_map<uint32_t, sk_sp<SkSurface>> buffers() const {
    return buffers_;
  }

  int last_planes_committed_count() const {
    return last_planes_committed_count_;
  }
  int modeset_sequence_id() const override;

  uint32_t get_cursor_handle_for_crtc(uint32_t crtc) const {
    const auto it = crtc_cursor_map_.find(crtc);
    return it != crtc_cursor_map_.end() ? it->second : 0;
  }

  gfx::Point get_crtc_cursor_location(uint32_t crtc) const {
    const auto it = crtc_cursor_location_map_.find(crtc);
    return it != crtc_cursor_location_map_.end() ? it->second : gfx::Point();
  }

  // Resets `drm_state_` to be empty, with no properties configured and no
  // property names set. Resets `plane_manager_` to nullptr (it will not be
  // re-created until InitializeState is called).
  void ResetStateWithNoProperties();

  // Calls `ResetStateWithNoProperties`, then configures `drm_state_` to have
  // all properties registered with their names in `property_names`, but no
  // objects configured.
  void ResetStateWithAllProperties();

  // Calls `ResetStateWithNoProperties`, and then configures `drm_state_`. Will
  // create `crtc_count` different CRTCs and connectors with 1 primary plane, 1
  // cursor plane (since some tests expect them), and `planes_per_crtc` - 1
  // overlay planes for each CRTC. All planes will be allocated with the
  // specified `plane_supported_formats` and `plane_supported_format_modifiers`.
  // Returns a reference to `drm_state_`.
  // TODO(b/335542790): Update tests to not need the returned reference to
  // `drm_state_`.
  FakeDrmDevice::FakeDrmState& ResetStateWithDefaultObjects(
      size_t crtc_count,
      size_t planes_per_crtc,
      size_t movable_planes = 0u,
      std::vector<uint32_t> plane_supported_formats = {DRM_FORMAT_XRGB8888},
      std::vector<drm_format_modifier> plane_supported_format_modifiers = {});

  // Create `plane_manager_`, set the connector link status, and the EDID
  // blob.
  void InitializeState(bool use_atomic);
  bool InitializeStateWithResult(bool use_atomic);

  // Return true if InitializeState has been called yet.
  bool IsInitialized() const { return !!plane_manager_; }

  void RunCallbacks();

  // Add a `property.id` to `object_id`, and set its value to `property.value`.
  // This can only be called before InitializeState.
  void AddProperty(uint32_t object_id, const DrmWrapper::Property& property);

  // Configure the possible enum values for a particular property. This can only
  // be called before InitializeState.
  void SetPossibleValuesForEnumProperty(
      uint32_t property_id,
      std::vector<std::pair<uint64_t /* value */, std::string /* name */>>
          values);

  // Functions to configure the FakeDrmState. Must be called before Initialize
  // is called.
  CrtcProperties& AddCrtc();
  CrtcProperties& AddCrtcWithPrimaryAndCursorPlanes();
  std::pair<CrtcProperties&, ConnectorProperties&> AddCrtcAndConnector();
  PlaneProperties& AddPlane(
      uint32_t crtc_id,
      uint32_t type,
      std::vector<uint32_t> supported_formats = {DRM_FORMAT_XRGB8888},
      std::vector<drm_format_modifier> supported_format_modifiers = {});
  PlaneProperties& AddPlane(
      const std::vector<uint32_t>& crtc_ids,
      uint32_t type,
      std::vector<uint32_t> supported_formats = {DRM_FORMAT_XRGB8888},
      std::vector<drm_format_modifier> supported_format_modifiers = {});
  ConnectorProperties& AddConnector();
  EncoderProperties& AddEncoder();

  void SetModifiersOverhead(base::flat_map<uint64_t, int> modifiers_overhead);
  void SetSystemLimitOfModifiers(uint64_t limit);

  const CrtcProperties& crtc_property(size_t idx) const {
    return drm_state_.crtc_properties[idx];
  }
  const ConnectorProperties& connector_property(size_t idx) const {
    return drm_state_.connector_properties[idx];
  }
  const PlaneProperties& plane_property(size_t idx) const {
    return drm_state_.plane_properties[idx];
  }

  const std::vector<CrtcProperties>& crtc_properties() const {
    return drm_state_.crtc_properties;
  }
  const std::vector<ConnectorProperties>& connector_properties() const {
    return drm_state_.connector_properties;
  }
  const std::vector<PlaneProperties>& plane_properties() const {
    return drm_state_.plane_properties;
  }
  const std::map<uint32_t, std::string>& property_names() const {
    return drm_state_.property_names;
  }

  // DrmDevice:
  ScopedDrmResourcesPtr GetResources() const override;
  ScopedDrmPlaneResPtr GetPlaneResources() const override;
  ScopedDrmObjectPropertyPtr GetObjectProperties(
      uint32_t object_id,
      uint32_t object_type) const override;
  ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id) const override;
  bool SetCrtc(uint32_t crtc_id,
               uint32_t framebuffer,
               std::vector<uint32_t> connectors,
               const drmModeModeInfo& mode) override;
  bool DisableCrtc(uint32_t crtc_id) override;
  ScopedDrmConnectorPtr GetConnector(uint32_t connector_id) const override;
  ScopedDrmEncoderPtr GetEncoder(uint32_t encoder_id) const override;
  bool AddFramebuffer2(uint32_t width,
                       uint32_t height,
                       uint32_t format,
                       uint32_t handles[4],
                       uint32_t strides[4],
                       uint32_t offsets[4],
                       uint64_t modifiers[4],
                       uint32_t* framebuffer,
                       uint32_t flags) override;
  bool RemoveFramebuffer(uint32_t framebuffer) override;
  ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer) const override;
  bool PageFlip(uint32_t crtc_id,
                uint32_t framebuffer,
                scoped_refptr<PageFlipRequest> page_flip_request) override;
  ScopedDrmPlanePtr GetPlane(uint32_t plane_id) const override;
  ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                   const char* name) const override;
  ScopedDrmPropertyPtr GetProperty(uint32_t id) const override;
  bool SetProperty(uint32_t connector_id,
                   uint32_t property_id,
                   uint64_t value) override;
  ScopedDrmPropertyBlob CreatePropertyBlob(const void* blob,
                                           size_t size) override;
  void DestroyPropertyBlob(uint32_t id) override;
  bool GetCapability(uint64_t capability, uint64_t* value) const override;
  ScopedDrmPropertyBlobPtr GetPropertyBlob(uint32_t property_id) const override;
  ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                           const char* name) const override;
  bool SetObjectProperty(uint32_t object_id,
                         uint32_t object_type,
                         uint32_t property_id,
                         uint32_t property_value) override;
  bool SetCursor(uint32_t crtc_id,
                 uint32_t handle,
                 const gfx::Size& size) override;
  bool MoveCursor(uint32_t crtc_id, const gfx::Point& point) override;
  bool CommitProperties(drmModeAtomicReq* request,
                        uint32_t flags,
                        uint32_t crtc_count,
                        scoped_refptr<PageFlipRequest> callback) override;
  bool CreateDumbBuffer(const SkImageInfo& info,
                        uint32_t* handle,
                        uint32_t* stride) override;
  bool DestroyDumbBuffer(uint32_t handle) override;
  bool MapDumbBuffer(uint32_t handle, size_t size, void** pixels) override;
  bool UnmapDumbBuffer(void* pixels, size_t size) override;
  bool CloseBufferHandle(uint32_t handle) override;
  bool SetGammaRamp(uint32_t crtc_id,
                    const display::GammaCurve& curve) override;
  bool SetCapability(uint64_t capability, uint64_t value) override;
  std::optional<std::string> GetDriverName() const override;
  void SetDriverName(std::optional<std::string> name);
  uint32_t GetFramebufferForCrtc(uint32_t crtc_id) const;

  // There is a circular reference between DrmDevice and
  // HardwareDisplayPlaneManager, as described in https://crbug.com/40263526.
  // This function can be used to break the cycle in unittests. It must be
  // called after any associated ScreenManager has been destroyed.
  void ResetPlaneManagerForTesting();

  // When CommitProperties has been called with a PageFlipRequest, FakeDrmDevice
  // holds a reference to the PageFlipRequest, resulting in a reference cycle.
  // This reference os released as a part of RunCallbacks. ClearCallbacks breaks
  // the cycle without running the callbacks.
  void ClearCallbacks();

 protected:
  ~FakeDrmDevice() override;

 private:
  // Properties of the plane associated with a fb.
  struct FramebufferProps {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t modifier = 0;
  };

  bool UpdateProperty(uint32_t id,
                      uint64_t value,
                      std::vector<DrmWrapper::Property>* properties,
                      bool add_property_if_needed = false);

  bool UpdateProperty(uint32_t object_id,
                      uint32_t property_id,
                      uint64_t value,
                      bool add_property_if_needed = false);

  bool ValidatePropertyValue(uint32_t id, uint64_t value);

  // Returns true iff SetPossibleValuesForEnumProperty() has been called for
  // `prop_id`.
  bool IsPropertyValueEnum(uint32_t prop_id) const;

  // Fills `property->count_enums` and `property->enums` with the possible enum
  // values for the property (provided to a previous call to
  // SetPossibleValuesForEnumProperty()). It's assumed that `property->id` has
  // the correct property ID.
  void FillPossibleValuesForEnumProperty(drmModePropertyRes* property) const;

  int set_crtc_call_count_ = 0;
  int add_framebuffer_call_count_ = 0;
  int remove_framebuffer_call_count_ = 0;
  int page_flip_call_count_ = 0;
  int overlay_clear_call_count_ = 0;
  int allocate_buffer_count_ = 0;
  int test_modeset_count_ = 0;
  int commit_modeset_count_ = 0;
  int seamless_modeset_count_ = 0;
  int commit_count_ = 0;
  int set_object_property_count_ = 0;
  int set_gamma_ramp_count_ = 0;
  int last_planes_committed_count_ = 0;
  int modeset_sequence_id_ = 0;

  bool set_crtc_expectation_ = true;
  bool add_framebuffer_expectation_ = true;
  bool page_flip_expectation_ = true;
  bool create_dumb_buffer_expectation_ = true;
  bool legacy_gamma_ramp_expectation_ = false;
  bool commit_expectation_ = true;
  bool modeset_with_overlays_expectation_ = true;

  uint32_t current_framebuffer_ = 0;

  std::optional<std::string> driver_name_ = "mock";

  base::flat_map<uint32_t /*handle*/, sk_sp<SkSurface>> buffers_;

  std::map<uint32_t, uint32_t> crtc_cursor_map_;
  std::unordered_map<uint32_t, gfx::Point> crtc_cursor_location_map_;

  std::set<uint32_t> framebuffer_ids_;
  std::map<uint32_t, uint32_t> crtc_fb_;
  std::map<uint64_t, uint64_t> capabilities_;

  base::queue<PageFlipRequest::PageFlipCallback> callbacks_;

  FakeDrmState drm_state_;

  struct BlobState {
    BlobState();
    BlobState(const BlobState&);
    ~BlobState();

    // The reference count for this blob. The blob is retained and released
    // with the lifetime of the ScopedDrmPropertyBlob returned by
    // CreatePropertyBlob. It is also retained when its id is set as a value in
    // UpdateProperty and released when that value is overwritten.
    uint32_t ref_count = 0;
    std::vector<uint8_t> data;
  };
  std::map<uint32_t, BlobState> allocated_blobs_;

  // Retain or release the blob with id `id`. Returns true if the blob was found
  // and retained or released. If no blob exists with `id`, return false and
  // do nothing.
  bool RetainBlob(uint32_t id);
  bool ReleaseBlob(uint32_t id);

  // Props of the plane associated with the generated fb_id.
  base::flat_map<uint32_t /*fb_id*/, FramebufferProps> fb_props_;

  uint64_t system_watermark_limitations_ = std::numeric_limits<uint64_t>::max();
  base::flat_map<uint64_t /*modifier*/, int /*overhead*/> modifiers_overhead_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_H_
