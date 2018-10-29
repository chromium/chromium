// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/remote_view/remote_view_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"

namespace views {

RemoteViewHost::RemoteViewHost()
    : embedding_root_(std::make_unique<aura::Window>(nullptr)) {
  embedding_root_->set_owned_by_parent(false);
  embedding_root_->SetName("RemoteViewHostWindow");
  embedding_root_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  embedding_root_->Init(ui::LAYER_NOT_DRAWN);
}

RemoteViewHost::~RemoteViewHost() = default;

void RemoteViewHost::EmbedUsingToken(const base::UnguessableToken& embed_token,
                                     int embed_flags,
                                     EmbedCallback callback) {
  // Only works with mus.
  DCHECK_EQ(aura::Env::Mode::MUS, aura::Env::GetInstance()->mode());

  embed_token_ = embed_token;
  embed_flags_ = embed_flags;
  embed_callback_ = std::move(callback);

  if (GetWidget())
    EmbedImpl();
}

void RemoteViewHost::EmbedImpl() {
  DCHECK(IsEmbedPending());
  aura::WindowPortMus::Get(embedding_root_.get())
      ->EmbedUsingToken(embed_token_, embed_flags_,
                        base::BindOnce(&RemoteViewHost::OnEmbedResult,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void RemoteViewHost::OnEmbedResult(bool success) {
  LOG_IF(ERROR, !success) << "Failed to embed, token=" << embed_token_;
  embed_token_ = {};
  if (embed_callback_)
    std::move(embed_callback_).Run(success);
}

void RemoteViewHost::AddedToWidget() {
  if (native_view())
    return;
  Attach(embedding_root_.get());
  if (IsEmbedPending())
    EmbedImpl();
}

}  // namespace views
