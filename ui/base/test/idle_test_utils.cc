// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/idle_test_utils.h"

#include "ui/base/idle/idle_polling_service.h"
#include "ui/base/idle/idle_time_provider.h"

namespace ui {
namespace test {

ScopedIdleProviderForTest::ScopedIdleProviderForTest(
    std::unique_ptr<IdleTimeProvider> provider) {
  IdlePollingService::GetInstance()->SetProviderForTest(std::move(provider));
}

ScopedIdleProviderForTest::~ScopedIdleProviderForTest() {
  IdlePollingService::GetInstance()->SetProviderForTest(nullptr);
}

}  // namespace test
}  // namespace ui
