// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_WRAPPER_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

struct SkImageInfo;

namespace display {
class GammaCurve;
}  // namespace display

namespace ui {

class DrmWrapper;

class DrmPropertyBlobMetadata {
 public:
  DrmPropertyBlobMetadata(DrmWrapper* drm, uint32_t id);

  DrmPropertyBlobMetadata(const DrmPropertyBlobMetadata&) = delete;
  DrmPropertyBlobMetadata& operator=(const DrmPropertyBlobMetadata&) = delete;

  ~DrmPropertyBlobMetadata();

  uint32_t id() const { return id_; }

 private:
  raw_ptr<DrmWrapper> drm_;  // Not owned;
  uint32_t id_;
};

using ScopedDrmPropertyBlob = std::unique_ptr<DrmPropertyBlobMetadata>;

// Wraps DRM calls into a tight interface. Used to provide different
// implementations of the DRM calls. For the actual implementation the DRM API
// would be called. In unit tests this interface would be stubbed.
class DrmWrapper {
 public:
  DrmWrapper(const base::FilePath& device_path,
             base::ScopedFD fd,
             bool is_primary_device);
  DrmWrapper(const DrmWrapper&) = delete;
  DrmWrapper& operator=(const DrmWrapper&) = delete;
  virtual ~DrmWrapper();

  struct Property {
    // Unique identifier for the property. 0 denotes an invalid ID.
    uint32_t id = 0;

    // Depending on the property, this may be an actual value describing the
    // property or an ID of another property.
    uint64_t value = 0;
  };

  // Open device.
  virtual bool Initialize();

  /*******
   * CRTCs
   *******/

  // Get the CRTC state. This is generally used to save state before using the
  // CRTC. When the user finishes using the CRTC, the user should restore the
  // CRTC to it's initial state. Use |SetCrtc| to restore the state.
  virtual ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id) const;

