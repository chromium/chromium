// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class SelectionRequestorTest : public testing::Test {
 public:
  SelectionRequestorTest()
      : x_display_(gfx::GetXDisplay()), x_window_(x11::None) {}

  ~SelectionRequestorTest() override {}

  // Responds to the SelectionRequestor's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   |value|.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(XAtom selection,
                           XAtom target,
                           const std::string& value) {
    ui::SetStringProperty(x_window_, requestor_->x_property_,
                          gfx::GetAtom("STRING"), value);

    XEvent xev;
    xev.type = SelectionNotify;
    xev.xselection.serial = 0u;
    xev.xselection.display = x_display_;
    xev.xselection.requestor = x_window_;
    xev.xselection.selection = selection;
    xev.xselection.target = target;
    xev.xselection.property = requestor_->x_property_;
    xev.xselection.time = x11::CurrentTime;
    xev.xselection.type = SelectionNotify;
    requestor_->OnSelectionNotify(xev);
  }

 protected:
  void SetUp() override {
    // Make X11 synchronous for our display connection.
    XSynchronize(x_display_, x11::True);

    // Create a window for the selection requestor to use.
    x_window_ = XCreateWindow(x_display_,
                              DefaultRootWindow(x_display_),
                              0, 0, 10, 10,    // x, y, width, height
                              0,               // border width
                              CopyFromParent,  // depth
                              InputOnly,
                              CopyFromParent,  // visual
                              0,
                              nullptr);

    event_source_ = PlatformEventSource::CreateDefault();
    CHECK(PlatformEventSource::GetInstance());
    requestor_ =
        std::make_unique<SelectionRequestor>(x_display_, x_window_, nullptr);
  }

  void TearDown() override {
    requestor_.reset();
    event_source_.reset();
    XDestroyWindow(x_display_, x_window_);
    XSynchronize(x_display_, x11::False);
  }

  Display* x_display_;

  // |requestor_|'s window.
  XID x_window_;

  std::unique_ptr<PlatformEventSource> event_source_;
  std::unique_ptr<SelectionRequestor> requestor_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionRequestorTest);
};

namespace {

// Converts |selection| to |target| and checks the returned values.
void PerformBlockingConvertSelection(SelectionRequestor* requestor,
                                     XAtom selection,
                                     XAtom target,
                                     const std::string& expected_data) {
  scoped_refptr<base::RefCountedMemory> out_data;
  size_t out_data_items = 0u;
  XAtom out_type = x11::None;
  EXPECT_TRUE(requestor->PerformBlockingConvertSelection(
      selection, target, &out_data, &out_data_items, &out_type));
  EXPECT_EQ(expected_data, ui::RefCountedMemoryToString(out_data));
  EXPECT_EQ(expected_data.size(), out_data_items);
  EXPECT_EQ(gfx::GetAtom("STRING"), out_type);
}

}  // namespace

// Test that SelectionRequestor correctly handles receiving a request while it
// is processing another request.
TEST_F(SelectionRequestorTest, NestedRequests) {
  // Assume that |selection| will have no owner. If there is an owner, the owner
  // will set the property passed into the XConvertSelection() request which is
  // undesirable.
  XAtom selection = gfx::GetAtom("FAKE_SELECTION");

  XAtom target1 = gfx::GetAtom("TARGET1");
  XAtom target2 = gfx::GetAtom("TARGET2");

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PerformBlockingConvertSelection,
                                base::Unretained(requestor_.get()), selection,
                                target2, "Data2"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target1, "Data1"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target2, "Data2"));
  PerformBlockingConvertSelection(requestor_.get(), selection, target1,
                                  "Data1");
}

}  // namespace ui
