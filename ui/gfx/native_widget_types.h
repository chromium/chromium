// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_WIDGET_TYPES_H_
#define UI_GFX_NATIVE_WIDGET_TYPES_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/gfx_export.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/owned_objc.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <string>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

// This file provides cross platform typedefs for native widget types.
//   NativeWindow: this is a handle to a native, top-level window
//   NativeView: this is a handle to a native UI element. It may be the
//     same type as a NativeWindow on some platforms.
//   NativeViewId: Often, in our cross process model, we need to pass around a
//     reference to a "window". This reference will, say, be echoed back from a
//     renderer to the browser when it wishes to query its size. On Windows we
//     use an HWND for this.
//
//     As a rule of thumb - if you're in the renderer, you should be dealing
//     with NativeViewIds. This should remind you that you shouldn't be doing
//     direct operations on platform widgets from the renderer process.
//
//     If you're in the browser, you're probably dealing with NativeViews,
//     unless you're in the IPC layer, which will be translating between
//     NativeViewIds from the renderer and NativeViews.
//
// The name 'View' here meshes with macOS where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.
//
// TODO(crbug.com/40267204): Both gfx::NativeEvent and ui::PlatformEvent
// are typedefs for native event types on different platforms, but they're
// slightly different and used in different places. They should be merged.
//
// TODO(crbug.com/40157665): gfx::NativeCursor is ui::Cursor in Aura;
// perhaps remove gfx::NativeCursor and use ui::Cursor everywhere?

#if defined(USE_AURA)
namespace aura {
class Window;
}
namespace ui {
class Cursor;
class Event;
namespace mojom {
enum class CursorType;
}
}  // namespace ui

#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN)
struct IAccessible;
#elif BUILDFLAG(IS_IOS)
#ifdef __OBJC__
@class UIImage;
#else
struct objc_object;
class UIImage;
#endif  // __OBJC__
#elif BUILDFLAG(IS_MAC)
#ifdef __OBJC__
@class NSImage;
@class NSView;
@class NSWindow;
#else
struct objc_object;
class NSImage;
class NSView;
class NSWindow;
#endif  // __OBJC__
#endif

#if BUILDFLAG(IS_ANDROID)
struct ANativeWindow;
namespace ui {
class WindowAndroid;
class ViewAndroid;
}  // namespace ui
#endif
class SkBitmap;

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern "C" {
struct _AtkObject;
using AtkObject = struct _AtkObject;
}
#endif

namespace gfx {

#if defined(USE_AURA)
using NativeCursor = ui::Cursor;
using NativeView = aura::Window*;
using NativeWindow = aura::Window*;
using NativeEvent = ui::Event*;
#elif BUILDFLAG(IS_IOS)
using NativeCursor = void*;
using NativeView = base::apple::WeakUIView;
using NativeWindow = base::apple::WeakUIWindow;
using NativeEvent = base::apple::OwnedUIEvent;
#elif BUILDFLAG(IS_MAC)
using NativeCursor = base::apple::OwnedNSCursor;
using NativeEvent = base::apple::OwnedNSEvent;
// NativeViews and NativeWindows on macOS are not necessarily in the same
// process as the NSViews and NSWindows that they represent. Require an explicit
// function call (GetNativeNSView or GetNativeNSWindow) to retrieve the
// underlying NSView or NSWindow <https://crbug.com/893719>. These are wrapper
// classes only and do not maintain any ownership, thus the __unsafe_unretained.
class GFX_EXPORT NativeView {
 public:
  constexpr NativeView() = default;
  // TODO(ccameron): Make this constructor explicit.
  constexpr NativeView(__unsafe_unretained NSView* ns_view)
      : ns_view_(ns_view) {}

  // This function name is verbose (that is, not just GetNSView) so that it
  // is easily grep-able.
  NSView* GetNativeNSView() const { return ns_view_; }

