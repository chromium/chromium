// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_wgl.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_wgl_api_implementation.h"

namespace gl {

namespace {
const PIXELFORMATDESCRIPTOR kPixelFormatDescriptor = {
  sizeof(kPixelFormatDescriptor),    // Size of structure.
  1,                       // Default version.
  PFD_DRAW_TO_WINDOW |     // Window drawing support.
  PFD_SUPPORT_OPENGL |     // OpenGL support.
  PFD_DOUBLEBUFFER,        // Double buffering support (not stereo).
  PFD_TYPE_RGBA,           // RGBA color mode (not indexed).
  24,                      // 24 bit color mode.
  0, 0, 0, 0, 0, 0,        // Don't set RGB bits & shifts.
  8, 0,                    // 8 bit alpha
  0,                       // No accumulation buffer.
  0, 0, 0, 0,              // Ignore accumulation bits.
  0,                       // no z-buffer.
  0,                       // no stencil buffer.
  0,                       // No aux buffer.
  PFD_MAIN_PLANE,          // Main drawing plane (not overlay).
  0,                       // Reserved.
  0, 0, 0,                 // Layer masks ignored.
};

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  switch (message) {
    case WM_ERASEBKGND:
      // Prevent windows from erasing the background.
      return 1;
    case WM_PAINT:
      // Do not paint anything.
      PAINTSTRUCT paint;
      if (BeginPaint(window, &paint))
        EndPaint(window, &paint);
      return 0;
    default:
      return DefWindowProc(window, message, w_param, l_param);
  }
}

class DisplayWGL {
 public:
  DisplayWGL()
      : module_handle_(0),
        window_class_(0),
        window_handle_(0),
        device_context_(0),
        pixel_format_(0) {
  }

  ~DisplayWGL() {
    if (window_handle_)
      DestroyWindow(window_handle_);
    if (window_class_)
      UnregisterClass(reinterpret_cast<wchar_t*>(window_class_),
                      module_handle_);
  }

  bool Init() {
    // We must initialize a GL context before we can bind to extension entry
    // points. This requires the device context for a window.
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<wchar_t*>(IntermediateWindowProc),
                           &module_handle_)) {
      LOG(ERROR) << "GetModuleHandleEx failed.";
      return false;
    }

    WNDCLASS intermediate_class;
    intermediate_class.style = CS_OWNDC;
    intermediate_class.lpfnWndProc = IntermediateWindowProc;
    intermediate_class.cbClsExtra = 0;
    intermediate_class.cbWndExtra = 0;
    intermediate_class.hInstance = module_handle_;
    intermediate_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    intermediate_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    intermediate_class.hbrBackground = NULL;
    intermediate_class.lpszMenuName = NULL;
    intermediate_class.lpszClassName = L"Intermediate GL Window";
    window_class_ = RegisterClass(&intermediate_class);
    if (!window_class_) {
      LOG(ERROR) << "RegisterClass failed.";
      return false;
    }

    window_handle_ = CreateWindowEx(WS_EX_NOPARENTNOTIFY,
                                    reinterpret_cast<wchar_t*>(window_class_),
                                    L"",
                                    WS_OVERLAPPEDWINDOW,
                                    0,
                                    0,
                                    100,
                                    100,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    if (!window_handle_) {
      LOG(ERROR) << "CreateWindow failed.";
      return false;
    }

    device_context_ = GetDC(window_handle_);
    pixel_format_ = ChoosePixelFormat(device_context_,
                                      &kPixelFormatDescriptor);
    if (pixel_format_ == 0) {
      LOG(ERROR) << "Unable to get the pixel format for GL context.";
      return false;
    }
    if (!SetPixelFormat(device_context_,
                        pixel_format_,
                        &kPixelFormatDescriptor)) {
      LOG(ERROR) << "Unable to set the pixel format for temporary GL context.";
      return false;
    }

    return true;
  }

  ATOM window_class() const { return window_class_; }
  HDC device_context() const { return device_context_; }
  int pixel_format() const { return pixel_format_; }

 private:
  HINSTANCE module_handle_;
  ATOM window_class_;
  HWND window_handle_;
  HDC device_context_;
  int pixel_format_;
};
DisplayWGL* g_wgl_display;
}  // namespace

