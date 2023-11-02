// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/class_property.h"

#include <limits.h>

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(const char*)
DEFINE_UI_CLASS_PROPERTY_TYPE(int)
DEFINE_UI_CLASS_PROPERTY_TYPE(float)

namespace {

class TestProperty {
 public:
  TestProperty() = default;
  TestProperty(const TestProperty&) = delete;
  TestProperty& operator=(const TestProperty&) = delete;
  ~TestProperty() {
    last_deleted_ = this;
  }
  static void* last_deleted() { return last_deleted_; }

 private:
  static void* last_deleted_;
};

class TestCascadingProperty {
 public:
  explicit TestCascadingProperty(ui::PropertyHandler* handler)
      : handler_(handler) {}
  ~TestCascadingProperty() = default;

  ui::PropertyHandler* handler() { return handler_; }

 private:
  raw_ptr<ui::PropertyHandler> handler_;
};

void* TestProperty::last_deleted_ = nullptr;

class AssignableTestProperty {
 public:
  AssignableTestProperty() {}
  AssignableTestProperty(int value) : value_(value) {}
  AssignableTestProperty(const AssignableTestProperty& other)
      : value_(other.value_) {}
  AssignableTestProperty(AssignableTestProperty&& other)
      : value_(std::move(other.value_)), was_move_assigned_(true) {}
  AssignableTestProperty& operator=(const AssignableTestProperty& other) {
    value_ = other.value_;
    was_move_assigned_ = false;
    return *this;
  }
  AssignableTestProperty& operator=(AssignableTestProperty&& other) {
    value_ = std::move(other.value_);
    was_move_assigned_ = true;
    return *this;
  }

  int value() const { return value_; }
  bool was_move_assigned() const { return was_move_assigned_; }

 private:
  int value_ = 0;
  bool was_move_assigned_ = false;
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(TestProperty, kOwnedKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(AssignableTestProperty,
                                   kAssignableKey,
                                   nullptr)
DEFINE_CASCADING_OWNED_UI_CLASS_PROPERTY_KEY(TestCascadingProperty,
                                             kCascadingOwnedKey,
                                             nullptr)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(TestProperty*)
DEFINE_UI_CLASS_PROPERTY_TYPE(AssignableTestProperty*)
DEFINE_UI_CLASS_PROPERTY_TYPE(TestCascadingProperty*)

namespace ui {
namespace test {

namespace {

class TestPropertyHandler : public PropertyHandler {
 public:
  TestPropertyHandler() = default;
  explicit TestPropertyHandler(TestPropertyHandler* parent) : parent_(parent) {}
  ~TestPropertyHandler() override = default;
  int num_events() const { return num_events_; }

 protected:
  void AfterPropertyChange(const void* key, int64_t old_value) override {
    ++num_events_;
  }
  PropertyHandler* GetParentHandler() const override { return parent_; }

 private:
  int num_events_ = 0;
  raw_ptr<TestPropertyHandler> parent_ = nullptr;
};

const int kDefaultIntValue = -2;
const char* kDefaultStringValue = "squeamish";
const char* kTestStringValue = "ossifrage";
const float kDefaultFloatValue = 3.0;

DEFINE_UI_CLASS_PROPERTY_KEY(int, kIntKey, kDefaultIntValue)
DEFINE_UI_CLASS_PROPERTY_KEY(const char*, kStringKey, kDefaultStringValue)
DEFINE_UI_CLASS_PROPERTY_KEY(float, kFloatKey, kDefaultFloatValue)
}

TEST(PropertyTest, Property) {
  PropertyHandler h;

  // Non-existent properties should return the default values.
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));
  EXPECT_EQ(kDefaultFloatValue, h.GetProperty(kFloatKey));

  // A set property value should be returned again (even if it's the default
  // value).
  h.SetProperty(kIntKey, INT_MAX);
  EXPECT_EQ(INT_MAX, h.GetProperty(kIntKey));
  h.SetProperty(kIntKey, kDefaultIntValue);
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  h.SetProperty(kIntKey, INT_MIN);
  EXPECT_EQ(INT_MIN, h.GetProperty(kIntKey));

  h.SetProperty<const char*>(kStringKey, nullptr);
  EXPECT_EQ(NULL, h.GetProperty(kStringKey));
  h.SetProperty(kStringKey, kDefaultStringValue);
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));
  h.SetProperty(kStringKey, kTestStringValue);
  EXPECT_EQ(std::string(kTestStringValue), h.GetProperty(kStringKey));

