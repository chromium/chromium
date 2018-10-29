// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/wm_test_helper.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/cpp/input_devices/input_device_client.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/wm_state.h"

namespace wm {

WMTestHelper::WMTestHelper(const gfx::Size& default_window_size,
                           service_manager::Connector* connector,
                           ui::ContextFactory* context_factory) {
  if (context_factory)
    aura::Env::GetInstance()->set_context_factory(context_factory);
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL)
    InitLocalHost(default_window_size);
  else
    InitMusHost(connector, default_window_size);
  aura::client::SetWindowParentingClient(host_->window(), this);

  focus_client_.reset(new aura::test::TestFocusClient);
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  root_window_event_filter_.reset(new wm::CompoundEventFilter);
  host_->window()->AddPreTargetHandler(root_window_event_filter_.get());

  new wm::DefaultActivationClient(host_->window());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));
}

WMTestHelper::~WMTestHelper() {
  if (test_ws_) {
    // Wait for test_ws to shutdown so that its AuraTestHelper is destroyed
    // and there is no lingering WindowTreeHost.
    base::RunLoop run_loop;
    test_ws_->Shutdown(run_loop.QuitClosure());
    run_loop.Run();
  }
  host_->window()->RemovePreTargetHandler(root_window_event_filter_.get());
}

aura::Window* WMTestHelper::GetDefaultParent(aura::Window* window,
                                             const gfx::Rect& bounds) {
  return host_->window();
}

void WMTestHelper::InitLocalHost(const gfx::Size& default_window_size) {
  wm_state_ = std::make_unique<WMState>();
  host_ = aura::WindowTreeHost::Create(
      ui::PlatformWindowInitProperties{gfx::Rect(default_window_size)});
  host_->InitHost();
}

void WMTestHelper::InitMusHost(service_manager::Connector* connector,
                               const gfx::Size& default_window_size) {
  DCHECK(!aura::Env::GetInstance()->HasWindowTreeClient());

  const bool running_in_ws_process = features::IsSingleProcessMash();

  if (!running_in_ws_process) {
    wm_state_ = std::make_unique<WMState>();

    input_device_client_ = std::make_unique<ws::InputDeviceClient>();
    ws::mojom::InputDeviceServerPtr input_device_server;
    connector->BindInterface(ws::mojom::kServiceName, &input_device_server);
    input_device_client_->Connect(std::move(input_device_server));
  }

  property_converter_ = std::make_unique<aura::PropertyConverter>();

  const bool create_discardable_memory = false;
  window_tree_client_ = aura::WindowTreeClient::CreateForWindowTreeFactory(
      connector, this, create_discardable_memory);
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_.get());
  if (running_in_ws_process) {
    // Spin message loop to wait for displays when WindowService runs in the
    // same process to avoid deadlock.
    display_wait_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    display_wait_loop_->Run();
    display_wait_loop_.reset();

    // Bind to test_ws so that it could be shutdown at the right time.
    connector->BindInterface(test_ws::mojom::kServiceName, &test_ws_);
  } else {
    window_tree_client_->WaitForDisplays();
  }

  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ws::mojom::WindowManager::kBounds_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(gfx::Rect(default_window_size));

  auto host_mus = std::make_unique<aura::WindowTreeHostMus>(
      aura::CreateInitParamsForTopLevel(window_tree_client_.get(), properties));
  host_mus->InitHost();

  host_ = std::move(host_mus);
}

void WMTestHelper::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {}

void WMTestHelper::OnUnembed(aura::Window* root) {}

void WMTestHelper::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {}

void WMTestHelper::OnLostConnection(aura::WindowTreeClient* client) {}

aura::PropertyConverter* WMTestHelper::GetPropertyConverter() {
  return property_converter_.get();
}

void WMTestHelper::OnDisplaysChanged(
    std::vector<ws::mojom::WsDisplayPtr> ws_displays,
    int64_t primary_display_id,
    int64_t internal_display_id,
    int64_t display_id_for_new_windows) {
  if (display_wait_loop_)
    display_wait_loop_->Quit();
}

}  // namespace wm
