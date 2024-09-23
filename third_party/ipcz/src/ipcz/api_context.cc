// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/api_context.h"

#include <utility>

namespace ipcz {

namespace {

thread_local bool is_thread_within_api_call = false;

}  // namespace

APIContext::APIContext()
    : was_thread_within_api_call_(
          std::exchange(is_thread_within_api_call, true)) {}

APIContext::~APIContext() {
  is_thread_within_api_call = was_thread_within_api_call_;
}

bool APIContext::IsCurrentThreadWithinAPICall() {
  return is_thread_within_api_call;
}

}  // namespace ipcz