  h.SetProperty(kFloatKey, 3.14f);
  EXPECT_EQ(3.14f, h.GetProperty(kFloatKey));
  h.SetProperty(kFloatKey, kDefaultFloatValue);
  EXPECT_EQ(kDefaultFloatValue, h.GetProperty(kFloatKey));
  h.SetProperty(kFloatKey, -234.5678f);
  EXPECT_EQ(-234.5678f, h.GetProperty(kFloatKey));

  // ClearProperty should restore the default value.
  h.ClearProperty(kIntKey);
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  h.ClearProperty(kStringKey);
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));
}

TEST(PropertyTest, OwnedProperty) {
  TestProperty* p3;
  {
    PropertyHandler h;

    EXPECT_EQ(nullptr, h.GetProperty(kOwnedKey));
    void* last_deleted = TestProperty::last_deleted();
    TestProperty* p1 =
        h.SetProperty(kOwnedKey, std::make_unique<TestProperty>());
    EXPECT_EQ(p1, h.GetProperty(kOwnedKey));
    EXPECT_EQ(last_deleted, TestProperty::last_deleted());

    TestProperty* p2 =
        h.SetProperty(kOwnedKey, std::make_unique<TestProperty>());
    EXPECT_EQ(p2, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p1, TestProperty::last_deleted());

    h.ClearProperty(kOwnedKey);
    EXPECT_EQ(nullptr, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());

    p3 = h.SetProperty(kOwnedKey, std::make_unique<TestProperty>());
    EXPECT_EQ(p3, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());
  }
  EXPECT_EQ(p3, TestProperty::last_deleted());
}

TEST(PropertyTest, AcquireAllPropertiesFrom) {
  // Set some properties on src, including an owned property.
  std::unique_ptr<PropertyHandler> src = std::make_unique<PropertyHandler>();
  void* last_deleted = TestProperty::last_deleted();
  EXPECT_FALSE(src->GetProperty(kOwnedKey));
  TestProperty* p1 =
      src->SetProperty(kOwnedKey, std::make_unique<TestProperty>());
  src->SetProperty(kIntKey, INT_MAX);

  // dest will take ownership of the owned property. Existing properties with
  // similar keys will be overwritten. Existing properties with different keys
  // will remain unchanged.
  std::unique_ptr<PropertyHandler> dest = std::make_unique<PropertyHandler>();
  dest->SetProperty(kIntKey, INT_MIN);
  dest->SetProperty(kStringKey, kTestStringValue);

  dest->AcquireAllPropertiesFrom(std::move(*src));
  EXPECT_EQ(p1, dest->GetProperty(kOwnedKey));     // Ownership taken.
  EXPECT_EQ(INT_MAX, dest->GetProperty(kIntKey));  // Overwritten.
  // Remains unchanged.
  EXPECT_EQ(std::string(kTestStringValue), dest->GetProperty(kStringKey));

  // src no longer has properties.
  EXPECT_TRUE(src->GetAllPropertyKeys().empty());
  EXPECT_FALSE(src->GetProperty(kOwnedKey));
  EXPECT_EQ(kDefaultIntValue, src->GetProperty(kIntKey));
  // Destroy src. Owned property remains alive.
  src.reset();
  EXPECT_EQ(last_deleted, TestProperty::last_deleted());
  // Destroy dest, now the owned property is deleted.
  dest.reset();
  EXPECT_EQ(p1, TestProperty::last_deleted());
}

