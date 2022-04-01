// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class ProxyHelpersTest : public testing::Test {
 public:
  void SetUp() override {
    drm_thread_ = std::make_unique<base::Thread>("drm_thread");
    drm_thread_->Start();
  }

  void TearDown() override {
    drm_thread_->Stop();
    drm_thread_ = nullptr;
  }

  // QuitFunction runs on the DRM thread.
  void QuitFunction(int a) {
    EXPECT_TRUE(drm_checker_.CalledOnValidThread());

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyHelpersTest::QuitFunctionCallback,
                                  base::Unretained(this), 8));
  }

  // QuitFunctionCallback runs on the main thread.
  void QuitFunctionCallback(int a) {
    EXPECT_TRUE(main_checker_.CalledOnValidThread());

    auto quitter = run_loop_.QuitWhenIdleClosure();
    task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE, quitter);
  }

  void SetDrmChecker() { drm_checker_.DetachFromThread(); }

  void MoveType(int a,
                base::OnceCallback<void(std::unique_ptr<int>)> callback) {
    EXPECT_TRUE(drm_checker_.CalledOnValidThread());

    std::unique_ptr<int> p(new int);
    *p = a + 1;

    std::move(callback).Run(std::move(p));
  }

  void MoveTypeCallback(std::unique_ptr<int> p) {
    EXPECT_TRUE(main_checker_.CalledOnValidThread());

    *p = *p + 1;
    move_type_.swap(p);
    EXPECT_EQ(*p, 50);
  }

  void ValueType(int a, base::OnceCallback<void(int)> callback) {
    EXPECT_TRUE(drm_checker_.CalledOnValidThread());

    std::move(callback).Run(a + 1);
  }

  void ValueTypeCallback(int a) {
    EXPECT_TRUE(main_checker_.CalledOnValidThread());

    value_type_ = a + 1;
  }

  void StringType(std::string a,
                  base::OnceCallback<void(std::string)> callback) {
    EXPECT_TRUE(drm_checker_.CalledOnValidThread());
    std::move(callback).Run(a.append("e"));
  }

  void StringTypeCallback(std::string a) {
    EXPECT_TRUE(main_checker_.CalledOnValidThread());
    derived_string_ = a.append("r");
  }

 protected:
  // Main thread message loop.
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;

  // Thread to simulate the drm thread in ozone viz process.
  std::unique_ptr<base::Thread> drm_thread_;

  base::ThreadChecker main_checker_;
  base::ThreadChecker drm_checker_;

  // Variables to record operation.
  int value_type_ = 0;
  std::unique_ptr<int> move_type_;
  std::string original_string_;
  std::string derived_string_;
};

TEST_F(ProxyHelpersTest, PostTask) {
  // Binds the thread checker on the drm thread.
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyHelpersTest::SetDrmChecker, base::Unretained(this)));

  // Test passing a type by value.
  auto value_callback = base::BindOnce(&ProxyHelpersTest::ValueTypeCallback,
                                       base::Unretained(this));
  auto safe_value_callback = CreateSafeOnceCallback(std::move(value_callback));

  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyHelpersTest::ValueType, base::Unretained(this), 100,
                     std::move(safe_value_callback)));

  // Test passing a move-only type.
  move_type_ = std::make_unique<int>(50);

  auto move_callback = base::BindOnce(&ProxyHelpersTest::MoveTypeCallback,
                                      base::Unretained(this));
  auto safe_move_callback = CreateSafeOnceCallback(std::move(move_callback));

  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyHelpersTest::MoveType, base::Unretained(this), 100,
                     std::move(safe_move_callback)));

  // Test that passing a type that supports both move and value semantics
  // defaults to value.
  original_string_ = "This is a string";

  auto string_callback = base::BindOnce(&ProxyHelpersTest::StringTypeCallback,
                                        base::Unretained(this));
  auto safe_string_callback =
      CreateSafeOnceCallback(std::move(string_callback));

  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyHelpersTest::StringType, base::Unretained(this),
                     original_string_, std::move(safe_string_callback)));

  // Shutdown the RunLoop.
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyHelpersTest::QuitFunction,
                                base::Unretained(this), 42));

  run_loop_.Run();

  EXPECT_EQ(value_type_, 102);
  EXPECT_EQ(*move_type_, 102);
  EXPECT_TRUE(original_string_ == "This is a string");
  EXPECT_TRUE(derived_string_ == "This is a stringer");
}
}  // namespace ui
