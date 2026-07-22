// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/mock_input_event_activation_protector.h"

#include <utility>

#include "ui/views/input_protection/input_protection_policy.h"

namespace views {
MockInputEventActivationProtector::MockInputEventActivationProtector() =
    default;
MockInputEventActivationProtector::MockInputEventActivationProtector(
    std::unique_ptr<InputProtectionPolicy> policy)
    : InputEventActivationProtector(std::move(policy)) {}
MockInputEventActivationProtector::~MockInputEventActivationProtector() =
    default;
}  // namespace views
