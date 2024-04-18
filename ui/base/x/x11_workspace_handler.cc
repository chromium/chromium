// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_workspace_handler.h"

#include "base/strings/string_number_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

x11::Future<x11::GetPropertyReply> GetWorkspace() {
  auto* connection = x11::Connection::Get();
  return connection->GetProperty({
      .window = connection->default_screen().root,
      .property = static_cast<x11::Atom>(x11::GetAtom("_NET_CURRENT_DESKTOP")),
      .type = static_cast<x11::Atom>(x11::Atom::CARDINAL),
      .long_length = 1,
  });
}

}  // namespace

X11WorkspaceHandler::X11WorkspaceHandler(Delegate* delegate)
    : x_root_window_(ui::GetX11RootWindow()), delegate_(delegate) {
  DCHECK(delegate_);
  auto* connection = x11::Connection::Get();
  connection->AddEventObserver(this);

  x_root_window_events_ = connection->ScopedSelectEvent(
      x_root_window_, x11::EventMask::PropertyChange);
}

X11WorkspaceHandler::~X11WorkspaceHandler() {
  x11::Connection::Get()->RemoveEventObserver(this);
}

std::string X11WorkspaceHandler::GetCurrentWorkspace() {
  if (workspace_.empty()) {
    OnWorkspaceResponse(GetWorkspace().Sync());
  }
  return workspace_;
}

void X11WorkspaceHandler::OnEvent(const x11::Event& xev) {
  auto* prop = xev.As<x11::PropertyNotifyEvent>();
  if (prop && prop->window == x_root_window_ &&
      prop->atom == x11::GetAtom("_NET_CURRENT_DESKTOP")) {
    GetWorkspace().OnResponse(base::BindOnce(
        &X11WorkspaceHandler::OnWorkspaceResponse, weak_factory_.GetWeakPtr()));
  }
}

void X11WorkspaceHandler::OnWorkspaceResponse(
    x11::GetPropertyResponse response) {
  if (!response || response->format != 32 || response->value_len < 1) {
    return;
  }
  DCHECK_EQ(response->bytes_after, 0U);
  DCHECK_EQ(response->type, static_cast<x11::Atom>(x11::Atom::CARDINAL));

  uint32_t workspace;
  memcpy(&workspace, response->value->bytes(), 4);
  workspace_ = base::NumberToString(workspace);
  delegate_->OnCurrentWorkspaceChanged(workspace_);
}

}  // namespace ui
