// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

class HardwareDisplayPlaneManager;
class GbmDevice;

class DrmDevice : public DrmWrapper,
                  public base::RefCountedThreadSafe<DrmDevice> {
 public:
  using PageFlipCallback =
      base::OnceCallback<void(unsigned int /* frame */,
                              base::TimeTicks /* timestamp */)>;

  DrmDevice(const base::FilePath& device_path,
            base::ScopedFD fd,
            bool is_primary_device,
            std::unique_ptr<GbmDevice> gbm_device);

  DrmDevice(const DrmDevice&) = delete;
  DrmDevice& operator=(const DrmDevice&) = delete;

  // Open device.
  bool Initialize() override;

  bool SetCrtc(uint32_t crtc_id,
               uint32_t framebuffer,
               std::vector<uint32_t> connectors,
               const drmModeModeInfo& mode) override;

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

  // On success, true is returned and |page_flip_request| will receive a
  // callback signalling completion of the flip, if provided.
  virtual bool CommitProperties(
      drmModeAtomicReq* properties,
      uint32_t flags,
      uint32_t crtc_count,
      scoped_refptr<PageFlipRequest> page_flip_request);

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedDictionary dict) const override;

  display::DrmFormatsAndModifiers GetFormatsAndModifiersForCrtc(
      uint32_t crtc_id) const override;

  virtual int modeset_sequence_id() const;
  HardwareDisplayPlaneManager* plane_manager() { return plane_manager_.get(); }
  const HardwareDisplayPlaneManager* plane_manager() const {
    return plane_manager_.get();
  }
  GbmDevice* gbm_device() const { return gbm_.get(); }

 protected:
  friend class base::RefCountedThreadSafe<DrmDevice>;

  ~DrmDevice() override;

  std::unique_ptr<HardwareDisplayPlaneManager> plane_manager_;

 private:
  class IOWatcher;
  class PageFlipManager;

  // Sequence ID incremented at each modeset.
  // Currently used by DRM Framebuffer to indicate when was the fb initialized
  // wrt the preceding modeset.
  int modeset_sequence_id_ = 0;

  std::unique_ptr<PageFlipManager> page_flip_manager_;

  // Watcher for |fd_| listening for page flip events.
  std::unique_ptr<IOWatcher> watcher_;

  const std::unique_ptr<GbmDevice> gbm_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_
