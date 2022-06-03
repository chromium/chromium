// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

#include <stdint.h>  // required by xf86drmMode.h
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace ui {

void DrmResourcesDeleter::operator()(drmModeRes* resources) const {
  drmModeFreeResources(resources);
}

void DrmConnectorDeleter::operator()(drmModeConnector* connector) const {
  drmModeFreeConnector(connector);
}

void DrmCrtcDeleter::operator()(drmModeCrtc* crtc) const {
  drmModeFreeCrtc(crtc);
}

void DrmEncoderDeleter::operator()(drmModeEncoder* encoder) const {
  drmModeFreeEncoder(encoder);
}

void DrmObjectPropertiesDeleter::operator()(
    drmModeObjectProperties* properties) const {
  drmModeFreeObjectProperties(properties);
}

void DrmPlaneDeleter::operator()(drmModePlane* plane) const {
  drmModeFreePlane(plane);
}

void DrmPlaneResDeleter::operator()(drmModePlaneRes* plane) const {
  drmModeFreePlaneResources(plane);
}

void DrmPropertyDeleter::operator()(drmModePropertyRes* property) const {
  drmModeFreeProperty(property);
}

void DrmAtomicReqDeleter::operator()(drmModeAtomicReq* property) const {
  drmModeAtomicFree(property);
}

void DrmPropertyBlobDeleter::operator()(
    drmModePropertyBlobRes* property) const {
  drmModeFreePropertyBlob(property);
}

void DrmFramebufferDeleter::operator()(drmModeFB* framebuffer) const {
  drmModeFreeFB(framebuffer);
}

void DrmVersionDeleter::operator()(drmVersion* version) const {
  drmFreeVersion(version);
}

}  // namespace ui
