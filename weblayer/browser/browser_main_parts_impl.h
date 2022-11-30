// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "components/embedder_support/android/metrics/memory_metrics_logger.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

class PrefService;

namespace performance_manager {
class PerformanceManagerLifetime;
}

#if BUILDFLAG(IS_ANDROID)
namespace crash_reporter {
class ChildExitObserver;
}
#endif

namespace weblayer {
class BrowserProcess;
struct MainParams;

class BrowserMainPartsImpl : public content::BrowserMainParts {
 public:
  BrowserMainPartsImpl(MainParams* params,
                       std::unique_ptr<PrefService> local_state);

  BrowserMainPartsImpl(const BrowserMainPartsImpl&) = delete;
  BrowserMainPartsImpl& operator=(const BrowserMainPartsImpl&) = delete;

  ~BrowserMainPartsImpl() override;

  // BrowserMainParts overrides.
  int PreCreateThreads() override;
  int PreEarlyInitialization() override;
  void PostCreateThreads() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void OnFirstIdle() override;
  void PostMainMessageLoopRun() override;

 private:
  raw_ptr<MainParams> params_;

  std::unique_ptr<BrowserProcess> browser_process_;
  std::unique_ptr<performance_manager::PerformanceManagerLifetime>
      performance_manager_lifetime_;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<metrics::MemoryMetricsLogger> memory_metrics_logger_;
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
#endif  // BUILDFLAG(IS_ANDROID)

  // Ownership of this moves to BrowserProcess. See
  // ContentBrowserClientImpl::local_state_ for details.
  std::unique_ptr<PrefService> local_state_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_
