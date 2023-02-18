// Copyright 2017 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client/annotation.h"

#include <array>
#include <string>

#include "client/annotation_list.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "test/gtest_death.h"
#include "util/misc/clock.h"
#include "util/synchronization/scoped_spin_guard.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

class SpinGuardAnnotation final : public Annotation {
 public:
  SpinGuardAnnotation(Annotation::Type type, const char name[])
      : Annotation(type,
                   name,
                   &value_,
                   ConcurrentAccessGuardMode::kScopedSpinGuard) {}

  bool Set(bool value, uint64_t spin_guard_timeout_ns) {
    auto guard = TryCreateScopedSpinGuard(spin_guard_timeout_ns);
    if (!guard) {
      return false;
    }
    value_ = value;
    SetSize(sizeof(value_));
    return true;
  }

 private:
  bool value_;
};

class ScopedSpinGuardUnlockThread final : public Thread {
 public:
  ScopedSpinGuardUnlockThread(ScopedSpinGuard scoped_spin_guard,
                              uint64_t sleep_time_ns)
      : scoped_spin_guard_(std::move(scoped_spin_guard)),
        sleep_time_ns_(sleep_time_ns) {}

 private:
  void ThreadMain() override {
    SleepNanoseconds(sleep_time_ns_);

    // Move the ScopedSpinGuard member into a local variable which is
    // destroyed when ThreadMain() returns.
    ScopedSpinGuard local_scoped_spin_guard(std::move(scoped_spin_guard_));

    // After this point, local_scoped_spin_guard will be destroyed and unlocked.
  }

  ScopedSpinGuard scoped_spin_guard_;
  const uint64_t sleep_time_ns_;
};

class Annotation : public testing::Test {
 public:
  void SetUp() override {
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(&annotations_);
  }

  void TearDown() override {
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(nullptr);
  }

  size_t AnnotationsCount() {
    size_t result = 0;
    for (auto* annotation : annotations_) {
      if (annotation->is_set())
        ++result;
    }
    return result;
  }

 protected:
  crashpad::AnnotationList annotations_;
};

TEST_F(Annotation, Basics) {
  constexpr crashpad::Annotation::Type kType =
      crashpad::Annotation::UserDefinedType(1);

  const char kName[] = "annotation 1";
  char buffer[1024];
  crashpad::Annotation annotation(kType, kName, buffer);

  EXPECT_FALSE(annotation.is_set());
  EXPECT_EQ(0u, AnnotationsCount());

  EXPECT_EQ(kType, annotation.type());
  EXPECT_EQ(0u, annotation.size());
  EXPECT_EQ(std::string(kName), annotation.name());
  EXPECT_EQ(buffer, annotation.value());

  annotation.SetSize(10);

  EXPECT_TRUE(annotation.is_set());
  EXPECT_EQ(1u, AnnotationsCount());

  EXPECT_EQ(10u, annotation.size());
  EXPECT_EQ(&annotation, *annotations_.begin());

  annotation.Clear();

  EXPECT_FALSE(annotation.is_set());
  EXPECT_EQ(0u, AnnotationsCount());

  EXPECT_EQ(0u, annotation.size());
}

TEST_F(Annotation, StringType) {
  crashpad::StringAnnotation<5> annotation("name");

  EXPECT_FALSE(annotation.is_set());

  EXPECT_EQ(crashpad::Annotation::Type::kString, annotation.type());
  EXPECT_EQ(0u, annotation.size());
  EXPECT_EQ(std::string("name"), annotation.name());
  EXPECT_EQ(0u, annotation.value().size());

  annotation.Set("test");

  EXPECT_TRUE(annotation.is_set());
  EXPECT_EQ(1u, AnnotationsCount());

  EXPECT_EQ(4u, annotation.size());
  EXPECT_EQ("test", annotation.value());

  annotation.Set(std::string("loooooooooooong"));

  EXPECT_TRUE(annotation.is_set());
  EXPECT_EQ(1u, AnnotationsCount());

  EXPECT_EQ(5u, annotation.size());
  EXPECT_EQ("loooo", annotation.value());
}

