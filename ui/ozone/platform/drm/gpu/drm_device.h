// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/common/linux/gbm_device.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

typedef struct _drmEventContext drmEventContext;
typedef struct _drmModeModeInfo drmModeModeInfo;

struct SkImageInfo;

namespace display {
struct GammaRampRGBEntry;
}

namespace ui {

class HardwareDisplayPlaneManager;
class DrmDevice;

class DrmPropertyBlobMetadata {
 public:
  DrmPropertyBlobMetadata(DrmDevice* drm, uint32_t id);
  ~DrmPropertyBlobMetadata();

  uint32_t id() const { return id_; }

 private:
  DrmDevice* drm_;  // Not owned;
  uint32_t id_;

  DISALLOW_COPY_AND_ASSIGN(DrmPropertyBlobMetadata);
};

using ScopedDrmPropertyBlob = std::unique_ptr<DrmPropertyBlobMetadata>;

// Wraps DRM calls into a nice interface. Used to provide different
// implementations of the DRM calls. For the actual implementation the DRM API
// would be called. In unit tests this interface would be stubbed.
class DrmDevice : public base::RefCountedThreadSafe<DrmDevice> {
 public:
  using PageFlipCallback =
      base::OnceCallback<void(unsigned int /* frame */,
                              base::TimeTicks /* timestamp */)>;

  struct Property {
    // Unique identifier for the property. 0 denotes an invalid ID.
    uint32_t id;

    // Depending on the property, this may be an actual value describing the
    // property or an ID of another property.
    uint64_t value;
  };

  DrmDevice(const base::FilePath& device_path,
            base::File file,
            bool is_primary_device,
            std::unique_ptr<GbmDevice> gbm_device);

  bool is_primary_device() const { return is_primary_device_; }

  bool is_atomic() const { return is_atomic_; }

  bool allow_addfb2_modifiers() const { return allow_addfb2_modifiers_; }

  // Open device.
  virtual bool Initialize();

  // Returns all the DRM resources for this device. This includes CRTC,
  // connectors, and encoders state.
  virtual ScopedDrmResourcesPtr GetResources();

  // Returns the properties associated with object with id |object_id| and type
  // |object_type|. |object_type| is one of DRM_MODE_OBJECT_*.
  virtual ScopedDrmObjectPropertyPtr GetObjectProperties(uint32_t object_id,
                                                         uint32_t object_type);

  // Get the CRTC state. This is generally used to save state before using the
  // CRTC. When the user finishes using the CRTC, the user should restore the
  // CRTC to it's initial state. Use |SetCrtc| to restore the state.
  virtual ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id);

