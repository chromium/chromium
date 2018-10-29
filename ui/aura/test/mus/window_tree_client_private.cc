// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/window_tree_client_private.h"

#include "base/unguessable_token.h"
#include "ui/aura/mus/embed_root.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace aura {
namespace {

constexpr int64_t kDisplayId = 1;

}  // namespace

WindowTreeClientPrivate::WindowTreeClientPrivate(
    WindowTreeClient* tree_client_impl)
    : tree_client_impl_(tree_client_impl) {}

WindowTreeClientPrivate::WindowTreeClientPrivate(Window* window)
    : WindowTreeClientPrivate(WindowPortMus::Get(window)->window_tree_client_) {
}

WindowTreeClientPrivate::~WindowTreeClientPrivate() {}

// static
std::unique_ptr<WindowTreeClient>
WindowTreeClientPrivate::CreateWindowTreeClient(
    WindowTreeClientDelegate* window_tree_delegate) {
  std::unique_ptr<WindowTreeClient> wtc(new WindowTreeClient(
      nullptr, window_tree_delegate, nullptr, nullptr, false));
  return wtc;
}

void WindowTreeClientPrivate::OnEmbed(ws::mojom::WindowTree* window_tree) {
  const ws::Id focused_window_id = 0;
  tree_client_impl_->OnEmbedImpl(window_tree, CreateWindowDataForEmbed(),
                                 kDisplayId, focused_window_id, true,
                                 base::nullopt);
}

void WindowTreeClientPrivate::CallOnObservedInputEvent(
    std::unique_ptr<ui::Event> event) {
  tree_client_impl_->OnObservedInputEvent(std::move(event));
}

void WindowTreeClientPrivate::CallOnCaptureChanged(Window* new_capture,
                                                   Window* old_capture) {
  tree_client_impl_->OnCaptureChanged(
      new_capture ? WindowPortMus::Get(new_capture)->server_id() : 0,
      old_capture ? WindowPortMus::Get(old_capture)->server_id() : 0);
}

void WindowTreeClientPrivate::CallOnEmbedFromToken(EmbedRoot* embed_root) {
  embed_root->OnScheduledEmbedForExistingClient(
      base::UnguessableToken::Create());
  tree_client_impl_->OnEmbedFromToken(embed_root->token(),
                                      CreateWindowDataForEmbed(), kDisplayId,
                                      base::Optional<viz::LocalSurfaceId>());
}

void WindowTreeClientPrivate::SetTree(ws::mojom::WindowTree* window_tree) {
  tree_client_impl_->WindowTreeConnectionEstablished(window_tree);
}

bool WindowTreeClientPrivate::HasEventObserver() {
  return !tree_client_impl_->event_type_to_observer_count_.empty();
}

Window* WindowTreeClientPrivate::GetWindowByServerId(ws::Id id) {
  WindowMus* window = tree_client_impl_->GetWindowByServerId(id);
  return window ? window->GetWindow() : nullptr;
}

WindowMus* WindowTreeClientPrivate::NewWindowFromWindowData(
    WindowMus* parent,
    const ws::mojom::WindowData& window_data) {
  return tree_client_impl_->NewWindowFromWindowData(parent, window_data);
}

bool WindowTreeClientPrivate::HasInFlightChanges() {
  return !tree_client_impl_->in_flight_map_.empty();
}

bool WindowTreeClientPrivate::HasChangeInFlightOfType(ChangeType type) {
  for (auto& pair : tree_client_impl_->in_flight_map_) {
    if (pair.second->change_type() == type)
      return true;
  }
  return false;
}

void WindowTreeClientPrivate::FlushForTesting() {
  tree_client_impl_->binding_.FlushForTesting();
}

ws::mojom::WindowDataPtr WindowTreeClientPrivate::CreateWindowDataForEmbed() {
  ws::mojom::WindowDataPtr root_data(ws::mojom::WindowData::New());
  root_data->parent_id = 0;
  // OnEmbed() is passed windows the client doesn't own. Use a |client_id| of 1
  // to mirror what the server does for the client-id portion, and use the
  // number of roots for the window id. The important part is the combination is
  // unique to the client.
  const ws::Id client_id = 1;
  root_data->window_id =
      (client_id << 32) | tree_client_impl_->GetRoots().size();
  root_data->visible = true;
  return root_data;
}

}  // namespace aura
