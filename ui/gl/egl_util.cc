// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/egl_util.h"

#include "build/build_config.h"

namespace ui {

namespace {
const char* GetDebugMessageTypeString(EGLint source) {
  switch (source) {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
      return "Critical";
    case EGL_DEBUG_MSG_ERROR_KHR:
      return "Error";
    case EGL_DEBUG_MSG_WARN_KHR:
      return "Warning";
    case EGL_DEBUG_MSG_INFO_KHR:
      return "Info";
    default:
      return "UNKNOWN";
  }
}
}  // namespace

const char* GetEGLErrorString(uint32_t error) {
  switch (error) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
      return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_CONTEXT_LOST:
      return "EGL_CONTEXT_LOST";
    default:
      return "UNKNOWN";
  }
}

// Returns the last EGL error as a string.
const char* GetLastEGLErrorString() {
  return GetEGLErrorString(eglGetError());
}

void EGLAPIENTRY LogEGLDebugMessage(EGLenum error,
                                    const char* command,
                                    EGLint message_type,
                                    EGLLabelKHR thread_label,
                                    EGLLabelKHR object_label,
                                    const char* message) {
  std::string formatted_message = std::string("EGL Driver message (") +
                                  GetDebugMessageTypeString(message_type) +
                                  ") " + command + ": " + message;

  // Assume that all labels that have been set are strings
  if (thread_label) {
    formatted_message += " thread: ";
    formatted_message += static_cast<const char*>(thread_label);
  }
  if (object_label) {
    formatted_message += " object: ";
    formatted_message += static_cast<const char*>(object_label);
  }

  if (message_type == EGL_DEBUG_MSG_CRITICAL_KHR ||
      message_type == EGL_DEBUG_MSG_ERROR_KHR) {
    LOG(ERROR) << formatted_message;
  } else {
    DVLOG(1) << formatted_message;
  }
}
}  // namespace ui
