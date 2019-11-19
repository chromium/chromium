// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/window_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/demo/demo_window.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {
namespace {

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

const char kWindowSize[] = "window-size";

}  // namespace

WindowManager::WindowManager(std::unique_ptr<RendererFactory> renderer_factory,
                             base::OnceClosure quit_closure)
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()),
      quit_closure_(std::move(quit_closure)),
      renderer_factory_(std::move(renderer_factory)) {
  if (!renderer_factory_->Initialize())
    LOG(FATAL) << "Failed to initialize renderer factory";

  if (delegate_) {
    delegate_->AddObserver(this);
    delegate_->Initialize();
    OnConfigurationChanged();
  } else {
    LOG(WARNING) << "No display delegate; falling back to test window";
    int width = kTestWindowWidth;
    int height = kTestWindowHeight;
    sscanf(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(kWindowSize)
               .c_str(),
           "%dx%d", &width, &height);

    DemoWindow* window = new DemoWindow(this, renderer_factory_.get(),
                                        gfx::Rect(gfx::Size(width, height)));
    window->Start();
  }
}

WindowManager::~WindowManager() {
  if (delegate_)
    delegate_->RemoveObserver(this);
}

void WindowManager::Quit() {
  std::move(quit_closure_).Run();
}

void WindowManager::OnConfigurationChanged() {
  if (is_configuring_) {
    should_configure_ = true;
    return;
  }

  is_configuring_ = true;
  delegate_->GetDisplays(base::BindRepeating(&WindowManager::OnDisplaysAquired,
                                             base::Unretained(this)));
}

void WindowManager::OnDisplaySnapshotsInvalidated() {}

void WindowManager::OnDisplaysAquired(
    const std::vector<display::DisplaySnapshot*>& displays) {
  windows_.clear();

  gfx::Point origin;
  for (auto* display : displays) {
    if (!display->native_mode()) {
      LOG(ERROR) << "Display " << display->display_id()
                 << " doesn't have a native mode";
      continue;
    }

    delegate_->Configure(
        *display, display->native_mode(), origin,
        base::BindRepeating(&WindowManager::OnDisplayConfigured,
                            base::Unretained(this),
                            gfx::Rect(origin, display->native_mode()->size())));
    origin.Offset(display->native_mode()->size().width(), 0);
  }
  is_configuring_ = false;

  if (should_configure_) {
    should_configure_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindRepeating(&WindowManager::OnConfigurationChanged,
                                       base::Unretained(this)));
  }
}

void WindowManager::OnDisplayConfigured(const gfx::Rect& bounds, bool success) {
  if (success) {
    std::unique_ptr<DemoWindow> window(
        new DemoWindow(this, renderer_factory_.get(), bounds));
    window->Start();
    windows_.push_back(std::move(window));
  } else {
    LOG(ERROR) << "Failed to configure display at " << bounds.ToString();
  }
}

}  // namespace ui
