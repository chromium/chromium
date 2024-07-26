// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/glib/scoped_gsignal.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/glib/scoped_gobject.h"

namespace ui {

namespace {

G_DECLARE_FINAL_TYPE(TestObject, test_object, TEST, OBJECT, GObject)

// Used by G_DECLARE_FINAL_TYPE above.
struct _TestObject {
  GObject parent_instance;
};

G_DEFINE_TYPE(TestObject, test_object, G_TYPE_OBJECT)

// Used by G_DEFINE_TYPE above.
void test_object_class_init(TestObjectClass*) {
  g_signal_newv("some-signal", test_object_get_type(),
                (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
                               G_SIGNAL_NO_HOOKS),
                nullptr, nullptr, nullptr, nullptr, G_TYPE_NONE, 0, nullptr);
}

// Used by G_DEFINE_TYPE above.
void test_object_init(TestObject* self) {}

ScopedGObject<GObject> CreateGObject() {
  return TakeGObject(G_OBJECT(g_object_new(test_object_get_type(), nullptr)));
}

void EmitSignal(GObject* instance, const gchar* detailed_signal) {
  g_signal_emit_by_name(instance, detailed_signal);
}

}  // namespace

TEST(ScopedGSignalTest, Empty) {
  ScopedGSignal signal;
  EXPECT_FALSE(signal.Connected());
}

TEST(ScopedGSignalTest, Construction) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal(instance, "some-signal",
                       base::BindRepeating([](GObject* obj) {}));
  EXPECT_TRUE(signal.Connected());
}

TEST(ScopedGSignalTest, DisconnectsOnDestruction) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  bool signal_fired = false;
  {
    ScopedGSignal signal(
        instance, "some-signal",
        base::BindRepeating([](bool* fired, GObject* obj) { *fired = true; },
                            &signal_fired));
    EXPECT_TRUE(signal.Connected());
  }

  EmitSignal(instance, "some-signal");
  EXPECT_FALSE(signal_fired);
}

TEST(ScopedGSignalTest, DisconnectsOnGClosureFinalize) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal(instance, "some-signal",
                       base::BindRepeating([](GObject* obj) {}));
  EXPECT_TRUE(signal.Connected());

  instance.Reset();
  EXPECT_FALSE(signal.Connected());
}

TEST(ScopedGSignalTest, DisconnectsOnReset) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal(instance, "some-signal",
                       base::BindRepeating([](GObject* obj) {}));
  EXPECT_TRUE(signal.Connected());

  signal.Reset();
  EXPECT_FALSE(signal.Connected());
}

TEST(ScopedGSignalTest, MoveConstruct) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal1(instance, "some-signal",
                        base::BindRepeating([](GObject* obj) {}));
  EXPECT_TRUE(signal1.Connected());

  ScopedGSignal signal2{std::move(signal1)};
  EXPECT_FALSE(signal1.Connected());
  EXPECT_TRUE(signal2.Connected());
}

TEST(ScopedGSignalTest, MoveAssign) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal1(instance, "some-signal",
                        base::BindRepeating([](GObject* obj) {}));
  EXPECT_TRUE(signal1.Connected());

  ScopedGSignal signal2 = std::move(signal1);
  EXPECT_FALSE(signal1.Connected());
  EXPECT_TRUE(signal2.Connected());
}

TEST(ScopedGSignalTest, SignalHandlerCalled) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  bool signal_fired = false;
  ScopedGSignal signal(
      instance, "some-signal",
      base::BindRepeating([](bool* fired, GObject* obj) { *fired = true; },
                          &signal_fired));
  EXPECT_TRUE(signal.Connected());

  EmitSignal(instance, "some-signal");
  EXPECT_TRUE(signal_fired);
}

TEST(ScopedGSignalTest, InvalidSignal) {
  auto instance = CreateGObject();
  ASSERT_TRUE(instance.get());

  ScopedGSignal signal(instance, "invalid-signal",
                       base::BindRepeating([](GObject* obj) {}));
  EXPECT_FALSE(signal.Connected());
}

}  // namespace ui
