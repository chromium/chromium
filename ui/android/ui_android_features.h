// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_UI_ANDROID_FEATURES_H_
#define UI_ANDROID_UI_ANDROID_FEATURES_H_

#include "base/feature_list.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// Keep sorted!

// Feature controlling whether or not HDR is enabled on Android.
// TODO(crbug.com/40263227): Leave this as a kill switch until Android U
// ships.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidHDR);

// Feature controlling how to compute work area on Android.
// TODO(crbug.com/372385871): Cleanup flag after investigating nothing is broken
// after changing the default behavior for
// availWidth/availHeight/availTop/availLeft.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidUseCorrectDisplayWorkArea);

// Use Android WindowManager's WindowMetrics as the data source for top-level
// browser window bounds in Blink.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidUseCorrectWindowBounds);

// Enables usage of the display topology API to obtain information about all
// displays.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidUseDisplayTopology);

// Enables using occlusion information from Android to save CPU and memory.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidWindowOcclusion);

// When launching an intent, check whether the caller has the permission to
// access a URI before returning the result.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kCheckIntentCallerPermission);

// Use the old-style opening of an External Picker when uploading files.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kDeprecatedExternalPickerFunction);

// Whether photo picker should be disabled for video capture.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kDisablePhotoPickerForVideoCapture);

// Whether to enable the refactor of the smallestScreenWidthDp override.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kRefactorMinWidthContextOverride);

// Reports bottom overscrolls on the web page.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kReportBottomOverscrolls);

// Kill switch to turn off validation in TextViewWithLeading that requires a
// leading value to be configured.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kRequireLeadingInTextViewWithLeading);

// Use ACTION_OPEN_DOCUMENT rather than ACTION_GET_CONTENT when selecting a
// file.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kSelectFileOpenDocument);

// TODO(crbug.com/328601354): Cleanup flag after investigating nothing is broken
// after changing the default behavior for EventForwarder observers.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(
    kSendTouchMovesToEventForwarderObservers);

// Enables the new ETC1 encoder (used in tab and back/forward thumbnails).
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kUseNewEtc1Encoder);

// Conducts a check to determine if the View is eligible to a Hit. This is a
// mitigation of when the prerendered view (hidden) somehow receives the touch
// event even though it is hidden, due to the ordering of the `children_` in
// `ViewAndroid`. Refer to crbug.com/442832509 for more details.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kCheckHitEligibility);

// Enables touchpad overscroll for history navigation.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(
    kAndroidTouchpadOverscrollHistoryNavigation);

}  // namespace ui

#endif  // UI_ANDROID_UI_ANDROID_FEATURES_H_
