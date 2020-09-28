// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_workspace_handler.h"

#include "base/strings/string_number_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

x11::Future<x11::GetPropertyReply> GetWorkspace() {
  auto* connection = x11::Connection::Get();
  return connection->GetProperty({
      .window = connection->default_screen().root,
      .property = static_cast<x11::Atom>(gfx::GetAtom("_NET_CURRENT_DESKTOP")),
      .type = static_cast<x11::Atom>(gfx::GetAtom("CARDINAL")),
      .long_length = 1,
  });
}

}  // namespace

X11WorkspaceHandler::X11WorkspaceHandler(Delegate* delegate)
    : x_root_window_(ui::GetX11RootWindow()), delegate_(delegate) {
  DCHECK(delegate_);
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

  x_root_window_events_ = std::make_unique<ui::XScopedEventSelector>(
      x_root_window_, x11::EventMask::PropertyChange);
}

X11WorkspaceHandler::~X11WorkspaceHandler() {
  if (ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

std::string X11WorkspaceHandler::GetCurrentWorkspace() {
  if (workspace_.empty())
    OnWorkspaceResponse(GetWorkspace().Sync());
  return workspace_;
}

bool X11WorkspaceHandler::DispatchXEvent(x11::Event* xev) {
  auto* prop = xev->As<x11::PropertyNotifyEvent>();
  if (prop && prop->window == x_root_window_ &&
      prop->atom == gfx::GetAtom("_NET_CURRENT_DESKTOP")) {
    GetWorkspace().OnResponse(base::BindOnce(
        &X11WorkspaceHandler::OnWorkspaceResponse, weak_factory_.GetWeakPtr()));
    return true;
  }

  return false;
}

void X11WorkspaceHandler::OnWorkspaceResponse(
    x11::GetPropertyResponse response) {
  if (!response || response->format != 32 || response->value->size() < 4)
    return;
  DCHECK_EQ(response->bytes_after, 0U);
  DCHECK_EQ(response->type, static_cast<x11::Atom>(gfx::GetAtom("CARDINAL")));

  uint32_t workspace;
  memcpy(&workspace, response->value->data(), 4);
  workspace_ = base::NumberToString(workspace);
  delegate_->OnCurrentWorkspaceChanged(workspace_);
}

}  // namespace ui
