// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Returns a string containing the x-coordinate of each of the views in |model|.
std::string BoundsString(const ViewModel& model) {
  std::string result;
  for (int i = 0; i < model.view_size(); ++i) {
    if (i != 0)
      result += " ";
    result += base::NumberToString(model.ideal_bounds(i).x());
  }
  return result;
}

// Returns a string containing the id of each of the views in |model|.
std::string ViewIDsString(const ViewModel& model) {
  std::string result;
  for (int i = 0; i < model.view_size(); ++i) {
    if (i != 0)
      result += " ";
    result += base::NumberToString(model.view_at(i)->GetID());
  }
  return result;
}

}  // namespace

TEST(ViewModel, BasicAssertions) {
  View v1;
  ViewModel model;
  model.Add(&v1, 0);
  EXPECT_EQ(1, model.view_size());
  EXPECT_EQ(&v1, model.view_at(0));
  gfx::Rect v1_bounds(1, 2, 3, 4);
  model.set_ideal_bounds(0, v1_bounds);
  EXPECT_EQ(v1_bounds, model.ideal_bounds(0));
  EXPECT_EQ(0, model.GetIndexOfView(&v1));
}

TEST(ViewModel, Move) {
  View v1, v2, v3;
  v1.SetID(0);
  v2.SetID(1);
  v3.SetID(2);
  ViewModel model;
  model.Add(&v1, 0);
  model.Add(&v2, 1);
  model.Add(&v3, 2);
  model.Move(0, 2);
  EXPECT_EQ("1 2 0", ViewIDsString(model));

  model.Move(2, 0);
  EXPECT_EQ("0 1 2", ViewIDsString(model));
}

TEST(ViewModel, MoveViewOnly) {
  View v1, v2, v3;
  v1.SetID(0);
  v2.SetID(1);
  v3.SetID(2);
  ViewModel model;
  model.Add(&v1, 0);
  model.Add(&v2, 1);
  model.Add(&v3, 2);
  model.set_ideal_bounds(0, gfx::Rect(10, 0, 1, 2));
  model.set_ideal_bounds(1, gfx::Rect(11, 0, 1, 2));
  model.set_ideal_bounds(2, gfx::Rect(12, 0, 1, 2));
  model.MoveViewOnly(0, 2);
  EXPECT_EQ("1 2 0", ViewIDsString(model));
  EXPECT_EQ("10 11 12", BoundsString(model));

  model.MoveViewOnly(2, 0);
  EXPECT_EQ("0 1 2", ViewIDsString(model));
  EXPECT_EQ("10 11 12", BoundsString(model));

  model.MoveViewOnly(0, 1);
  EXPECT_EQ("1 0 2", ViewIDsString(model));
  EXPECT_EQ("10 11 12", BoundsString(model));

  model.MoveViewOnly(1, 0);
  EXPECT_EQ("0 1 2", ViewIDsString(model));
  EXPECT_EQ("10 11 12", BoundsString(model));
}

}  // namespace views
