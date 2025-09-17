// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_UI_TYPES_H_
#define UI_GFX_NATIVE_UI_TYPES_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/owned_objc.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <string>
#endif

#if BUILDFLAG(IS_IOS)
#include <variant>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

// This file provides cross platform typedefs for native ui types.
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
//     direct operations on platform ui types from the renderer process.
//
//     If you're in the browser, you're probably dealing with NativeViews,
//     unless you're in the IPC layer, which will be translating between
//     NativeViewIds from the renderer and NativeViews.
//
// The name 'View' here meshes with macOS where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.
//
// TODO(https://crbug.com/40267204): Both gfx::NativeEvent and ui::PlatformEvent
// are typedefs for native event types on different platforms, but they're
// slightly different and used in different places. They should be merged.
//
// TODO(https://crbug.com/40157665): gfx::NativeCursor is ui::Cursor in Aura;
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
class UIImage;
#endif  // __OBJC__
#elif BUILDFLAG(IS_MAC)
#ifdef __OBJC__
@class NSImage;
@class NSView;
@class NSWindow;
#else
class NSImage;
#endif  // __OBJC__
#endif

#if BUILDFLAG(IS_ANDROID)
struct ANativeWindow;
namespace ui {
class WindowAndroid;
class ViewAndroid;
}  // namespace ui
#endif

#if BUILDFLAG(IS_LINUX)
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
#if BUILDFLAG(USE_BLINK)
#if BUILDFLAG(IS_IOS_TVOS)
using NativeEvent =
    std::variant<base::apple::OwnedUIEvent, base::apple::OwnedUIPress>;
#else
using NativeEvent =
    std::variant<base::apple::OwnedUIEvent, base::apple::OwnedBEKeyEntry>;
#endif  // BUILDFLAG(IS_IOS_TVOS)
#else
using NativeEvent = base::apple::OwnedUIEvent;
#endif  // BUILDFLAG(USE_BLINK)
#elif BUILDFLAG(IS_MAC)
using NativeCursor = base::apple::OwnedNSCursor;
using NativeEvent = base::apple::OwnedNSEvent;
// NativeViews and NativeWindows on macOS are not necessarily in the same
// process as the NSViews and NSWindows that they represent. Require an explicit
// function call (GetNativeNSView or GetNativeNSWindow) to retrieve the
// underlying NSView or NSWindow <https://crbug.com/40597366>.
class COMPONENT_EXPORT(GFX) NativeView : public base::apple::WeakNSView {
 public:
  NativeView();
#ifdef __OBJC__
  explicit NativeView(NSView* ns_view);
  // This function name is verbose (that is, not just GetNSView) so that it
  // is easily grep-able.
  NSView* GetNativeNSView() const;
  // This is the base class's getter; please use the explicit GetNativeNSView()
  // from this class instead.
  NSView* Get() const = delete;
#endif  // __OBJC__
  std::string ToString() const;
};
class COMPONENT_EXPORT(GFX) NativeWindow : public base::apple::WeakNSWindow {
 public:
  NativeWindow();
#ifdef __OBJC__
  explicit NativeWindow(NSWindow* ns_window);
  // This function name is verbose (that is, not just GetNSWindow) so that it
  // is easily grep-able.
  NSWindow* GetNativeNSWindow() const;
  // This is the base class's getter; please use the explicit
  // GetNativeNSWindow() from this class instead.
  NSWindow* Get() const = delete;
#endif  // __OBJC__
  // This is needed to put NativeWindow into maps. This is kinda safe because to
  // construct the NativeWindow to be the search key, the window has to be alive
  // and thus the weak pointer hasn't gone away, but it's still not ideal.
  // TODO(avi): Remove this and `pointer_bits_`.
  bool operator<(const NativeWindow& other) const;
  std::string ToString() const;

 private:
  uintptr_t pointer_bits_ = 0;
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
// UIAccessibility is an informal protocol on NSObject, so make accessible
// objects owned NSObjects. Do not use as a general object wrapper.
using NativeViewAccessible = base::apple::OwnedNSObject;
#elif BUILDFLAG(IS_MAC)
using NativeViewAccessible = base::apple::OwnedNSAccessibility;
#elif BUILDFLAG(IS_LINUX)
// Linux doesn't have a native accessibility type.
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
// The compiler doesn't realize that a const nullptr can't point to anything
// mutable, so it's okay for this pointer to be duplicated.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunique-object-duplication"
inline constexpr AcceleratedWidget kNullAcceleratedWidget = nullptr;
#pragma clang diagnostic pop
#elif BUILDFLAG(IS_IOS)
using AcceleratedWidget = uint64_t;
inline constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#elif BUILDFLAG(IS_MAC)
using AcceleratedWidget = uint64_t;
inline constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#elif BUILDFLAG(IS_ANDROID)
using AcceleratedWidget = ANativeWindow*;
constexpr AcceleratedWidget kNullAcceleratedWidget = nullptr;
#elif BUILDFLAG(IS_OZONE)
using AcceleratedWidget = uint32_t;
inline constexpr AcceleratedWidget kNullAcceleratedWidget = 0;
#else
#error unknown platform
#endif

}  // namespace gfx

#endif  // UI_GFX_NATIVE_UI_TYPES_H_