  explicit operator bool() const { return ns_view_ != nullptr; }
  bool operator==(const NativeView& other) const {
    return ns_view_ == other.ns_view_;
  }
  bool operator!=(const NativeView& other) const {
    return ns_view_ != other.ns_view_;
  }
  bool operator<(const NativeView& other) const {
    return ns_view_ < other.ns_view_;
  }
  std::string ToString() const;

 private:
#if HAS_FEATURE(objc_arc)
  __unsafe_unretained NSView* ns_view_ = nullptr;
#else
  // RAW_PTR_EXCLUSION: Points to Objective-C object which isn't supported.
  RAW_PTR_EXCLUSION NSView* ns_view_ = nullptr;
#endif
};
class GFX_EXPORT NativeWindow {
 public:
  constexpr NativeWindow() = default;
  // TODO(ccameron): Make this constructor explicit.
  constexpr NativeWindow(__unsafe_unretained NSWindow* ns_window)
      : ns_window_(ns_window) {}

  // This function name is verbose (that is, not just GetNSWindow) so that it
  // is easily grep-able.
  NSWindow* GetNativeNSWindow() const { return ns_window_; }

  explicit operator bool() const { return ns_window_ != nullptr; }
  bool operator==(const NativeWindow& other) const {
    return ns_window_ == other.ns_window_;
  }
  bool operator!=(const NativeWindow& other) const {
    return ns_window_ != other.ns_window_;
  }
  bool operator<(const NativeWindow& other) const {
    return ns_window_ < other.ns_window_;
  }
  std::string ToString() const;

 private:
#if defined(__has_feature) && __has_feature(objc_arc)
  __unsafe_unretained NSWindow* ns_window_ = nullptr;
#else
  // RAW_PTR_EXCLUSION: #global-scope, #union; Also, points to Objective-C
  // object which isn't supported.
  RAW_PTR_EXCLUSION NSWindow* ns_window_ = nullptr;
#endif
};
#elif BUILDFLAG(IS_ANDROID)
using NativeCursor = void*;
using NativeView = ui::ViewAndroid*;
using NativeWindow = ui::WindowAndroid*;
using NativeEvent = base::android::ScopedJavaGlobalRef<jobject>;
#else
#error Unknown build environment.
#endif

#if BUILDFLAG(IS_WIN)
using NativeViewAccessible = IAccessible*;
#elif BUILDFLAG(IS_IOS)
#ifdef __OBJC__
using NativeViewAccessible = id;
#else
using NativeViewAccessible = struct objc_object*;
#endif
#elif BUILDFLAG(IS_MAC)
#ifdef __OBJC__
using NativeViewAccessible = id;
#else
using NativeViewAccessible = struct objc_object*;
#endif
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Linux doesn't have a native font type.
using NativeViewAccessible = AtkObject*;
#else
// Android, Chrome OS, etc.
using UnimplementedNativeViewAccessible =
    struct _UnimplementedNativeViewAccessible;
using NativeViewAccessible = UnimplementedNativeViewAccessible*;
#endif

// Note: for test_shell we're packing a pointer into the NativeViewId. So, if
// you make it a type which is smaller than a pointer, you have to fix
// test_shell.
//
// See comment at the top of the file for usage.
using NativeViewId = intptr_t;

// AcceleratedWidget provides a surface to compositors to paint pixels.
#if BUILDFLAG(IS_WIN)
using AcceleratedWidget = HWND;
constexpr AcceleratedWidget kNullAcceleratedWidget = nullptr;
#elif BUILDFLAG(IS_IOS)
using AcceleratedWidget = uint64_t;  // A UIView*.
constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#elif BUILDFLAG(IS_MAC)
using AcceleratedWidget = uint64_t;
constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#elif BUILDFLAG(IS_ANDROID)
using AcceleratedWidget = ANativeWindow*;
constexpr AcceleratedWidget kNullAcceleratedWidget = nullptr;
#elif BUILDFLAG(IS_OZONE)
using AcceleratedWidget = uint32_t;
constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#else
#error unknown platform
#endif

}  // namespace gfx

#endif  // UI_GFX_NATIVE_WIDGET_TYPES_H_
