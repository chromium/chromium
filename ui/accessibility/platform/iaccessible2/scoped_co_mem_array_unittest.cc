// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/iaccessible2/scoped_co_mem_array.h"

#include <objbase.h>

#include <vector>

#include "base/win/windows_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

void AllocateComArray(std::vector<LONG>& vector,
                      LONG** out_ptr,
                      LONG* out_size) {
  *out_size = static_cast<LONG>(vector.size());
  *out_ptr = static_cast<LONG*>(CoTaskMemAlloc(sizeof(LONG) * vector.size()));
  for (std::size_t i = 0; i < vector.size(); i++)
    (*out_ptr)[i] = vector[i];
}

TEST(ScopedCoMemArray, Receive) {
  std::vector<LONG> vector{10, 20};
  ScopedCoMemArray<LONG> array;
  AllocateComArray(vector, array.Receive(), array.ReceiveSize());
  EXPECT_EQ(array.size(), 2);
  EXPECT_EQ(array[0], 10);
  EXPECT_EQ(array[1], 20);
}

}  //  namespace ui
