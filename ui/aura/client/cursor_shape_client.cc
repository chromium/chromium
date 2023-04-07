// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/cursor_shape_client.h"

#include "base/check.h"
#include "ui/aura/env.h"

namespace aura::client {

CursorShapeClient::~CursorShapeClient() = default;

void SetCursorShapeClient(CursorShapeClient* client) {
  CHECK(aura::Env::HasInstance());
  aura::Env::GetInstance()->set_cursor_shape_client(client);
}

const CursorShapeClient& GetCursorShapeClient() {
  CHECK(aura::Env::HasInstance());
  CHECK(aura::Env::GetInstance()->cursor_shape_client());
  return *aura::Env::GetInstance()->cursor_shape_client();
}

}  // namespace aura::client
