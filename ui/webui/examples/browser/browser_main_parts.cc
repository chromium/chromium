// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/browser_main_parts.h"

#include <tuple>

#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/webui/examples/browser/browser_context.h"

namespace webui_examples {

BrowserMainParts::BrowserMainParts() = default;

BrowserMainParts::~BrowserMainParts() = default;

int BrowserMainParts::PreMainMessageLoopRun() {
  std::ignore = temp_dir_.CreateUniqueTempDir();
  browser_context_ = std::make_unique<BrowserContext>(temp_dir_.GetPath());
  return 0;
}

void BrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop->QuitClosure());
}

void BrowserMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
}

}  // namespace webui_examples