TEST(PropertyTest, AssignableProperty) {
  PropertyHandler h;

  // Verify that assigning a property by value allocates a value.
  EXPECT_EQ(nullptr, h.GetProperty(kAssignableKey));
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  AssignableTestProperty* p = h.GetProperty(kAssignableKey);
  EXPECT_NE(nullptr, p);
  EXPECT_EQ(1, p->value());

  // Verify that assigning by value updates the existing value without
  // additional allocation.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_EQ(p, h.GetProperty(kAssignableKey));
  EXPECT_EQ(2, p->value());

  // Same as the above case, but with a const reference instead of a move.
  const AssignableTestProperty kTestProperty2{3};
  h.SetProperty(kAssignableKey, kTestProperty2);
  EXPECT_EQ(p, h.GetProperty(kAssignableKey));
  EXPECT_EQ(3, p->value());

  // Verify that clearing the property deallocates the value.
  h.ClearProperty(kAssignableKey);
  EXPECT_EQ(nullptr, h.GetProperty(kAssignableKey));

  // Verify that setting by value after clearing allocates a new value.
  h.SetProperty(kAssignableKey, AssignableTestProperty{4});
  EXPECT_EQ(4, h.GetProperty(kAssignableKey)->value());
}

TEST(PropertyTest, SetProperty_ForwardsParametersCorrectly) {
  PropertyHandler h;

  // New property from a const ref.
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from inline rvalue ref.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from lvalue.
  AssignableTestProperty test_property{3};
  h.SetProperty(kAssignableKey, test_property);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from lvalue ref.
  AssignableTestProperty& ref = test_property;
  h.SetProperty(kAssignableKey, ref);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from moved rvalue ref.
  h.SetProperty(kAssignableKey, std::move(test_property));
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from const ref.
  const AssignableTestProperty& const_ref = kTestProperty;
  h.SetProperty(kAssignableKey, const_ref);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // New property from rvalue ref.
  h.ClearProperty(kAssignableKey);
  h.SetProperty(kAssignableKey, AssignableTestProperty{4});
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // New property from lvalue.
  h.ClearProperty(kAssignableKey);
  test_property = AssignableTestProperty{5};
  h.SetProperty(kAssignableKey, test_property);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());
}

TEST(PropertyTest, PropertyChangedEvent) {
  TestPropertyHandler h;

  // Verify that initially setting the value creates an event.
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  EXPECT_EQ(1, h.num_events());

  // Verify that assigning by value sends an event.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_EQ(2, h.num_events());

  // Same as the above case, but with a const reference instead of a move.
  const AssignableTestProperty kTestProperty2{3};
  h.SetProperty(kAssignableKey, kTestProperty2);
  EXPECT_EQ(3, h.num_events());

  // Verify that clearing the property creates an event.
  h.ClearProperty(kAssignableKey);
  EXPECT_EQ(4, h.num_events());

  // Verify that setting a heap-allocated value also ticks the event counter.
  h.SetProperty(kAssignableKey, std::make_unique<AssignableTestProperty>(4));
  EXPECT_EQ(5, h.num_events());

  // Verify that overwriting a heap-allocated value ticks the event counter.
  h.SetProperty(kAssignableKey, std::make_unique<AssignableTestProperty>(5));
  EXPECT_EQ(6, h.num_events());
}

TEST(PropertyTest, CascadingProperties) {
  TestPropertyHandler h;
  TestPropertyHandler h2(&h);

  // Set the property on the parent handler.
  h.SetProperty(kCascadingOwnedKey,
                std::make_unique<TestCascadingProperty>(&h));

  // Get the property value from the child handler.
  auto* value = h2.GetProperty(kCascadingOwnedKey);

  EXPECT_TRUE(value);
  // The property value should have a reference to |h|.
  EXPECT_EQ(&h, value->handler());
}

// TODO(kylixrd, pbos): Once all the call-sites are fixed to only use the
// unique_ptr version for owned properties, enable the following test to ensure
// that passing raw pointers for owned properties will, in fact, DCHECK.
TEST(PropertyTest, DISABLED_CheckedOwnedProperties) {
  PropertyHandler h;

  EXPECT_EQ(nullptr, h.GetProperty(kOwnedKey));
  // The following SetProperty call should DCHECK if it's enabled. NOTE: This
  // will leak the TestProperty!
  EXPECT_DEATH_IF_SUPPORTED(h.SetProperty(kOwnedKey, new TestProperty()), "");
  EXPECT_EQ(nullptr, h.GetProperty(kOwnedKey));
}

} // namespace test
} // namespace ui
