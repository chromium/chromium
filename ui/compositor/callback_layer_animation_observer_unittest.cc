// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/callback_layer_animation_observer.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/test/layer_animation_observer_test_api.h"

namespace ui {
namespace test {

// Simple class that tracks whether callbacks were invoked and when.
class TestCallbacks {
 public:
  TestCallbacks();
  virtual ~TestCallbacks();

  void ResetCallbackObservations();

  void set_should_delete_observer_on_animations_ended(
      bool should_delete_observer_on_animations_ended) {
    should_delete_observer_on_animations_ended_ =
        should_delete_observer_on_animations_ended;
  }

  bool animations_started() const { return animations_started_; }

  bool animations_ended() const { return animations_ended_; }

  virtual void AnimationsStarted(const CallbackLayerAnimationObserver&);

  virtual bool AnimationsEnded(const CallbackLayerAnimationObserver&);

  testing::AssertionResult StartedEpochIsBeforeEndedEpoch();

 private:
  // Monotonic counter that tracks the next time snapshot.
  int next_epoch_ = 0;

  // Is true when AnimationsStarted() has been called.
  bool animations_started_ = false;

  // Relative time snapshot of when AnimationsStarted() was last called.
  int animations_started_epoch_ = -1;

  // Is true when AnimationsEnded() has been called.
  bool animations_ended_ = false;

  // Relative time snapshot of when AnimationsEnded() was last called.
  int animations_ended_epoch_ = -1;

  // The return value for AnimationsEnded().
  bool should_delete_observer_on_animations_ended_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCallbacks);
};

TestCallbacks::TestCallbacks() {}

TestCallbacks::~TestCallbacks() {}

void TestCallbacks::ResetCallbackObservations() {
  next_epoch_ = 0;
  animations_started_ = false;
  animations_started_epoch_ = -1;
  animations_ended_ = false;
  animations_ended_epoch_ = -1;
  should_delete_observer_on_animations_ended_ = false;
}

void TestCallbacks::AnimationsStarted(const CallbackLayerAnimationObserver&) {
  animations_started_ = true;
  animations_started_epoch_ = next_epoch_++;
}

bool TestCallbacks::AnimationsEnded(const CallbackLayerAnimationObserver&) {
  animations_ended_ = true;
  animations_ended_epoch_ = next_epoch_++;
  return should_delete_observer_on_animations_ended_;
}

testing::AssertionResult TestCallbacks::StartedEpochIsBeforeEndedEpoch() {
  if (animations_started_epoch_ < animations_ended_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The started epoch=" << animations_started_epoch_
           << " is NOT before the ended epoch=" << animations_ended_epoch_;
  }
}

// A child of TestCallbacks that can explicitly delete a
// CallbackLayerAnimationObserver in the AnimationsStarted() or
// AnimationsEnded() callback.
class TestCallbacksThatExplicitlyDeletesObserver : public TestCallbacks {
 public:
  TestCallbacksThatExplicitlyDeletesObserver();

  void set_observer_to_delete_in_animation_started(
      CallbackLayerAnimationObserver* observer) {
    observer_to_delete_in_animation_started_ = observer;
  }

  void set_observer_to_delete_in_animation_ended(
      CallbackLayerAnimationObserver* observer) {
    observer_to_delete_in_animation_ended_ = observer;
  }

  // TestCallbacks:
  void AnimationsStarted(
      const CallbackLayerAnimationObserver& observer) override;
  bool AnimationsEnded(const CallbackLayerAnimationObserver& observer) override;

 private:
  // The observer to delete, if non-NULL, in AnimationsStarted().
  CallbackLayerAnimationObserver* observer_to_delete_in_animation_started_ =
      nullptr;

  // The observer to delete, if non-NULL, in AnimationsEnded().
  CallbackLayerAnimationObserver* observer_to_delete_in_animation_ended_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestCallbacksThatExplicitlyDeletesObserver);
};

TestCallbacksThatExplicitlyDeletesObserver::
    TestCallbacksThatExplicitlyDeletesObserver() {}

void TestCallbacksThatExplicitlyDeletesObserver::AnimationsStarted(
    const CallbackLayerAnimationObserver& observer) {
  if (observer_to_delete_in_animation_started_)
    delete observer_to_delete_in_animation_started_;
  TestCallbacks::AnimationsStarted(observer);
}

