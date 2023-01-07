// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/cursor_shape_client.h"

#include "base/check.h"
#include "ui/aura/env.h"

namespace aura::client {

CursorShapeClient::~CursorShapeClient() = default;

void SetCursorShapeClient(CursorShapeClient* client) {
  DCHECK(aura::Env::HasInstance());
  aura::Env::GetInstance()->set_cursor_shape_client(client);
}

CursorShapeClient* GetCursorShapeClient() {
  DCHECK(aura::Env::HasInstance());
  return aura::Env::GetInstance()->cursor_shape_client();
}

}  // namespace aura::client
