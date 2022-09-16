// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/site_isolation_policy.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace weblayer {
namespace {
using testing::UnorderedElementsAreArray;

// Some command-line switches override field trials - the tests need to be
// skipped in this case.
bool ShouldSkipBecauseOfConflictingCommandLineSwitches() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteIsolation))
    return true;

  return false;
}

}  // namespace

class SiteIsolationPolicyTest : public testing::Test {
 public:
  SiteIsolationPolicyTest() = default;

  SiteIsolationPolicyTest(const SiteIsolationPolicyTest&) = delete;
  SiteIsolationPolicyTest& operator=(const SiteIsolationPolicyTest&) = delete;

  void SetUp() override {
    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
  }

  // Sets the same memory threshold for both strict site isolation and partial
  // site isolation modes, since these tests care about both. For example,
  // UseDedicatedProcessesForAllSites() depends on the former, while preloaded
  // isolated origins use the latter.
  void SetMemoryThreshold(const std::string& threshold) {
    threshold_feature_.InitAndEnableFeatureWithParameters(
        site_isolation::features::kSiteIsolationMemoryThresholds,
        {{site_isolation::features::
              kStrictSiteIsolationMemoryThresholdParamName,
          threshold},
         {site_isolation::features::
              kPartialSiteIsolationMemoryThresholdParamName,
          threshold}});
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList threshold_feature_;
};

TEST_F(SiteIsolationPolicyTest, NoIsolationBelowMemoryThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  SetMemoryThreshold("768");
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());
}

TEST_F(SiteIsolationPolicyTest, IsolationAboveMemoryThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  SetMemoryThreshold("128");
  // Android should only use the preloaded origin list, while desktop should
  // isolate all sites.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_TRUE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());
#endif
}

TEST_F(SiteIsolationPolicyTest, IsolatedOriginsContainPreloadedOrigins) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  std::vector<url::Origin> expected_embedder_origins =
      site_isolation::GetBrowserSpecificBuiltInIsolatedOrigins();
  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_THAT(expected_embedder_origins,
              UnorderedElementsAreArray(isolated_origins));
}
}  // namespace weblayer
