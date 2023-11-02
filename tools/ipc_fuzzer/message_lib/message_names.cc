// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/message_lib/message_names.h"
#include "tools/ipc_fuzzer/message_lib/all_messages.h"

#include "tools/ipc_fuzzer/message_lib/all_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  names.Add(static_cast<uint32_t>(name::ID), #name);

void PopulateIpcMessageNames(ipc_fuzzer::MessageNames& names) {
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

namespace ipc_fuzzer {

// static
MessageNames* MessageNames::all_names_ = NULL;

MessageNames::MessageNames() {
}

MessageNames::~MessageNames() {
}

// static
MessageNames* MessageNames::GetInstance() {
  if (!all_names_) {
    all_names_ = new MessageNames();
    PopulateIpcMessageNames(*all_names_);
  }
  return all_names_;
}

}  // namespace ipc_fuzzer
