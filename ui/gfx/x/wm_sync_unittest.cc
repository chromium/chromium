// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/wm_sync.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/connection.h"

namespace x11 {

TEST(WmSyncTest, Basic) {
  Connection* connection = Connection::Get();

  bool synced = false;
  WmSync wm_sync(connection,
                 base::BindOnce([](bool* synced) { *synced = true; }, &synced));
  while (!synced) {
    connection->DispatchAll();
  }
}

}  // namespace x11
