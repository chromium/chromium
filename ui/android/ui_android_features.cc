// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_android_features.h"
#include "base/feature_list.h"

namespace ui {

BASE_FEATURE(kAndroidHDR, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidUseCorrectDisplayWorkArea,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidUseCorrectWindowBounds, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidUseDisplayTopology, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidWindowOcclusion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckIntentCallerPermission, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecatedExternalPickerFunction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePhotoPickerForVideoCapture,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRefactorMinWidthContextOverride,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportBottomOverscrolls, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRequireLeadingInTextViewWithLeading,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSelectFileOpenDocument, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSendTouchMovesToEventForwarderObservers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewEtc1Encoder, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckHitEligibility, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTouchpadOverscrollHistoryNavigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ui