// static
bool GLSurfaceWGL::initialized_ = false;

GLSurfaceWGL::GLSurfaceWGL() {
}

GLSurfaceWGL::~GLSurfaceWGL() {
}

void* GLSurfaceWGL::GetDisplay() {
  return GetDisplayDC();
}

// static
bool GLSurfaceWGL::InitializeOneOff() {
  if (initialized_)
    return true;

  DCHECK(g_wgl_display == NULL);
  std::unique_ptr<DisplayWGL> wgl_display(new DisplayWGL);
  if (!wgl_display->Init())
    return false;

  g_wgl_display = wgl_display.release();
  initialized_ = true;
  return true;
}

// static
bool GLSurfaceWGL::InitializeExtensionSettingsOneOff() {
  if (!initialized_)
    return false;
  g_driver_wgl.InitializeExtensionBindings();
  return true;
}

void GLSurfaceWGL::InitializeOneOffForTesting() {
  if (g_wgl_display == NULL) {
    g_wgl_display = new DisplayWGL;
  }
}

HDC GLSurfaceWGL::GetDisplayDC() {
  return g_wgl_display->device_context();
}

NativeViewGLSurfaceWGL::NativeViewGLSurfaceWGL(gfx::AcceleratedWidget window)
    : window_(window), child_window_(NULL), device_context_(NULL) {
  DCHECK(window);
}

NativeViewGLSurfaceWGL::~NativeViewGLSurfaceWGL() {
  Destroy();
}

bool NativeViewGLSurfaceWGL::Initialize(GLSurfaceFormat format) {
  DCHECK(!device_context_);

  RECT rect;
  if (!GetClientRect(window_, &rect)) {
    LOG(ERROR) << "GetClientRect failed.\n";
    Destroy();
    return false;
  }

  // Create a child window. WGL has problems using a window handle owned by
  // another process.
  child_window_ = CreateWindowEx(
      WS_EX_NOPARENTNOTIFY,
      reinterpret_cast<wchar_t*>(g_wgl_display->window_class()), L"",
      WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE, 0, 0, rect.right - rect.left,
      rect.bottom - rect.top, window_, NULL, NULL, NULL);
  if (!child_window_) {
    LOG(ERROR) << "CreateWindow failed.\n";
    Destroy();
    return false;
  }

  // The GL context will render to this window.
  device_context_ = GetDC(child_window_);
  if (!device_context_) {
    LOG(ERROR) << "Unable to get device context for window.";
    Destroy();
    return false;
  }

  if (!SetPixelFormat(device_context_, g_wgl_display->pixel_format(),
                      &kPixelFormatDescriptor)) {
    LOG(ERROR) << "Unable to set the pixel format for GL context.";
    Destroy();
    return false;
  }

  format_ = format;

  return true;
}

void NativeViewGLSurfaceWGL::Destroy() {
  if (child_window_ && device_context_)
    ReleaseDC(child_window_, device_context_);

  if (child_window_)
    DestroyWindow(child_window_);

  child_window_ = NULL;
  device_context_ = NULL;
}

bool NativeViewGLSurfaceWGL::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    ColorSpace color_space,
                                    bool has_alpha) {
  RECT rect;
  if (!GetClientRect(window_, &rect)) {
    LOG(ERROR) << "Failed to get parent window size.";
    return false;
  }
  DCHECK(size.width() == (rect.right - rect.left) &&
         size.height() == (rect.bottom - rect.top));
  if (!MoveWindow(child_window_, 0, 0, size.width(), size.height(), FALSE)) {
    LOG(ERROR) << "Failed to resize child window.";
    return false;
  }
  return true;
}

bool NativeViewGLSurfaceWGL::Recreate() {
  Destroy();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to create surface.";
    return false;
  }
  return true;
}

