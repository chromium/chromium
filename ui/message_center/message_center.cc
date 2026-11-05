// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center.h"

#include "ui/message_center/lock_screen/empty_lock_screen_controller.h"
#include "ui/message_center/message_center_impl.h"

namespace message_center {

//------------------------------------------------------------------------------

namespace {
static MessageCenter* g_message_center = nullptr;
}

// static
void MessageCenter::Initialize() {
  Initialize(std::make_unique<EmptyLockScreenController>());
}

// static
void MessageCenter::Initialize(
    std::unique_ptr<LockScreenController> lock_screen_controller) {
  CHECK(!g_message_center, base::NotFatalUntil::M163);
  CHECK(lock_screen_controller, base::NotFatalUntil::M163);
  g_message_center = new MessageCenterImpl(std::move(lock_screen_controller));
}

// static
void MessageCenter::InitializeForTesting(
    std::unique_ptr<MessageCenter> message_center) {
  CHECK(!g_message_center, base::NotFatalUntil::M163);
  CHECK(message_center, base::NotFatalUntil::M163);
  g_message_center = message_center.release();
}

// static
MessageCenter* MessageCenter::Get() {
  return g_message_center;
}

// static
void MessageCenter::Shutdown() {
  CHECK(g_message_center, base::NotFatalUntil::M163);
  delete g_message_center;
  g_message_center = nullptr;
}

MessageCenter::MessageCenter() = default;

MessageCenter::~MessageCenter() = default;

}  // namespace message_center
