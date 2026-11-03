// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_SCOPED_INPUT_METHOD_MANAGER_FOR_TESTING_H_
#define UI_BASE_IME_ASH_SCOPED_INPUT_METHOD_MANAGER_FOR_TESTING_H_

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash::input_method {

// Creates a `T` and installs it as the global InputMethodManager for the
// lifetime of this object. Destroying this object shuts the global instance
// down, which deletes the `T`. Tests therefore never call
// InputMethodManager::Initialize() or Shutdown() themselves, and a test that
// exits early (e.g. on a failed ASSERT) cannot leak the instance into later
// tests.
//
// Declare it before the members that use the InputMethodManager, so that it
// outlives them. Hold it in a std::optional if it has to be installed at a
// specific point in SetUp().
template <typename T>
class ScopedInputMethodManagerForTesting {
 public:
  // Forwards `args` to the constructor of `T`.
  template <typename... Args>
  explicit ScopedInputMethodManagerForTesting(Args&&... args) {
    CHECK(!InputMethodManager::Get())
        << "An InputMethodManager is already installed (leaked by an earlier "
           "test?).";
    auto manager = std::make_unique<T>(std::forward<Args>(args)...);
    manager_ = manager.get();
    // The global instance takes ownership; Shutdown() deletes it.
    InputMethodManager::Initialize(manager.release());
  }

  ScopedInputMethodManagerForTesting(
      const ScopedInputMethodManagerForTesting&) = delete;
  ScopedInputMethodManagerForTesting& operator=(
      const ScopedInputMethodManagerForTesting&) = delete;

  ~ScopedInputMethodManagerForTesting() {
    CHECK_EQ(InputMethodManager::Get(), manager_.get())
        << "The global InputMethodManager was shut down or replaced outside "
           "of ScopedInputMethodManagerForTesting.";
    // Clear `manager_` before Shutdown() deletes the instance, so that it
    // never dangles.
    manager_ = nullptr;
    InputMethodManager::Shutdown();
  }

  T* get() const { return manager_; }
  T* operator->() const { return manager_; }

 private:
  raw_ptr<T> manager_;
};

}  // namespace ash::input_method

#endif  // UI_BASE_IME_ASH_SCOPED_INPUT_METHOD_MANAGER_FOR_TESTING_H_