bool TestCallbacksThatExplicitlyDeletesObserver::AnimationsEnded(
    const CallbackLayerAnimationObserver& observer) {
  if (observer_to_delete_in_animation_ended_)
    delete observer_to_delete_in_animation_ended_;
  return TestCallbacks::AnimationsEnded(observer);
}

// A test specific CallbackLayerAnimationObserver that will set a bool when
// destroyed.
class TestCallbackLayerAnimationObserver
    : public CallbackLayerAnimationObserver {
 public:
  TestCallbackLayerAnimationObserver(
      AnimationStartedCallback animation_started_callback,
      AnimationEndedCallback animation_ended_callback,
      bool* destroyed);

  TestCallbackLayerAnimationObserver(
      AnimationStartedCallback animation_started_callback,
      bool should_delete_observer,
      bool* destroyed);

  TestCallbackLayerAnimationObserver(
      AnimationEndedCallback animation_ended_callback,
      bool* destroyed);

  ~TestCallbackLayerAnimationObserver() override;

 private:
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TestCallbackLayerAnimationObserver);
};

TestCallbackLayerAnimationObserver::TestCallbackLayerAnimationObserver(
    AnimationStartedCallback animation_started_callback,
    AnimationEndedCallback animation_ended_callback,
    bool* destroyed)
    : CallbackLayerAnimationObserver(animation_started_callback,
                                     animation_ended_callback),
      destroyed_(destroyed) {
  if (destroyed_)
    (*destroyed_) = false;
}

TestCallbackLayerAnimationObserver::TestCallbackLayerAnimationObserver(
    AnimationStartedCallback animation_started_callback,
    bool should_delete_observer,
    bool* destroyed)
    : CallbackLayerAnimationObserver(animation_started_callback,
                                     should_delete_observer),
      destroyed_(destroyed) {
  if (destroyed_)
    (*destroyed_) = false;
}

TestCallbackLayerAnimationObserver::TestCallbackLayerAnimationObserver(
    AnimationEndedCallback animation_ended_callback,
    bool* destroyed)
    : CallbackLayerAnimationObserver(animation_ended_callback),
      destroyed_(destroyed) {
  if (destroyed_)
    (*destroyed_) = false;
}

TestCallbackLayerAnimationObserver::~TestCallbackLayerAnimationObserver() {
  if (destroyed_)
    (*destroyed_) = true;
}

class CallbackLayerAnimationObserverTest : public testing::Test {
 public:
  CallbackLayerAnimationObserverTest();
  ~CallbackLayerAnimationObserverTest() override;

 protected:
  // Creates a LayerAnimationSequence.  The lifetime of the sequence will be
  // managed by this.
  LayerAnimationSequence* CreateLayerAnimationSequence();

  std::unique_ptr<TestCallbacks> callbacks_;

  std::unique_ptr<CallbackLayerAnimationObserver> observer_;

  std::unique_ptr<LayerAnimationObserverTestApi> observer_test_api_;

  // List of managaged sequences created by CreateLayerAnimationSequence() that
  // need to be destroyed.
  std::vector<std::unique_ptr<LayerAnimationSequence>> sequences_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackLayerAnimationObserverTest);
};

CallbackLayerAnimationObserverTest::CallbackLayerAnimationObserverTest()
    : callbacks_(new TestCallbacks()),
      observer_(new CallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(callbacks_.get())),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(callbacks_.get())))),
      observer_test_api_(new LayerAnimationObserverTestApi(observer_.get())) {}

CallbackLayerAnimationObserverTest::~CallbackLayerAnimationObserverTest() {
  observer_test_api_.reset();
  // The |observer_| will detach from all attached sequences upon destruction so
  // we need to explicitly delete the |observer_| before the |sequences_| and
  // |callbacks_|.
  observer_.reset();
}

LayerAnimationSequence*
CallbackLayerAnimationObserverTest::CreateLayerAnimationSequence() {
  sequences_.emplace_back(new LayerAnimationSequence);
  return sequences_.back().get();
}

class CallbackLayerAnimationObserverTestOverwrite
    : public CallbackLayerAnimationObserverTest {
 public:
  CallbackLayerAnimationObserverTestOverwrite();

 protected:
  void AnimationStarted(const CallbackLayerAnimationObserver& observer);

  std::unique_ptr<CallbackLayerAnimationObserver> CreateAnimationObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackLayerAnimationObserverTestOverwrite);
};

