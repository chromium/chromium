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
  DCHECK(!g_message_center);
  DCHECK(lock_screen_controller);
  g_message_center = new MessageCenterImpl(std::move(lock_screen_controller));
}

// static
MessageCenter* MessageCenter::Get() {
  return g_message_center;
}

// static
void MessageCenter::Shutdown() {
  DCHECK(g_message_center);
  delete g_message_center;
  g_message_center = nullptr;
}

MessageCenter::MessageCenter() {}

MessageCenter::~MessageCenter() {}

}  // namespace message_center
