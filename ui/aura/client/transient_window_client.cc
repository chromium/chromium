// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/transient_window_client.h"

namespace aura {
namespace client {

namespace {

TransientWindowClient* g_transient_window_client_instance = nullptr;

}  // namespace

void SetTransientWindowClient(TransientWindowClient* client) {
  g_transient_window_client_instance = client;
}

TransientWindowClient* GetTransientWindowClient() {
  return g_transient_window_client_instance;
}

}  // namespace client
}  // namespace aura