  // Used to configure CRTC with ID |crtc_id| to use the connector in
  // |connectors|. The CRTC will be configured with mode |mode| and will display
  // the framebuffer with ID |framebuffer|. Before being able to display the
  // framebuffer, it should be registered with the CRTC using |AddFramebuffer|.
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       std::vector<uint32_t> connectors,
                       drmModeModeInfo* mode);

  // Used to set a specific configuration to the CRTC. Normally this function
  // would be called with a CRTC saved state (from |GetCrtc|) to restore it to
  // its original configuration.
  virtual bool SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors);

  virtual bool DisableCrtc(uint32_t crtc_id);

  // Returns the connector properties for |connector_id|.
  virtual ScopedDrmConnectorPtr GetConnector(uint32_t connector_id);

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

  // Deregister the given |framebuffer|.
  virtual bool RemoveFramebuffer(uint32_t framebuffer);

  // Get the DRM details associated with |framebuffer|.
  virtual ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer);

  // Schedules a pageflip for CRTC |crtc_id|. This function will return
  // immediately. Upon completion of the pageflip event, the CRTC will be
  // displaying the buffer with ID |framebuffer| and will have a DRM event
  // queued on |fd_|.
  //
  // On success, true is returned and |page_flip_request| will receive a
  // callback signalling completion of the flip.
  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        scoped_refptr<PageFlipRequest> page_flip_request);

  // Returns the list of all planes available on this DRM device.
  virtual ScopedDrmPlaneResPtr GetPlaneResources();

  // Returns the properties associated with plane with id |plane_id|.
  virtual ScopedDrmPlanePtr GetPlane(uint32_t plane_id);

  // Returns the property with name |name| associated with |connector|. Returns
  // NULL if property not found. If the returned value is valid, it must be
  // released using FreeProperty().
  virtual ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                           const char* name);

  virtual ScopedDrmPropertyPtr GetProperty(uint32_t id);

  // Sets the value of property with ID |property_id| to |value|. The property
  // is applied to the connector with ID |connector_id|.
  virtual bool SetProperty(uint32_t connector_id,
                           uint32_t property_id,
                           uint64_t value);

  // Creates a property blob with data |blob| of size |size|.
  virtual ScopedDrmPropertyBlob CreatePropertyBlob(void* blob, size_t size);

  virtual void DestroyPropertyBlob(uint32_t id);

  // Returns a binary blob associated with |property_id|. May be nullptr if the
  // property couldn't be found.
  virtual ScopedDrmPropertyBlobPtr GetPropertyBlob(uint32_t property_id);

  // Returns a binary blob associated with |connector|. The binary blob is
  // associated with the property with name |name|. Return NULL if the property
  // could not be found or if the property does not have a binary blob. If valid
  // the returned object must be freed using FreePropertyBlob().
  virtual ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                                   const char* name);

  // Sets a property (defined by {|property_id|, |property_value|} on an object
  // with ID |object_id| and type |object_type|.
  // |object_id| and |property_id| are unique identifiers.
  // |object_type| is one of DRM_MODE_OBJECT_*.
  virtual bool SetObjectProperty(uint32_t object_id,
                                 uint32_t object_type,
                                 uint32_t property_id,
                                 uint32_t property_value);

  // Can be used to query device/driver |capability|. Sets the value of
  // |capability| to |value|. Returns true in case of a succesful query.
  virtual bool GetCapability(uint64_t capability, uint64_t* value);

  // Set the cursor to be displayed in CRTC |crtc_id|. (width, height) is the
  // cursor size pointed by |handle|.
  virtual bool SetCursor(uint32_t crtc_id,
                         uint32_t handle,
                         const gfx::Size& size);

  // Move the cursor on CRTC |crtc_id| to (x, y);
  virtual bool MoveCursor(uint32_t crtc_id, const gfx::Point& point);

  virtual bool CreateDumbBuffer(const SkImageInfo& info,
                                uint32_t* handle,
                                uint32_t* stride);

  virtual bool DestroyDumbBuffer(uint32_t handle);

  virtual bool MapDumbBuffer(uint32_t handle, size_t size, void** pixels);

  virtual bool UnmapDumbBuffer(void* pixels, size_t size);

  virtual bool CloseBufferHandle(uint32_t handle);

  // On success, true is returned and |page_flip_request| will receive a
  // callback signalling completion of the flip, if provided.
  virtual bool CommitProperties(
      drmModeAtomicReq* properties,
      uint32_t flags,
      uint32_t crtc_count,
      scoped_refptr<PageFlipRequest> page_flip_request);

  virtual bool SetCapability(uint64_t capability, uint64_t value);

  virtual bool SetGammaRamp(uint32_t crtc_id,
                            const std::vector<display::GammaRampRGBEntry>& lut);

  // Drm master related
  virtual bool SetMaster();
  virtual bool DropMaster();

  int get_fd() const { return file_.GetPlatformFile(); }

  base::FilePath device_path() const { return device_path_; }

  HardwareDisplayPlaneManager* plane_manager() { return plane_manager_.get(); }

  GbmDevice* gbm_device() const { return gbm_.get(); }

 protected:
  friend class base::RefCountedThreadSafe<DrmDevice>;

  virtual ~DrmDevice();

  std::unique_ptr<HardwareDisplayPlaneManager> plane_manager_;

 private:
  class IOWatcher;
  class PageFlipManager;

  // Path to the DRM device (in sysfs).
  const base::FilePath device_path_;

  // DRM device.
  const base::File file_;

  std::unique_ptr<PageFlipManager> page_flip_manager_;

  // Watcher for |fd_| listening for page flip events.
  std::unique_ptr<IOWatcher> watcher_;

  const bool is_primary_device_;

  bool is_atomic_ = false;

  bool allow_addfb2_modifiers_ = false;

  const std::unique_ptr<GbmDevice> gbm_;

  DISALLOW_COPY_AND_ASSIGN(DrmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_
