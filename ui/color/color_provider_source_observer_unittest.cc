// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source_observer.h"

namespace ui {

namespace {

class MockColorProviderSource : public ColorProviderSource {
 public:
  MOCK_METHOD(ColorProviderKey, GetColorProviderKey, (), (const, override));
  MOCK_METHOD(const ColorProvider*, GetColorProvider, (), (const, override));
  MOCK_METHOD(RendererColorMap,
              GetRendererColorMap,
              (ColorProviderKey::ColorMode, ColorProviderKey::ForcedColors),
              (const, override));
};

class MockColorProviderSourceObserver : public ColorProviderSourceObserver {
 public:
  void ObserveForTesting(ColorProviderSource* source) { Observe(source); }
  MOCK_METHOD(void, OnColorProviderChanged, (), ());
};

}  // namespace

using ColorProviderSourceObserverTest = testing::Test;

// Verify the observation is reset when source is destroyed.
TEST_F(ColorProviderSourceObserverTest, DestroyingSourceClearsItFromObservers) {
  auto source = std::make_unique<MockColorProviderSource>();
  MockColorProviderSourceObserver observer_1;
  MockColorProviderSourceObserver observer_2;

  // OnColorProviderChanged() should be called twice. Once when the observer is
  // first added to the source and again when the source is destroyed.
  EXPECT_CALL(observer_1, OnColorProviderChanged()).Times(2);
  EXPECT_CALL(observer_2, OnColorProviderChanged()).Times(2);

  auto set_observation = [&](MockColorProviderSourceObserver* observer) {
    observer->ObserveForTesting(source.get());
    EXPECT_EQ(source.get(), observer->GetColorProviderSourceForTesting());
    EXPECT_TRUE(source->observers_for_testing().HasObserver(observer));
  };
  set_observation(&observer_1);
  set_observation(&observer_2);

  // When the source is destroyed the observer's source() method should return
  // nullptr.
  source.reset();
  EXPECT_EQ(nullptr, observer_1.GetColorProviderSourceForTesting());
  EXPECT_EQ(nullptr, observer_2.GetColorProviderSourceForTesting());
}

// Verify the observer is removed from the source's observer list when the
// observer is destroyed.
TEST_F(ColorProviderSourceObserverTest, DestroyingObserverClearsItFromSource) {
  MockColorProviderSource source;
  auto observer_1 = std::make_unique<MockColorProviderSourceObserver>();
  auto observer_2 = std::make_unique<MockColorProviderSourceObserver>();

  // OnColorProviderChanged() should be called once when the observer is first
  // added to the source.
  EXPECT_CALL(*observer_1, OnColorProviderChanged()).Times(1);
  EXPECT_CALL(*observer_2, OnColorProviderChanged()).Times(1);

  auto set_observation = [&](MockColorProviderSourceObserver* observer) {
    observer->ObserveForTesting(&source);
    EXPECT_EQ(&source, observer->GetColorProviderSourceForTesting());
    EXPECT_TRUE(source.observers_for_testing().HasObserver(observer));
  };
  set_observation(observer_1.get());
  set_observation(observer_2.get());

  // When the observer is destroyed it should be removed from the source's list
  // of observers. Other observers should remain.
  observer_1.reset();
  EXPECT_FALSE(source.observers_for_testing().empty());
  EXPECT_TRUE(source.observers_for_testing().HasObserver(observer_2.get()));

  observer_2.reset();
  EXPECT_TRUE(source.observers_for_testing().empty());

  // Further calls to NotifyColorProviderChanged() should succeed and not
  // result in any more calls to OnColorProviderChanged().
  source.NotifyColorProviderChanged();
}

// Verify OnColorProviderChanged() is called by the source as expected.
TEST_F(ColorProviderSourceObserverTest,
       ObserverCorrectlySetsObservationOfSource) {
  MockColorProviderSource source;
  MockColorProviderSourceObserver observer_1;
  MockColorProviderSourceObserver observer_2;

  // observer_2 should receive notifications up to and including when its
  // observation is reset below.
  EXPECT_CALL(observer_1, OnColorProviderChanged()).Times(4);
  EXPECT_CALL(observer_2, OnColorProviderChanged()).Times(3);

  auto set_observation_and_notify =
      [&](MockColorProviderSourceObserver* observer) {
        observer->ObserveForTesting(&source);
        EXPECT_EQ(&source, observer->GetColorProviderSourceForTesting());
        EXPECT_TRUE(source.observers_for_testing().HasObserver(observer));
        source.NotifyColorProviderChanged();
      };
  set_observation_and_notify(&observer_1);
  set_observation_and_notify(&observer_2);

  observer_2.ObserveForTesting(nullptr);

  // After removing the observation for observer_2 we should still get
  // notifications for observer_1.
  EXPECT_EQ(&source, observer_1.GetColorProviderSourceForTesting());
  EXPECT_EQ(nullptr, observer_2.GetColorProviderSourceForTesting());
  EXPECT_TRUE(source.observers_for_testing().HasObserver(&observer_1));
  EXPECT_FALSE(source.observers_for_testing().HasObserver(&observer_2));
  source.NotifyColorProviderChanged();
}

}  // namespace ui
