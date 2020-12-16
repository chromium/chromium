// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/property_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

namespace x11 {

TEST(X11PropertyCacheTest, Basic) {
  x11::Connection connection;
  auto window = CreateDummyWindow("", &connection);
  auto atom = gfx::GetAtom("DUMMY ATOM");
  SetProperty(window, atom, x11::Atom::ATOM, atom, &connection).Sync();

  PropertyCache cache(&connection, window, {atom});
  auto& response = cache.GetProperty(atom);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->bytes_after, 0u);
  EXPECT_EQ(response->format, 32);
  EXPECT_EQ(response->type, x11::Atom::ATOM);
  EXPECT_EQ(*response->value->front_as<x11::Atom>(), atom);
  EXPECT_EQ(response->value_len, 1u);
}

}  // namespace x11
