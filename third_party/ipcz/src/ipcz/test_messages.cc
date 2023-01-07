// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/test_messages.h"

#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::test::msg {

#pragma pack(push, 1)

// clang-format off
#include "ipcz/message_macros/message_params_definition_macros.h"
#include "ipcz/test_messages_generator.h"
#include "ipcz/message_macros/undef_message_macros.h"

#include "ipcz/message_macros/message_definition_macros.h"
#include "ipcz/test_messages_generator.h"
#include "ipcz/message_macros/undef_message_macros.h"

#include "ipcz/message_macros/message_listener_definition_macros.h"
#include "ipcz/test_messages_generator.h"
#include "ipcz/message_macros/undef_message_macros.h"

#include "ipcz/message_macros/message_listener_dispatch_macros.h"
#include "ipcz/test_messages_generator.h"
#include "ipcz/message_macros/undef_message_macros.h"
// clang-format on

#pragma pack(pop)

}  // namespace ipcz::test::msg
