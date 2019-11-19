// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

namespace weblayer {
struct MainParams;

class BrowserMainPartsImpl : public content::BrowserMainParts {
 public:
  BrowserMainPartsImpl(MainParams* params,
                       const content::MainFunctionParams& main_function_params);
  ~BrowserMainPartsImpl() override;

  // BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;

 private:
  MainParams* params_;

  // For running weblayer_browsertests.
  const content::MainFunctionParams main_function_params_;
  bool run_message_loop_ = true;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainPartsImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_MAIN_PARTS_IMPL_H_