CallbackLayerAnimationObserverTestOverwrite::
    CallbackLayerAnimationObserverTestOverwrite() {
  observer_ = CreateAnimationObserver();
  observer_test_api_ =
      std::make_unique<LayerAnimationObserverTestApi>(observer_.get());
}

void CallbackLayerAnimationObserverTestOverwrite::AnimationStarted(
    const CallbackLayerAnimationObserver& observer) {
  observer_->OnLayerAnimationAborted(sequences_.front().get());
  observer_test_api_.reset();
  // Replace the current observer with a new observer so that the destructor
  // gets called on the current observer.
  observer_ = CreateAnimationObserver();
}

std::unique_ptr<CallbackLayerAnimationObserver>
CallbackLayerAnimationObserverTestOverwrite::CreateAnimationObserver() {
  return std::make_unique<CallbackLayerAnimationObserver>(
      base::BindRepeating(
          &CallbackLayerAnimationObserverTestOverwrite::AnimationStarted,
          base::Unretained(this)),
      base::BindRepeating([](const CallbackLayerAnimationObserver& observer) {
        return false;
      }));
}

TEST(CallbackLayerAnimationObserverDestructionTest, VerifyFalseAutoDelete) {
  TestCallbacks callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(false);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          false, &is_destroyed);
  observer->SetActive();

  EXPECT_FALSE(is_destroyed);
  delete observer;
}

TEST(CallbackLayerAnimationObserverDestructionTest, VerifyTrueAutoDelete) {
  TestCallbacks callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(false);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          true, &is_destroyed);
  observer->SetActive();

  EXPECT_TRUE(is_destroyed);
}

TEST(CallbackLayerAnimationObserverDestructionTest,
     AnimationEndedReturnsFalse) {
  TestCallbacks callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(false);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(&callbacks)),
          &is_destroyed);
  observer->SetActive();

  EXPECT_FALSE(is_destroyed);
  delete observer;
}

TEST(CallbackLayerAnimationObserverDestructionTest, AnimationEndedReturnsTrue) {
  TestCallbacks callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(true);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(&callbacks)),
          &is_destroyed);
  observer->SetActive();

  EXPECT_TRUE(is_destroyed);
}

// Verifies that there are not heap-use-after-free errors when an observer has
// its animation aborted and it gets destroyed due to a
// unique_ptr<CallbackLayerAnimationObserver> being assigned a new value.
TEST_F(CallbackLayerAnimationObserverTestOverwrite,
       VerifyOverwriteOnAnimationStart) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);
  observer_->OnLayerAnimationEnded(sequence_1);
  observer_->SetActive();
  EXPECT_FALSE(observer_->active());
}

TEST_F(CallbackLayerAnimationObserverTest, VerifyInitialState) {
  EXPECT_FALSE(observer_->active());
  EXPECT_EQ(0, observer_->aborted_count());
  EXPECT_EQ(0, observer_->successful_count());

  EXPECT_FALSE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

// Verifies that the CallbackLayerAnimationObserver is robust to explicit
// deletes caused as a side effect of calling the AnimationsStartedCallback()
// when there are no animation sequences attached. This test also guards against
// heap-use-after-free errors.
TEST_F(
    CallbackLayerAnimationObserverTest,
    ExplicitlyDeleteObserverInAnimationStartedCallbackWithNoSequencesAttached) {
  TestCallbacksThatExplicitlyDeletesObserver callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(true);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(&callbacks)),
          &is_destroyed);

  callbacks.set_observer_to_delete_in_animation_started(observer);

  observer->SetActive();

  EXPECT_TRUE(is_destroyed);
}

// Verifies that the CallbackLayerAnimationObserver is robust to explicit
// deletes caused as a side effect of calling the AnimationsStartedCallback()
// when there are some animation sequences attached. This test also guards
// against heap-use-after-free errors.
TEST_F(
    CallbackLayerAnimationObserverTest,
    ExplicitlyDeleteObserverInAnimationStartedCallbackWithSomeSequencesAttached) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  TestCallbacksThatExplicitlyDeletesObserver callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(true);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(&callbacks)),
          &is_destroyed);

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);

  callbacks.set_observer_to_delete_in_animation_started(observer);

  observer->SetActive();

  EXPECT_TRUE(is_destroyed);
}

