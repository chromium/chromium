// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_SCOPED_DRM_TYPES_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_SCOPED_DRM_TYPES_H_

#include <memory>

#include "base/memory/free_deleter.h"

typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeEncoder drmModeEncoder;
typedef struct _drmModeFB drmModeFB;
typedef struct _drmModeObjectProperties drmModeObjectProperties;
typedef struct _drmModePlane drmModePlane;
typedef struct _drmModePlaneRes drmModePlaneRes;
typedef struct _drmModeProperty drmModePropertyRes;
typedef struct _drmModeAtomicReq drmModeAtomicReq;
typedef struct _drmModePropertyBlob drmModePropertyBlobRes;
typedef struct _drmModeRes drmModeRes;
typedef struct _drmVersion drmVersion;
typedef struct drm_color_lut drm_color_lut;
typedef struct drm_color_ctm drm_color_ctm;

namespace ui {

struct DrmResourcesDeleter {
  void operator()(drmModeRes* resources) const;
};
struct DrmConnectorDeleter {
  void operator()(drmModeConnector* connector) const;
};
struct DrmCrtcDeleter {
  void operator()(drmModeCrtc* crtc) const;
};
struct DrmEncoderDeleter {
  void operator()(drmModeEncoder* encoder) const;
};
struct DrmObjectPropertiesDeleter {
  void operator()(drmModeObjectProperties* properties) const;
};
struct DrmPlaneDeleter {
  void operator()(drmModePlane* plane) const;
};
struct DrmPlaneResDeleter {
  void operator()(drmModePlaneRes* plane_res) const;
};
struct DrmPropertyDeleter {
  void operator()(drmModePropertyRes* property) const;
};
struct DrmAtomicReqDeleter {
  void operator()(drmModeAtomicReq* property) const;
};
struct DrmPropertyBlobDeleter {
  void operator()(drmModePropertyBlobRes* property) const;
};
struct DrmFramebufferDeleter {
  void operator()(drmModeFB* framebuffer) const;
};
struct DrmVersionDeleter {
  void operator()(drmVersion* version) const;
};

typedef std::unique_ptr<drmModeRes, DrmResourcesDeleter> ScopedDrmResourcesPtr;
typedef std::unique_ptr<drmModeConnector, DrmConnectorDeleter>
    ScopedDrmConnectorPtr;
typedef std::unique_ptr<drmModeCrtc, DrmCrtcDeleter> ScopedDrmCrtcPtr;
typedef std::unique_ptr<drmModeEncoder, DrmEncoderDeleter> ScopedDrmEncoderPtr;
typedef std::unique_ptr<drmModeObjectProperties, DrmObjectPropertiesDeleter>
    ScopedDrmObjectPropertyPtr;
typedef std::unique_ptr<drmModePlane, DrmPlaneDeleter> ScopedDrmPlanePtr;
typedef std::unique_ptr<drmModePlaneRes, DrmPlaneResDeleter>
    ScopedDrmPlaneResPtr;
typedef std::unique_ptr<drmModePropertyRes, DrmPropertyDeleter>
    ScopedDrmPropertyPtr;
typedef std::unique_ptr<drmModeAtomicReq, DrmAtomicReqDeleter>
    ScopedDrmAtomicReqPtr;
typedef std::unique_ptr<drmModePropertyBlobRes, DrmPropertyBlobDeleter>
    ScopedDrmPropertyBlobPtr;
typedef std::unique_ptr<drmModeFB, DrmFramebufferDeleter>
    ScopedDrmFramebufferPtr;
typedef std::unique_ptr<drmVersion, DrmVersionDeleter> ScopedDrmVersionPtr;

typedef std::unique_ptr<drm_color_lut, base::FreeDeleter> ScopedDrmColorLutPtr;
typedef std::unique_ptr<drm_color_ctm, base::FreeDeleter> ScopedDrmColorCtmPtr;

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_SCOPED_DRM_TYPES_H_
