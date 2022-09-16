// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_

#include <string>

#include "base/callback.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace weblayer {
class ProfileImpl;

// Various utilities to bridge to Java code for metrics related tests.

using OnLogsMetricsCallback =
    base::RepeatingCallback<void(metrics::ChromeUserMetricsExtension)>;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConsentType
enum class ConsentType {
  kConsent,
  kNoConsent,
  // If this is supplied to InstallTestGmsBridge(), the callback used to
  // determine if consent is given is not automatically called. Use
  // RunConsentCallback() to apply consent.
  kDelayConsent,
};

// Call this in the SetUp() test harness method to install the test
// GmsBridge and to set the metrics user consent state.
void InstallTestGmsBridge(
    ConsentType consent_type,
    const OnLogsMetricsCallback on_log_metrics = OnLogsMetricsCallback());

// Call this in the TearDown() test harness method to remove the GmsBridge.
void RemoveTestGmsBridge();

// See comment for kDelayConsent.
void RunConsentCallback(bool has_consent);

ProfileImpl* CreateProfile(const std::string& name, bool incognito = false);

void DestroyProfile(const std::string& name, bool incognito = false);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_METRICS_TEST_HELPER_H_