// Verifies that a 'true' return value for AnimationEndedCallback is ignored if
// the CallbackLayerAnimationObserver is explicitly deleted as a side effect of
// calling the AnimationEndedCallback. This test also guards against
// heap-use-after-free errors.
TEST_F(CallbackLayerAnimationObserverTest,
       IgnoreTrueReturnValueForAnimationEndedCallbackIfExplicitlyDeleted) {
  TestCallbacksThatExplicitlyDeletesObserver callbacks;
  callbacks.set_should_delete_observer_on_animations_ended(true);

  bool is_destroyed = false;

  TestCallbackLayerAnimationObserver* observer =
      new TestCallbackLayerAnimationObserver(
          base::BindRepeating(&TestCallbacks::AnimationsStarted,
                              base::Unretained(&callbacks)),
          base::BindRepeating(&TestCallbacks::AnimationsEnded,
                              base::Unretained(&callbacks)),
          &is_destroyed);

  callbacks.set_observer_to_delete_in_animation_ended(observer);

  observer->SetActive();

  EXPECT_TRUE(is_destroyed);
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenNoSequencesWereAttached) {
  observer_->SetActive();

  EXPECT_FALSE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_TRUE(callbacks_->animations_ended());
  EXPECT_TRUE(callbacks_->StartedEpochIsBeforeEndedEpoch());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenAllSequencesAreAttachedButNoneWereStarted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_FALSE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenAllSequencesAreAttachedAndOnlySomeWereStarted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_FALSE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenAllSequencesAreAttachedAndOnlySomeWereCompleted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationEnded(sequence_1);

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_FALSE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveAfterAllSequencesWereStartedButNoneWereCompleted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenAllSequencesAreStartedAndOnlySomeWereCompleted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);
  observer_->OnLayerAnimationEnded(sequence_1);

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveWhenAllSequencesWereCompleted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);
  observer_->OnLayerAnimationEnded(sequence_1);
  observer_->OnLayerAnimationEnded(sequence_2);

  observer_->SetActive();

  EXPECT_FALSE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_TRUE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest,
       SetActiveAgainAfterAllSequencesWereCompleted) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_3 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_4 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationStarted(sequence_2);
  observer_->OnLayerAnimationEnded(sequence_1);
  observer_->OnLayerAnimationEnded(sequence_2);

  observer_->SetActive();

  EXPECT_FALSE(observer_->active());

  observer_test_api_->AttachedToSequence(sequence_3);
  observer_test_api_->AttachedToSequence(sequence_4);

  callbacks_->ResetCallbackObservations();

  observer_->SetActive();

  EXPECT_TRUE(observer_->active());
  EXPECT_FALSE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
  EXPECT_EQ(2, observer_->successful_count());

  observer_->OnLayerAnimationStarted(sequence_3);
  observer_->OnLayerAnimationStarted(sequence_4);

  EXPECT_TRUE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_FALSE(callbacks_->animations_ended());
  EXPECT_EQ(2, observer_->successful_count());

  observer_->OnLayerAnimationEnded(sequence_3);
  observer_->OnLayerAnimationEnded(sequence_4);

  EXPECT_FALSE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_TRUE(callbacks_->animations_ended());
  EXPECT_EQ(4, observer_->successful_count());
}

TEST_F(CallbackLayerAnimationObserverTest, DetachBeforeActive) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);
  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationEnded(sequence_1);
  observer_test_api_->DetachedFromSequence(sequence_1, true);
  observer_test_api_->DetachedFromSequence(sequence_2, true);

  observer_->SetActive();

  EXPECT_FALSE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_TRUE(callbacks_->animations_ended());
}

TEST_F(CallbackLayerAnimationObserverTest, DetachAfterActive) {
  LayerAnimationSequence* sequence_1 = CreateLayerAnimationSequence();
  LayerAnimationSequence* sequence_2 = CreateLayerAnimationSequence();

  observer_test_api_->AttachedToSequence(sequence_1);
  observer_test_api_->AttachedToSequence(sequence_2);

  observer_->SetActive();

  observer_->OnLayerAnimationStarted(sequence_1);
  observer_->OnLayerAnimationEnded(sequence_1);
  observer_test_api_->DetachedFromSequence(sequence_1, true);
  observer_test_api_->DetachedFromSequence(sequence_2, true);

  EXPECT_FALSE(observer_->active());
  EXPECT_TRUE(callbacks_->animations_started());
  EXPECT_TRUE(callbacks_->animations_ended());
}

}  // namespace test
}  // namespace ui
