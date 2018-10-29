// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/ui_features.h"

namespace features {

// Keep sorted!
UI_BASE_EXPORT extern const base::Feature kEnableEmojiContextMenu;
UI_BASE_EXPORT extern const base::Feature kEnableFloatingVirtualKeyboard;
UI_BASE_EXPORT extern const base::Feature
    kEnableFullscreenHandwritingVirtualKeyboard;
UI_BASE_EXPORT extern const base::Feature kEnableStylusVirtualKeyboard;
UI_BASE_EXPORT extern const base::Feature kEnableVirtualKeyboardMdUi;
UI_BASE_EXPORT extern const base::Feature kEnableVirtualKeyboardUkm;
UI_BASE_EXPORT extern const base::Feature kExperimentalUi;
UI_BASE_EXPORT extern const base::Feature kSystemKeyboardLock;
UI_BASE_EXPORT extern const base::Feature kTouchableAppContextMenu;
UI_BASE_EXPORT extern const base::Feature kNotificationIndicator;
UI_BASE_EXPORT extern const base::Feature kUiCompositorScrollWithLayers;

UI_BASE_EXPORT bool IsTouchableAppContextMenuEnabled();
UI_BASE_EXPORT bool IsNotificationIndicatorEnabled();

UI_BASE_EXPORT bool IsUiGpuRasterizationEnabled();

#if defined(OS_WIN)
UI_BASE_EXPORT extern const base::Feature kInputPaneOnScreenKeyboard;
UI_BASE_EXPORT extern const base::Feature kPointerEventsForTouch;
UI_BASE_EXPORT extern const base::Feature kPrecisionTouchpad;
UI_BASE_EXPORT extern const base::Feature kPrecisionTouchpadScrollPhase;
UI_BASE_EXPORT extern const base::Feature kTSFImeSupport;

// Returns true if the system should use WM_POINTER events for touch events.
UI_BASE_EXPORT bool IsUsingWMPointerForTouch();
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
UI_BASE_EXPORT extern const base::Feature kDirectManipulationStylus;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

// Used to have ash (Chrome OS system UI) run in its own process.
// TODO(jamescook): Make flag only available in Chrome OS.
UI_BASE_EXPORT extern const base::Feature kMash;

UI_BASE_EXPORT extern const base::Feature kSingleProcessMash;

// Returns true if Chrome's aura usage is backed by the WindowService.
UI_BASE_EXPORT bool IsUsingWindowService();

// Returns true if ash in running in a separate process (and is hosting the UI
// service and Viz graphics). See //ash/README.md.
UI_BASE_EXPORT bool IsMultiProcessMash();

// Returns true if code outside of ash is using the WindowService. In this mode
// there are two aura::Envs. Ash uses one with Env::Mode::LOCAL. Non-ash code
// uses an aura::Env with a mode of MUS. The non-ash code using mus targets the
// WindowService that ash is running. This exercises the WindowService mojo APIs
// similar to kMash, but leaves ash and browser running in the same process.
// See //ash/README.md.
UI_BASE_EXPORT bool IsSingleProcessMash();

#if defined(OS_MACOSX)
UI_BASE_EXPORT extern const base::Feature kHostWindowsInAppShimProcess;

// Returns true if the NSWindows for apps will be created in the app's process,
// and will forward input to the browser process.
UI_BASE_EXPORT bool HostWindowsInAppShimProcess();
#endif  //  defined(OS_MACOSX)

// Use mojo communication in the drm platform instead of paramtraits. Remove
// this switch (and associated code) when the drm platform always uses mojo
// communication.
// TODO(rjkroege): Remove in http://crbug.com/806092.
UI_BASE_EXPORT extern const base::Feature kEnableOzoneDrmMojo;
UI_BASE_EXPORT bool IsOzoneDrmMojo();

// Whether default UI should use a dark mode color scheme, if enabled on
// macOS Mojave/Windows 10.
UI_BASE_EXPORT extern const base::Feature kDarkMode;

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
