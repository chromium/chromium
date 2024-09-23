// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display.h"

#import <Metal/Metal.h>

#include "base/apple/scoped_nsobject.h"
#include "ui/gl/gl_bindings.h"

// From ANGLE's egl/eglext_angle.h.
#ifndef EGL_ANGLE_metal_shared_event_sync
#define EGL_ANGLE_metal_hared_event_sync 1
#define EGL_SYNC_METAL_SHARED_EVENT_ANGLE 0x34D8
#define EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE 0x34D9
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE 0x34DA
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE 0x34DB
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE 0x34DC
#endif

namespace gl {

struct GLDisplayEGL::ObjCStorage {
  base::apple::scoped_nsprotocol<id<MTLSharedEvent>> metal_shared_event;
  uint64_t metal_signaled_value = 0;
};

// Because on Apple platforms there is a member variable of a type (ObjCStorage)
// that is defined in this file, the constructor/destructor also have to be
// here. If making changes to this copy, be sure to adjust the other copy in
// gl_display.cc.
GLDisplayEGL::GLDisplayEGL(uint64_t system_device_id, DisplayKey display_key)
    : GLDisplay(system_device_id, display_key, EGL) {
  ext = std::make_unique<DisplayExtensionsEGL>();
}

GLDisplayEGL::~GLDisplayEGL() = default;

bool GLDisplayEGL::CreateMetalSharedEvent(id<MTLSharedEvent>* shared_event_out,
                                          uint64_t* signal_value_out) {
  CHECK(ext->b_EGL_ANGLE_metal_shared_event_sync);
  if (!objc_storage_->metal_shared_event) {
    std::vector<EGLAttrib> attribs;
    attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE);
    attribs.push_back(0);
    attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE);
    attribs.push_back(0);
    attribs.push_back(EGL_NONE);
    EGLSync sync =
        eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, &attribs[0]);
    if (!sync)
      return false;

    // eglCopyMetalSharedEventANGLE returns an MTLSharedEvent object that has a
    // retain count of 1.
    objc_storage_->metal_shared_event.reset(static_cast<id<MTLSharedEvent>>(
        eglCopyMetalSharedEventANGLE(display_, sync)));

    // The sync object is already enqueued for signaling in ANGLE's command
    // stream. Since the MTLSharedEvent is already retained, it's safe to delete
    // the sync object immediately.
    eglDestroySync(display_, sync);
  }

  // Create another sync object, perhaps redundantly the first time,
  // but with our specified signal value.
  ++objc_storage_->metal_signaled_value;
  id<MTLSharedEvent> shared_event = objc_storage_->metal_shared_event.get();
  std::vector<EGLAttrib> attribs;
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE);
  attribs.push_back(
      static_cast<EGLAttrib>(reinterpret_cast<uintptr_t>(shared_event)));
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE);
  attribs.push_back(objc_storage_->metal_signaled_value & 0xFFFFFFFF);
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE);
  attribs.push_back((objc_storage_->metal_signaled_value >> 32) & 0xFFFFFFFF);
  attribs.push_back(EGL_NONE);

  EGLSync sync =
      eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, &attribs[0]);
  if (!sync)
    return false;

  // The sync object is already enqueued for signaling in ANGLE's command
  // stream. Since the MTLSharedEvent is already retained, it's safe to delete
  // the sync object immediately.
  eglDestroySync(display_, sync);

  *shared_event_out = objc_storage_->metal_shared_event.get();
  *signal_value_out = objc_storage_->metal_signaled_value;
  return true;
}

void GLDisplayEGL::WaitForMetalSharedEvent(id<MTLSharedEvent> shared_event,
                                           uint64_t signal_value) {
  CHECK(objc_storage_);
  if (objc_storage_->metal_shared_event.get() == shared_event) {
    // If the event is owned by this display, skip the wait.
    // Currently ANGLE/Metal is only single threaded. There is no need to issue
    // a GPU wait for an event that was signaled by the same display. Because
    // the works before and after the signal belong to the same metal queue
    // hence they are already synchronized with each other implicitly.
    CHECK_GE(objc_storage_->metal_signaled_value, signal_value);
    return;
  }

  CHECK(ext->b_EGL_ANGLE_metal_shared_event_sync);
  EGLAttrib attribs[] = {
      // Pass the Metal shared event as an EGLAttrib.
      EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE,
      static_cast<EGLAttrib>(reinterpret_cast<uintptr_t>(shared_event)),
      // EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE is important as it requests
      // ANGLE to create an EGL sync object from the Metal shared event, but NOT
      // signal it to the specified value. The shared event is imported with
      // that signal value. The next call to eglWaitSync enqueue's a GPU wait to
      // wait for that value to be signaled by another command buffer.
      EGL_SYNC_CONDITION,
      EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE,
      // Encode the signaled value in two EGLAttribs.
      EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE,
      EGLAttrib(signal_value & 0xFFFFFFFF),
      EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE,
      EGLAttrib((signal_value >> 32) & 0xFFFFFFFF),
      EGL_NONE,
  };

  EGLSync sync =
      eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, attribs);
  EGLBoolean res = eglWaitSync(display_, sync, 0);
  DCHECK(res == EGL_TRUE);
  // The wait on the sync object has been enqueued already, so it's safe to
  // destroy it now.
  eglDestroySync(display_, sync);
}

void GLDisplayEGL::InitMetalSharedEventStorage() {
  objc_storage_ = std::make_unique<ObjCStorage>();
}

void GLDisplayEGL::CleanupMetalSharedEventStorage() {
  objc_storage_.reset();
}

}  // namespace gl
