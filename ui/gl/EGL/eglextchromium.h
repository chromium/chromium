// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains Chromium-specific EGL extensions declarations.

#ifndef UI_GL_EGL_EGLEXTCHROMIUM_H_
#define UI_GL_EGL_EGLEXTCHROMIUM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <EGL/eglplatform.h>

/* EGLSyncControlCHROMIUM requires 64-bit uint support */
#if KHRONOS_SUPPORT_INT64
#ifndef EGL_CHROMIUM_sync_control
#define EGL_CHROMIUM_sync_control 1
typedef khronos_uint64_t EGLuint64CHROMIUM;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncValuesCHROMIUM(
    EGLDisplay dpy, EGLSurface surface, EGLuint64CHROMIUM *ust,
    EGLuint64CHROMIUM *msc, EGLuint64CHROMIUM *sbc);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETSYNCVALUESCHROMIUMPROC)
    (EGLDisplay dpy, EGLSurface surface, EGLuint64CHROMIUM *ust,
     EGLuint64CHROMIUM *msc, EGLuint64CHROMIUM *sbc);
#endif /* EGL_CHROMIUM_sync_control */

#ifndef EGL_ANGLE_sync_control_rate
#define EGL_ANGLE_sync_control_rate 1
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglGetMscRateANGLE(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLint* numerator,
                                                 EGLint* denominator);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef EGLBoolean(EGLAPIENTRYP PFNEGLGETMSCRATEANGLEPROC)(EGLDisplay dpy,
                                                           EGLSurface surface,
                                                           EGLint* numerator,
                                                           EGLint* denominator);
#endif /* EGL_ANGLE_sync_control_rate */
#endif /* KHRONOS_SUPPORT_INT64 */

#ifdef __cplusplus
}
#endif

#define  // UI_GL_EGL_EGLEXTCHROMIUM_H_