TEST_F(Annotation, BaseAnnotationShouldNotSupportSpinGuard) {
  char buffer[1024];
  crashpad::Annotation annotation(
      crashpad::Annotation::Type::kString, "no-spin-guard", buffer);
  EXPECT_EQ(annotation.concurrent_access_guard_mode(),
            crashpad::Annotation::ConcurrentAccessGuardMode::kUnguarded);
#if !DCHECK_IS_ON()
  // This fails a DCHECK() in debug builds, so only test it when DCHECK()
  // is not on.
  EXPECT_EQ(std::nullopt, annotation.TryCreateScopedSpinGuard(0));
#endif
}

TEST_F(Annotation, CustomAnnotationShouldSupportSpinGuardAndSet) {
  constexpr crashpad::Annotation::Type kType =
      crashpad::Annotation::UserDefinedType(1);
  SpinGuardAnnotation spin_guard_annotation(kType, "spin-guard");
  EXPECT_EQ(spin_guard_annotation.concurrent_access_guard_mode(),
            crashpad::Annotation::ConcurrentAccessGuardMode::kScopedSpinGuard);
  EXPECT_TRUE(spin_guard_annotation.Set(true, 0));
  EXPECT_EQ(1U, spin_guard_annotation.size());
}

TEST_F(Annotation, CustomAnnotationSetShouldFailIfRunConcurrently) {
  constexpr crashpad::Annotation::Type kType =
      crashpad::Annotation::UserDefinedType(1);
  SpinGuardAnnotation spin_guard_annotation(kType, "spin-guard");
  auto guard = spin_guard_annotation.TryCreateScopedSpinGuard(0);
  EXPECT_NE(std::nullopt, guard);
  // This should fail, since the guard is already held and the timeout is 0.
  EXPECT_FALSE(spin_guard_annotation.Set(true, 0));
}

TEST_F(Annotation,
       CustomAnnotationSetShouldSucceedIfSpinGuardUnlockedAsynchronously) {
  constexpr crashpad::Annotation::Type kType =
      crashpad::Annotation::UserDefinedType(1);
  SpinGuardAnnotation spin_guard_annotation(kType, "spin-guard");
  auto guard = spin_guard_annotation.TryCreateScopedSpinGuard(0);
  EXPECT_NE(std::nullopt, guard);
  // Pass the guard off to a background thread which unlocks it after 1 ms.
  constexpr uint64_t kSleepTimeNs = 1000000;  // 1 ms
  ScopedSpinGuardUnlockThread unlock_thread(std::move(guard.value()),
                                            kSleepTimeNs);
  unlock_thread.Start();

  // Try to set the annotation with a 100 ms timeout.
  constexpr uint64_t kSpinGuardTimeoutNanos = 100000000;  // 100 ms

  // This should succeed after 1 ms, since the timeout is much larger than the
  // time the background thread holds the guard.
  EXPECT_TRUE(spin_guard_annotation.Set(true, kSpinGuardTimeoutNanos));

  unlock_thread.Join();
}

TEST(StringAnnotation, ArrayOfString) {
  static crashpad::StringAnnotation<4> annotations[] = {
      {"test-1", crashpad::StringAnnotation<4>::Tag::kArray},
      {"test-2", crashpad::StringAnnotation<4>::Tag::kArray},
      {"test-3", crashpad::StringAnnotation<4>::Tag::kArray},
      {"test-4", crashpad::StringAnnotation<4>::Tag::kArray},
  };

  for (auto& annotation : annotations) {
    EXPECT_FALSE(annotation.is_set());
  }
}

#if DCHECK_IS_ON()

TEST(AnnotationDeathTest, EmbeddedNUL) {
  crashpad::StringAnnotation<5> annotation("name");
  EXPECT_DEATH_CHECK(annotation.Set(std::string("te\0st", 5)), "embedded NUL");
}

#endif

}  // namespace
}  // namespace test
}  // namespace crashpad
