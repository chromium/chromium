// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEW_METADATA_TEST_UTILS_H_
#define UI_VIEWS_TEST_VIEW_METADATA_TEST_UTILS_H_

#include "ui/views/view.h"

namespace views::test {

// Iterate through all the metadata for the given instance, reading each
// property and then setting the property. Exercises the getters, setters, and
// property type-converters associated with each.
void TestViewMetadata(View* view);

}  // namespace views::test

#endif  // UI_VIEWS_TEST_VIEW_METADATA_TEST_UTILS_H_
