// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/cocoa/nswindow_test_util.h"

namespace ui {
namespace test {

ScopedFakeNSWindowFullscreen::ScopedFakeNSWindowFullscreen() {
  instance_count_ += 1;
  ui::NSWindowFakedForTesting::SetEnabled(instance_count_ > 0);
}

ScopedFakeNSWindowFullscreen::~ScopedFakeNSWindowFullscreen() {
  instance_count_ -= 1;
  ui::NSWindowFakedForTesting::SetEnabled(instance_count_ > 0);
}

// static
int ScopedFakeNSWindowFullscreen::instance_count_ = 0;

}  // namespace test
}  // namespace ui