bool NativeViewGLSurfaceWGL::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceWGL::SwapBuffers(
    PresentationCallback callback) {
  // TODO(penghuang): Provide presentation feedback. https://crbug.com/776877
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceWGL:RealSwapBuffers",
      "width", GetSize().width(),
      "height", GetSize().height());

  // Resize the child window to match the parent before swapping. Do not repaint
  // it as it moves.
  RECT rect;
  if (!GetClientRect(window_, &rect))
    return gfx::SwapResult::SWAP_FAILED;
  if (!MoveWindow(child_window_,
                  0,
                  0,
                  rect.right - rect.left,
                  rect.bottom - rect.top,
                  FALSE)) {
    return gfx::SwapResult::SWAP_FAILED;
  }

  DCHECK(device_context_);
  if (::SwapBuffers(device_context_) == TRUE) {
    // TODO(penghuang): Provide more accurate values for presentation feedback.
    constexpr int64_t kRefreshIntervalInMicroseconds =
        base::Time::kMicrosecondsPerSecond / 60;
    std::move(callback).Run(gfx::PresentationFeedback(
        base::TimeTicks::Now(),
        base::TimeDelta::FromMicroseconds(kRefreshIntervalInMicroseconds),
        0 /* flags */));
    return gfx::SwapResult::SWAP_ACK;
  } else {
    std::move(callback).Run(gfx::PresentationFeedback::Failure());
    return gfx::SwapResult::SWAP_FAILED;
  }
}

gfx::Size NativeViewGLSurfaceWGL::GetSize() {
  RECT rect;
  BOOL result = GetClientRect(child_window_, &rect);
  DCHECK(result);
  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
}

void* NativeViewGLSurfaceWGL::GetHandle() {
  return device_context_;
}

GLSurfaceFormat NativeViewGLSurfaceWGL::GetFormat() {
  return GLSurfaceFormat();
}

void NativeViewGLSurfaceWGL::SetVSyncEnabled(bool enabled) {
  DCHECK(GLContext::GetCurrent() && GLContext::GetCurrent()->IsCurrent(this));
  if (g_driver_wgl.ext.b_WGL_EXT_swap_control) {
    wglSwapIntervalEXT(enabled ? 1 : 0);
  } else {
    LOG(WARNING) << "Could not disable vsync: driver does not "
                    "support WGL_EXT_swap_control";
  }
}

PbufferGLSurfaceWGL::PbufferGLSurfaceWGL(const gfx::Size& size)
    : size_(size),
      device_context_(NULL),
      pbuffer_(NULL) {
  // Some implementations of Pbuffer do not support having a 0 size. For such
  // cases use a (1, 1) surface.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

PbufferGLSurfaceWGL::~PbufferGLSurfaceWGL() {
  Destroy();
}

bool PbufferGLSurfaceWGL::Initialize(GLSurfaceFormat format) {
  DCHECK(!device_context_);

  if (!g_driver_wgl.fn.wglCreatePbufferARBFn) {
    LOG(ERROR) << "wglCreatePbufferARB not available.";
    Destroy();
    return false;
  }

  const int kNoAttributes[] = { 0 };
  pbuffer_ = wglCreatePbufferARB(g_wgl_display->device_context(),
                                 g_wgl_display->pixel_format(), size_.width(),
                                 size_.height(), kNoAttributes);

  if (!pbuffer_) {
    LOG(ERROR) << "Unable to create pbuffer.";
    Destroy();
    return false;
  }

  device_context_ = wglGetPbufferDCARB(static_cast<HPBUFFERARB>(pbuffer_));
  if (!device_context_) {
    LOG(ERROR) << "Unable to get pbuffer device context.";
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLSurfaceWGL::Destroy() {
  if (pbuffer_ && device_context_)
    wglReleasePbufferDCARB(static_cast<HPBUFFERARB>(pbuffer_), device_context_);

  device_context_ = NULL;

  if (pbuffer_) {
    wglDestroyPbufferARB(static_cast<HPBUFFERARB>(pbuffer_));
    pbuffer_ = NULL;
  }
}

bool PbufferGLSurfaceWGL::IsOffscreen() {
  return true;
}

gfx::SwapResult PbufferGLSurfaceWGL::SwapBuffers(
    PresentationCallback callback) {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size PbufferGLSurfaceWGL::GetSize() {
  return size_;
}

void* PbufferGLSurfaceWGL::GetHandle() {
  return device_context_;
}

GLSurfaceFormat PbufferGLSurfaceWGL::GetFormat() {
  return GLSurfaceFormat();
}

}  // namespace gl