  // Used to configure CRTC with ID |crtc_id| to use the connector in
  // |connectors|. The CRTC will be configured with mode |mode| and will display
  // the framebuffer with ID |framebuffer|. Before being able to display the
  // framebuffer, it should be registered with the CRTC using |AddFramebuffer|.
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       std::vector<uint32_t> connectors,
                       const drmModeModeInfo& mode);

  virtual bool DisableCrtc(uint32_t crtc_id);

  /**************
   * Capabilities
   **************/

  // Queries whether a |capability| is available and stores its value in
  // |value| if found.
  virtual bool GetCapability(uint64_t capability, uint64_t* value) const;

  // Can be used to query device/driver |capability|. Sets the value of
  // |capability| to |value|. Returns true in case of a successful query.
  virtual bool SetCapability(uint64_t capability, uint64_t value);

  /************
   * Connectors
   ************/

  // Returns the connector properties for |connector_id|.
  virtual ScopedDrmConnectorPtr GetConnector(uint32_t connector_id) const;

  /********
   * Cursor
   ********/

  // Move the cursor on CRTC |crtc_id| to (x, y);
  virtual bool MoveCursor(uint32_t crtc_id, const gfx::Point& point);

  // Set the cursor to be displayed in CRTC |crtc_id|. (width, height) is the
  // cursor size pointed by |handle|.
  virtual bool SetCursor(uint32_t crtc_id,
                         uint32_t handle,
                         const gfx::Size& size);

  /************
   * DRM Master
   ************/

  virtual bool SetMaster();
  virtual bool DropMaster();

  /**************
   * Dumb Buffers
   **************/

  virtual bool CreateDumbBuffer(const SkImageInfo& info,
                                uint32_t* handle,
                                uint32_t* stride);
  virtual bool DestroyDumbBuffer(uint32_t handle);
  virtual bool MapDumbBuffer(uint32_t handle, size_t size, void** pixels);
  virtual bool UnmapDumbBuffer(void* pixels, size_t size);

  virtual bool CloseBufferHandle(uint32_t handle);

  /**********
   * Encoders
   **********/

  // Returns the encoder properties for |encoder_id|.
  virtual ScopedDrmEncoderPtr GetEncoder(uint32_t encoder_id) const;

  /**************
   * Framebuffers
   **************/

  // Get the DRM details associated with |framebuffer|.
  virtual ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer) const;

  // Deregister the given |framebuffer|.
  virtual bool RemoveFramebuffer(uint32_t framebuffer);

  // Register any format buffer with the CRTC. On successful registration, the
  // CRTC will assign a framebuffer ID to |framebuffer|.
  virtual bool AddFramebuffer2(uint32_t width,
                               uint32_t height,
                               uint32_t format,
                               uint32_t handles[4],
                               uint32_t strides[4],
                               uint32_t offsets[4],
                               uint64_t modifiers[4],
                               uint32_t* framebuffer,
                               uint32_t flags);

  /*******
   * Gamma
   *******/

  virtual bool SetGammaRamp(uint32_t crtc_id, const display::GammaCurve& lut);

  /********
   * Planes
   ********/

  // Returns the list of all planes available on this DRM device.
  virtual ScopedDrmPlaneResPtr GetPlaneResources() const;

  // Returns the properties associated with plane with id |plane_id|.
  virtual ScopedDrmPlanePtr GetPlane(uint32_t plane_id) const;

  /************
   * Properties
   ************/

  // Returns the properties associated with object with id |object_id| and type
  // |object_type|. |object_type| is one of DRM_MODE_OBJECT_*.
  virtual ScopedDrmObjectPropertyPtr GetObjectProperties(
      uint32_t object_id,
      uint32_t object_type) const;

  // Sets a property (defined by {|property_id|, |property_value|} on an object
  // with ID |object_id| and type |object_type|.
  // |object_id| and |property_id| are unique identifiers.
  // |object_type| is one of DRM_MODE_OBJECT_*.
  virtual bool SetObjectProperty(uint32_t object_id,
                                 uint32_t object_type,
                                 uint32_t property_id,
                                 uint32_t property_value);

  virtual ScopedDrmPropertyPtr GetProperty(uint32_t id) const;

  // Returns the property with name |name| associated with |connector|. Returns
  // NULL if property not found. If the returned value is valid, it must be
  // released using FreeProperty().
  virtual ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                           const char* name) const;

  // Sets the value of property with ID |property_id| to |value|. The property
  // is applied to the connector with ID |connector_id|.
  virtual bool SetProperty(uint32_t connector_id,
                           uint32_t property_id,
                           uint64_t value);

  /****************
   * Property Blobs
   ****************/

  // Creates a property blob with data |blob| of size |size|.
  virtual ScopedDrmPropertyBlob CreatePropertyBlob(const void* blob,
                                                   size_t size);
  // Creates a property blob with |size| for data |blob| which user space
  // can't read back.
  virtual ScopedDrmPropertyBlob CreatePropertyBlobWithFlags(const void* blob,
                                                            size_t size,
                                                            uint32_t flags);
  virtual void DestroyPropertyBlob(uint32_t id);

  // Returns a binary blob associated with |property_id|. May be nullptr if the
  // property couldn't be found.
  virtual ScopedDrmPropertyBlobPtr GetPropertyBlob(uint32_t property_id) const;

  // Returns a binary blob associated with |connector|. The binary blob is
  // associated with the property with name |name|. Return NULL if the property
  // could not be found or if the property does not have a binary blob. If valid
  // the returned object must be freed using FreePropertyBlob().
  virtual ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                                   const char* name) const;

  /***********
   * Resources
   ***********/

  // Returns all the DRM resources for this device. This includes CRTC,
  // connectors, and encoders state.
  virtual ScopedDrmResourcesPtr GetResources() const;

  /*********
   * Utility
   *********/

  // Adds trace records to |context|.
  virtual void WriteIntoTrace(perfetto::TracedDictionary dict) const;

  virtual std::optional<std::string> GetDriverName() const;

  // TODO(gildekel): remove once DrmWrapper and DrmDevice are completely
  // decoupled.
  // Returns a list of supported drm formats and modifiers for |crtc_id|. Note:
  // implementation in wrapper is a stub. Full implementation is in
  // DrmDevice::GetFormatsAndModifiersForCrtc().
  virtual display::DrmFormatsAndModifiers GetFormatsAndModifiersForCrtc(
      uint32_t crtc_id) const;

  // Extracts the FD from the given |drm|. The |drm| object will be invalidated.
  static base::ScopedFD ToScopedFD(std::unique_ptr<DrmWrapper> drm);

  base::FilePath device_path() const { return device_path_; }
  bool is_atomic() const { return is_atomic_; }
  bool is_primary_device() const { return is_primary_device_; }

 protected:
  // TODO(gildekel): move CommitProperties() and PageFlip() to the public API
  // once DrmWrapper and DrmDevice are completely decoupled. Consider changing
  // the signature to `void* user_data` instead of |page_flip_id|, which is too
  // specific.
  bool CommitProperties(drmModeAtomicReq* properties,
                        uint32_t flags,
                        uint64_t page_flip_id);

  bool PageFlip(uint32_t crtc_id, uint32_t framebuffer, uint64_t page_flip_id);

  const int& GetFd() const { return drm_fd_.get(); }

 private:
  // Path to the DRM device (in sysfs).
  const base::FilePath device_path_;

  // DRM device FD.
  base::ScopedFD drm_fd_;

  // Whether or not DRM was successfully set to atomic during the initialization
  // of this DRM device.
  bool is_atomic_ = false;

  const bool is_primary_device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_WRAPPER_H_
