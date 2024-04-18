// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/property_cache.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class PropertyCacheTest : public testing::Test {
 public:
  ~PropertyCacheTest() override = default;

 protected:
  base::flat_map<Atom, PropertyCache::PropertyValue>& GetProperties(
      PropertyCache* property_cache) {
    return property_cache->properties_;
  }

  Connection* connection() { return connection_; }

  Window window() { return window_; }

 private:
  void SetUp() override {
    connection_ = Connection::Get();
    window_ = connection_->CreateDummyWindow("");
  }

  void TearDown() override {
    connection_->DestroyWindow({window_}).Sync();
    window_ = Window::None;
    connection_ = nullptr;
  }

  raw_ptr<Connection> connection_ = nullptr;
  Window window_ = Window::None;
};

TEST_F(PropertyCacheTest, GetSync) {
  auto atom = x11::GetAtom("DUMMY ATOM");
  connection()->SetProperty(window(), atom, Atom::CARDINAL, 1234);

  PropertyCache cache(connection(), window(), {atom});

  // The cache should Sync() on getting the value.
  EXPECT_FALSE(GetProperties(&cache)[atom].response.has_value());
  auto* value = cache.GetAs<uint32_t>(atom);
  EXPECT_TRUE(GetProperties(&cache)[atom].response.has_value());

  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);
}

TEST_F(PropertyCacheTest, GetAsync) {
  auto atom = x11::GetAtom("DUMMY ATOM");
  connection()->SetProperty(window(), atom, Atom::CARDINAL, 1234);

  PropertyCache cache(connection(), window(), {atom});

  // The cache should not Sync() unnecessarily.
  EXPECT_FALSE(GetProperties(&cache)[atom].response.has_value());
  connection()->Sync();
  connection()->DispatchAll();
  EXPECT_TRUE(GetProperties(&cache)[atom].response.has_value());

  auto* value = cache.GetAs<uint32_t>(atom);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);
}

TEST_F(PropertyCacheTest, Event) {
  auto atom = x11::GetAtom("DUMMY ATOM");
  connection()->SetProperty(window(), atom, Atom::CARDINAL, 1234);

  PropertyCache cache(connection(), window(), {atom});

  auto* value = cache.GetAs<uint32_t>(atom);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);

  // Change the property and sync to ensure the PropertyNotify event is ready to
  // be dispatched.
  bool have_response = false;
  connection()
      ->SetProperty(window(), atom, Atom::CARDINAL, 5678)
      .OnResponse(
          base::BindOnce([](bool* have_response,
                            Response<void> response) { *have_response = true; },
                         &have_response));

  // Dispatch the PropertyNotify event, which will cause the PropertyCache to
  // send another GetPropertyRequest.  Calling DispatchAll() would introduce a
  // race condition where we could get the GetPropertyResponse early if the 2
  // round trips are completed fast enough.  To avoid this, only dispatch until
  // the property request is finished.
  while (!have_response) {
    connection()->Flush();
    connection()->ReadResponses();
    connection()->Dispatch();
  }

  // We don't have the new GetPropertyResponse yet, so the old value should
  // still be there.
  value = cache.GetAs<uint32_t>(atom);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);

  // Complete the second round trip to acquire the new property value.
  connection()->Sync();
  connection()->DispatchAll();

  // Now the cache should have the new value.
  value = cache.GetAs<uint32_t>(atom);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 5678u);
}

TEST_F(PropertyCacheTest, GetAs) {
  auto atom = x11::GetAtom("DUMMY ATOM");
  connection()->SetProperty(window(), atom, Atom::CARDINAL, 1234);

  PropertyCache cache(connection(), window(), {atom});

  // Get() should return the correct property value.
  auto& response = cache.Get(atom);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->bytes_after, 0u);
  EXPECT_EQ(response->format, 32);
  EXPECT_EQ(response->type, Atom::CARDINAL);
  EXPECT_EQ(*response->value->cast_to<uint32_t>(), 1234u);
  EXPECT_EQ(response->value_len, 1u);

  // GetAs() should do the same thing as Get().
  size_t size = 0;
  auto* value = cache.GetAs<uint32_t>(atom, &size);
  EXPECT_EQ(size, 1u);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);

  // GetAs() should allow a nullptr size.
  value = cache.GetAs<uint32_t>(atom /* size is defaulted to nullptr */);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1234u);

  // The below checks make requests and requires event dispatching, so
  // synchronize all requests to avoid verbosity from Sync()ing everywhere.
  connection()->SynchronizeForTest(true);

  // GetAs() should return nullptr if the type's size is mismatched.
  connection()->SetProperty(window(), atom, Atom::CARDINAL,
                            static_cast<uint8_t>(123));
  connection()->DispatchAll();
  value = cache.GetAs<uint32_t>(atom, &size);
  EXPECT_EQ(size, 0u);
  EXPECT_FALSE(value);

  // GetAs() should return nullptr if the property has no elements.
  connection()->SetArrayProperty(window(), atom, Atom::CARDINAL,
                                 std::vector<uint32_t>());
  connection()->DispatchAll();
  value = cache.GetAs<uint32_t>(atom, &size);
  EXPECT_EQ(size, 0u);
  EXPECT_FALSE(value);

  // GetAs() should return nullptr if the property is deleted.
  connection()->DeleteProperty(window(), atom);
  connection()->DispatchAll();
  value = cache.GetAs<uint32_t>(atom, &size);
  EXPECT_EQ(size, 0u);
  EXPECT_FALSE(value);

  connection()->SynchronizeForTest(true);
}

}  // namespace x11
