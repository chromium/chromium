// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_utils.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"

namespace views {

class WidgetUtilsTest : public testing::Test {
 public:
  WidgetUtilsTest() = default;
  ~WidgetUtilsTest() override = default;

  void SetUp() override {
    // Set up tree 1
    r1_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);
    c1_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);
    c2_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);
    g1_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);

    r1_->Add(c1_.get());
    r1_->Add(c2_.get());
    c1_->Add(g1_.get());

    // Set up tree 2
    r2_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);
    c3_ = ui::Layer::Create(ui::LAYER_NOT_DRAWN);
    r2_->Add(c3_.get());
  }

 protected:
  std::unique_ptr<ui::Layer> r1_;
  std::unique_ptr<ui::Layer> c1_;
  std::unique_ptr<ui::Layer> c2_;
  std::unique_ptr<ui::Layer> g1_;

  std::unique_ptr<ui::Layer> r2_;
  std::unique_ptr<ui::Layer> c3_;
};

TEST_F(WidgetUtilsTest, GetLayerRelation_Same) {
  LayerRelation relation = GetLayerRelation(r1_.get(), r1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kFirstIsChildOfSecond);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, nullptr);
  EXPECT_EQ(relation.ancestor_of_second, nullptr);
}

TEST_F(WidgetUtilsTest, GetLayerRelation_FirstIsChildOfSecond_Direct) {
  LayerRelation relation = GetLayerRelation(c1_.get(), r1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kFirstIsChildOfSecond);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, c1_.get());
  EXPECT_EQ(relation.ancestor_of_second, nullptr);
}

TEST_F(WidgetUtilsTest, GetLayerRelation_FirstIsChildOfSecond_Indirect) {
  LayerRelation relation = GetLayerRelation(g1_.get(), r1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kFirstIsChildOfSecond);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, c1_.get());
  EXPECT_EQ(relation.ancestor_of_second, nullptr);
}

TEST_F(WidgetUtilsTest, GetLayerRelation_SecondIsChildOfFirst_Direct) {
  LayerRelation relation = GetLayerRelation(r1_.get(), c1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kSecondIsChildOfFirst);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, nullptr);
  EXPECT_EQ(relation.ancestor_of_second, c1_.get());
}

TEST_F(WidgetUtilsTest, GetLayerRelation_SecondIsChildOfFirst_Indirect) {
  LayerRelation relation = GetLayerRelation(r1_.get(), g1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kSecondIsChildOfFirst);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, nullptr);
  EXPECT_EQ(relation.ancestor_of_second, c1_.get());
}

TEST_F(WidgetUtilsTest, GetLayerRelation_Siblings_Direct) {
  LayerRelation relation = GetLayerRelation(c1_.get(), c2_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kSiblings);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, c1_.get());
  EXPECT_EQ(relation.ancestor_of_second, c2_.get());
}

TEST_F(WidgetUtilsTest, GetLayerRelation_Siblings_Indirect) {
  LayerRelation relation = GetLayerRelation(g1_.get(), c2_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kSiblings);
  EXPECT_EQ(relation.common_parent, r1_.get());
  EXPECT_EQ(relation.ancestor_of_first, c1_.get());
  EXPECT_EQ(relation.ancestor_of_second, c2_.get());
}

TEST_F(WidgetUtilsTest, GetLayerRelation_Disjoint) {
  LayerRelation relation = GetLayerRelation(c1_.get(), r2_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kDisjoint);
  EXPECT_EQ(relation.common_parent, nullptr);
  EXPECT_EQ(relation.ancestor_of_first, nullptr);
  EXPECT_EQ(relation.ancestor_of_second, nullptr);
}

TEST_F(WidgetUtilsTest, GetLayerRelation_Nullptr) {
  LayerRelation relation = GetLayerRelation(nullptr, r1_.get());
  EXPECT_EQ(relation.type, LayerRelation::Type::kDisjoint);
  EXPECT_EQ(relation.common_parent, nullptr);
  EXPECT_EQ(relation.ancestor_of_first, nullptr);
  EXPECT_EQ(relation.ancestor_of_second, nullptr);
}

}  // namespace views
