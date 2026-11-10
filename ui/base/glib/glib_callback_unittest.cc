// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/glib/glib_callback.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

struct DummyPrintJob {};

using TrailingCallback = int (*)(int, const char*, gpointer);
using MiddleCallback = void (*)(DummyPrintJob*, gpointer, const GError*);
using VoidCallback = void (*)(gpointer);

class Receiver {
 public:
  void OnCompleted(DummyPrintJob* job, const GError* error) {
    called_ = true;
    last_job_ = job;
    last_error_ = error;
  }

  bool called() const { return called_; }
  DummyPrintJob* last_job() const { return last_job_; }
  const GError* last_error() const { return last_error_; }
  void ClearPointers() {
    last_job_ = nullptr;
    last_error_ = nullptr;
  }

  base::WeakPtr<Receiver> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }
  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

 private:
  bool called_ = false;
  raw_ptr<DummyPrintJob> last_job_ = nullptr;
  raw_ptr<const GError> last_error_ = nullptr;
  base::WeakPtrFactory<Receiver> weak_factory_{this};
};

}  // namespace

TEST(GlibCallbackTest, TrailingUserData) {
  bool resource_destroyed = false;
  base::ScopedClosureRunner on_destroy(base::BindOnce(
      [](bool* destroyed) { *destroyed = true; }, &resource_destroyed));

  auto [callback, user_data, destroy] = WrapGlibOnceCallback(base::BindOnce(
      [](base::ScopedClosureRunner, int x, const char* s) {
        return x + static_cast<int>(std::string_view(s).size());
      },
      std::move(on_destroy)));

  TrailingCallback c_callback = callback;
  ASSERT_NE(c_callback, nullptr);
  ASSERT_NE(user_data, nullptr);
  ASSERT_NE(destroy, nullptr);
  EXPECT_FALSE(resource_destroyed);

  EXPECT_EQ(c_callback(10, "glib", user_data), 14);
  EXPECT_TRUE(resource_destroyed);

  destroy(user_data);
}

TEST(GlibCallbackTest, MiddleUserData) {
  Receiver receiver;
  DummyPrintJob job;
  GError* error = g_error_new_literal(g_quark_from_static_string("test"), 1,
                                      "print failed");

  auto [callback, user_data, destroy] = WrapGlibOnceCallback(
      base::BindOnce(&Receiver::OnCompleted, receiver.GetWeakPtr()));

  MiddleCallback c_callback = callback;
  c_callback(&job, user_data, error);
  destroy(user_data);

  EXPECT_TRUE(receiver.called());
  EXPECT_EQ(receiver.last_job(), &job);
  EXPECT_EQ(receiver.last_error(), error);

  receiver.ClearPointers();
  g_error_free(error);
}

TEST(GlibCallbackTest, ZeroArgsGSourceFunc) {
  bool invoked = false;
  auto [callback, user_data, destroy] = WrapGlibOnceCallback(base::BindOnce(
      [](bool* ran) -> gboolean {
        *ran = true;
        return G_SOURCE_REMOVE;
      },
      &invoked));

  GSourceFunc source_func = callback;
  EXPECT_EQ(source_func(user_data), G_SOURCE_REMOVE);
  EXPECT_TRUE(invoked);
  destroy(user_data);
}

TEST(GlibCallbackTest, DestroyedWithoutInvocation) {
  bool invoked = false;
  bool resource_destroyed = false;
  base::ScopedClosureRunner on_destroy(base::BindOnce(
      [](bool* destroyed) { *destroyed = true; }, &resource_destroyed));

  auto [callback, user_data, destroy] = WrapGlibOnceCallback(
      base::BindOnce([](base::ScopedClosureRunner, bool* ran) { *ran = true; },
                     std::move(on_destroy), &invoked));

  VoidCallback unused_c_callback = callback;
  EXPECT_NE(unused_c_callback, nullptr);
  EXPECT_FALSE(resource_destroyed);

  // Simulate GLib/GTK cancelling and freeing `user_data` without calling back.
  destroy(user_data);
  EXPECT_FALSE(invoked);
  EXPECT_TRUE(resource_destroyed);
}

TEST(GlibCallbackTest, WeakPtrInvalidatedBeforeInvocation) {
  Receiver receiver;
  DummyPrintJob job;

  auto [callback, user_data, destroy] = WrapGlibOnceCallback(
      base::BindOnce(&Receiver::OnCompleted, receiver.GetWeakPtr()));

  receiver.InvalidateWeakPtrs();

  MiddleCallback c_callback = callback;
  c_callback(&job, user_data, nullptr);
  destroy(user_data);

  EXPECT_FALSE(receiver.called());
}

namespace {

// Mimics `gtk_print_job_send()`: runs the callback, then frees `user_data`.
void FakeSendAndComplete(DummyPrintJob* job,
                         MiddleCallback callback,
                         gpointer user_data,
                         GDestroyNotify destroy) {
  callback(job, user_data, nullptr);
  destroy(user_data);
}

// Mimics a GLib function that discards `user_data` without calling back, and
// returns a value.
int FakeCancel(int value,
               const char* name,
               VoidCallback callback,
               gpointer user_data,
               GDestroyNotify destroy) {
  destroy(user_data);
  return value + static_cast<int>(std::string_view(name).size());
}

}  // namespace

TEST(GlibCallbackTest, CallGlibWithCallbackInvokes) {
  Receiver receiver;
  DummyPrintJob job;

  CallGlibWithCallback(
      &FakeSendAndComplete,
      base::BindOnce(&Receiver::OnCompleted, receiver.GetWeakPtr()), &job);

  EXPECT_TRUE(receiver.called());
  EXPECT_EQ(receiver.last_job(), &job);
  EXPECT_EQ(receiver.last_error(), nullptr);
  receiver.ClearPointers();
}

TEST(GlibCallbackTest, CallGlibWithCallbackDestroysWithoutInvocation) {
  bool invoked = false;
  bool resource_destroyed = false;
  base::ScopedClosureRunner on_destroy(base::BindOnce(
      [](bool* destroyed) { *destroyed = true; }, &resource_destroyed));

  int result = CallGlibWithCallback(
      &FakeCancel,
      base::BindOnce([](base::ScopedClosureRunner, bool* ran) { *ran = true; },
                     std::move(on_destroy), &invoked),
      10, "glib");

  EXPECT_EQ(result, 14);
  EXPECT_FALSE(invoked);
  EXPECT_TRUE(resource_destroyed);
}

}  // namespace ui
