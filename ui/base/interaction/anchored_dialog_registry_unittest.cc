// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/anchored_dialog_registry.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAnchorId);

constexpr ElementContext kElementContext =
    ElementContext::CreateFakeContextForTesting(1);

class TestFocusTarget : public AnchoredDialogRegistry::FocusTarget {
 public:
  TestFocusTarget() = default;
  ~TestFocusTarget() override = default;

  // AnchoredDialogRegistry::FocusTarget:
  bool FocusFromAnchor(bool reverse) override {
    calls_.push_back(reverse);
    return true;
  }

  const std::vector<bool>& calls() const { return calls_; }

 private:
  std::vector<bool> calls_;
};

// A backend that supports test elements of type `ElementType` and records which
// targets are currently registered, in registration order.
template <typename ElementType>
class TestBackendBase : public AnchoredDialogRegistry::Backend {
 public:
  TestBackendBase() = default;
  ~TestBackendBase() override = default;

  // AnchoredDialogRegistry::Backend:
  std::unique_ptr<AnchoredDialogRegistry::Registration> Register(
      TrackedElement& anchor,
      AnchoredDialogRegistry::FocusTarget& target) override {
    if (!anchor.IsA<ElementType>()) {
      return nullptr;
    }
    return std::make_unique<TestRegistration>(*this, target);
  }

  // Simulates focus traversal entering the dialog of the anchor.
  bool FocusMostRecent(bool reverse) {
    return !registered_.empty() && registered_.back()->FocusFromAnchor(reverse);
  }

  const std::vector<raw_ptr<AnchoredDialogRegistry::FocusTarget>>& registered()
      const {
    return registered_;
  }

 private:
  class TestRegistration : public AnchoredDialogRegistry::Registration {
   public:
    TestRegistration(TestBackendBase& backend,
                     AnchoredDialogRegistry::FocusTarget& target)
        : backend_(backend), target_(target) {
      backend_->registered_.push_back(&target);
    }
    ~TestRegistration() override {
      std::erase(backend_->registered_, &*target_);
    }

   private:
    const raw_ref<TestBackendBase> backend_;
    const raw_ref<AnchoredDialogRegistry::FocusTarget> target_;
  };

  std::vector<raw_ptr<AnchoredDialogRegistry::FocusTarget>> registered_;
};

class TestBackend : public TestBackendBase<test::TestElement> {
 public:
  DECLARE_SAFE_CAST_TARGET()
};

class TestOtherBackend
    : public TestBackendBase<test::TestElementOtherFramework> {
 public:
  DECLARE_SAFE_CAST_TARGET()
};

DEFINE_SAFE_CAST_TARGET(TestBackend)
DEFINE_SAFE_CAST_TARGET(TestOtherBackend)

}  // namespace

TEST(AnchoredDialogRegistryTest, NullAnchor) {
  auto registry = AnchoredDialogRegistry::MakeInstanceForTesting();
  registry->MaybeRegisterBackend<TestBackend>();
  TestFocusTarget target;
  EXPECT_FALSE(registry->Register(nullptr, target));
}

TEST(AnchoredDialogRegistryTest, NoSupportingBackend) {
  auto registry = AnchoredDialogRegistry::MakeInstanceForTesting();
  auto anchor = std::make_unique<test::TestElement>(kAnchorId, kElementContext);
  TestFocusTarget target;

  // No backends at all.
  EXPECT_FALSE(registry->Register(anchor.get(), target));

  // Only a backend for a different framework.
  registry->MaybeRegisterBackend<TestOtherBackend>();
  EXPECT_FALSE(registry->Register(anchor.get(), target));
  EXPECT_THAT(registry->GetBackend<TestOtherBackend>()->registered(),
              testing::IsEmpty());
}

TEST(AnchoredDialogRegistryTest, RegistrationLifetime) {
  auto registry = AnchoredDialogRegistry::MakeInstanceForTesting();
  registry->MaybeRegisterBackend<TestBackend>();
  auto* const backend = registry->GetBackend<TestBackend>();
  ASSERT_TRUE(backend);

  auto anchor = std::make_unique<test::TestElement>(kAnchorId, kElementContext);
  TestFocusTarget target;

  auto registration = registry->Register(anchor.get(), target);
  ASSERT_TRUE(registration);
  EXPECT_THAT(backend->registered(), testing::ElementsAre(&target));

  EXPECT_TRUE(backend->FocusMostRecent(/*reverse=*/false));
  EXPECT_TRUE(backend->FocusMostRecent(/*reverse=*/true));
  EXPECT_THAT(target.calls(), testing::ElementsAre(false, true));

  registration.reset();
  EXPECT_THAT(backend->registered(), testing::IsEmpty());
  EXPECT_FALSE(backend->FocusMostRecent(/*reverse=*/false));
}

TEST(AnchoredDialogRegistryTest, MultipleFrameworks) {
  auto registry = AnchoredDialogRegistry::MakeInstanceForTesting();
  registry->MaybeRegisterBackend<TestBackend>();
  registry->MaybeRegisterBackend<TestOtherBackend>();
  // Registering the same backend type again is a no-op.
  registry->MaybeRegisterBackend<TestBackend>();

  auto anchor = std::make_unique<test::TestElement>(kAnchorId, kElementContext);
  auto other_anchor = std::make_unique<test::TestElementOtherFramework>(
      kAnchorId, kElementContext);
  TestFocusTarget target;
  TestFocusTarget other_target;

  auto registration = registry->Register(anchor.get(), target);
  auto other_registration =
      registry->Register(other_anchor.get(), other_target);
  ASSERT_TRUE(registration);
  ASSERT_TRUE(other_registration);

  EXPECT_THAT(registry->GetBackend<TestBackend>()->registered(),
              testing::ElementsAre(&target));
  EXPECT_THAT(registry->GetBackend<TestOtherBackend>()->registered(),
              testing::ElementsAre(&other_target));
}

}  // namespace ui
