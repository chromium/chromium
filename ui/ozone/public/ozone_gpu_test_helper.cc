// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_gpu_test_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {
const int kGpuProcessHostId = 1;
}  // namespace

class FakeGpuConnection {
 public:
  FakeGpuConnection() = default;
  ~FakeGpuConnection() = default;

  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) {
    mojo::GenericPendingReceiver receiver =
        mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe));
    CHECK(binders_.TryBind(&receiver))
        << "Unable to find mojo interface " << interface_name;
  }

  void Init() {
    ui::OzonePlatform::GetInstance()->AddInterfaces(&binders_);
    auto interface_binder = base::BindRepeating(
        &FakeGpuConnection::BindInterface, base::Unretained(this));
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnGpuServiceLaunched(kGpuProcessHostId, interface_binder,
                               base::DoNothing());
  }

 private:
  mojo::BinderMap binders_;
};

OzoneGpuTestHelper::OzoneGpuTestHelper() {
}

OzoneGpuTestHelper::~OzoneGpuTestHelper() {
}

bool OzoneGpuTestHelper::Initialize() {
  fake_gpu_connection_ = std::make_unique<FakeGpuConnection>();
  fake_gpu_connection_->Init();

  return true;
}

}  // namespace ui
