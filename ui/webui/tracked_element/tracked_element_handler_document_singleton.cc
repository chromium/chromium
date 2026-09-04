// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"

#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"

namespace ui {

namespace {

using ContextGetter = TrackedElementHandlerDocumentSingleton::ContextGetter;

void PostResult(
    base::OnceCallback<void(base::WeakPtr<TrackedElementHandler>)> callback,
    base::WeakPtr<TrackedElementHandler> result) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(result)));
}

// Holds the configuration for TrackedElementHandlers in a WebContents.
class TrackedElementHandlerConfig
    : public content::WebContentsUserData<TrackedElementHandlerConfig> {
 public:
  ~TrackedElementHandlerConfig() override = default;

  ui::ElementContext context() const { return context_getter_.Run(); }
  const auto& identifiers() const { return identifiers_; }

  template <typename T>
    requires std::ranges::input_range<T>
  void AddIdentifiers(const T& identifiers) {
    std::ranges::copy(identifiers,
                      std::inserter(identifiers_, identifiers_.end()));
  }

 private:
  friend class content::WebContentsUserData<TrackedElementHandlerConfig>;
  TrackedElementHandlerConfig(content::WebContents* web_contents,
                              ContextGetter context_getter,
                              std::vector<ui::ElementIdentifier> identifiers)
      : content::WebContentsUserData<TrackedElementHandlerConfig>(
            *web_contents),
        context_getter_(std::move(context_getter)) {
    AddIdentifiers(identifiers);
  }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  ContextGetter context_getter_;
  std::set<ui::ElementIdentifier> identifiers_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrackedElementHandlerConfig);

// User data that owns a TrackedElementHandler for a particular document.
class TrackedElementHandlerUserData
    : public content::DocumentUserData<TrackedElementHandlerUserData> {
 public:
  ~TrackedElementHandlerUserData() override = default;

  TrackedElementHandler* handler() { return handler_.get(); }

  void SetOrUpdateConfig(content::WebContents* web_contents,
                         const TrackedElementHandlerConfig* config) {
    CHECK(config);
    CHECK(web_contents);
    if (!handler_) {
      handler_ = std::make_unique<TrackedElementHandler>(web_contents,
                                                         config->context());
    }

    // This is called also when re-registering the config, to make sure the
    // identifier list is current. If this is not done then adding a second
    // component which uses identifier could be ignored if another component got
    // here first.
    handler_->RegisterIdentifiers(config->identifiers());
  }

  void AddCallback(
      base::OnceCallback<void(base::WeakPtr<TrackedElementHandler>)> callback) {
    callbacks_.push_back(std::move(callback));
  }

  void RunPendingCallbacks() {
    auto result = handler_->GetWeakPtr();

    for (auto& callback : callbacks_) {
      PostResult(std::move(callback), result);
    }
    callbacks_.clear();
  }

 private:
  friend class content::DocumentUserData<TrackedElementHandlerUserData>;
  explicit TrackedElementHandlerUserData(content::RenderFrameHost* rfh)
      : content::DocumentUserData<TrackedElementHandlerUserData>(rfh) {}

  DOCUMENT_USER_DATA_KEY_DECL();

  std::unique_ptr<TrackedElementHandler> handler_;
  std::vector<base::OnceCallback<void(base::WeakPtr<TrackedElementHandler>)>>
      callbacks_;
};

DOCUMENT_USER_DATA_KEY_IMPL(TrackedElementHandlerUserData);

}  // namespace

// static
void TrackedElementHandlerDocumentSingleton::Register(
    content::WebUIController* controller,
    std::vector<ui::ElementIdentifier> identifiers,
    ContextGetter maybe_context_getter) {
  if (!controller || !controller->web_ui()) {
    return;
  }

  content::WebContents* web_contents = controller->web_ui()->GetWebContents();
  if (!web_contents) {
    return;
  }

  // If the config already exists, add the identifiers. This avoids a case where
  // multiple components register different identifiers they want to use as
  // anchors. If this isn't done, different components could overwrite each
  // others' identifier lists.
  if (auto* const config =
          TrackedElementHandlerConfig::FromWebContents(web_contents)) {
    config->AddIdentifiers(identifiers);
    return;
  }

  if (!maybe_context_getter) {
    ui::ElementContext context = ui::ElementContext(
        controller, base::PassKey<TrackedElementHandlerDocumentSingleton>());
    maybe_context_getter = base::BindRepeating(
        [](ui::ElementContext context) { return context; }, std::move(context));
  }
  TrackedElementHandlerConfig::CreateForWebContents(
      web_contents, std::move(maybe_context_getter), std::move(identifiers));
  const TrackedElementHandlerConfig* const config =
      TrackedElementHandlerConfig::FromWebContents(web_contents);

  // Activate any pending GetOrCreateAsync() callbacks, now we have the config.
  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    auto* user_data = TrackedElementHandlerUserData::GetForCurrentDocument(rfh);
    if (user_data) {
      user_data->SetOrUpdateConfig(web_contents, config);
      user_data->RunPendingCallbacks();
    }
  });
}

// static
base::WeakPtr<TrackedElementHandler>
TrackedElementHandlerDocumentSingleton::GetOrCreate(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return nullptr;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return nullptr;
  }

  const TrackedElementHandlerConfig* const config =
      TrackedElementHandlerConfig::FromWebContents(web_contents);
  if (!config) {
    return nullptr;
  }

  auto* user_data =
      TrackedElementHandlerUserData::GetOrCreateForCurrentDocument(rfh);
  user_data->SetOrUpdateConfig(web_contents, config);

  return user_data->handler()->GetWeakPtr();
}

// static
void TrackedElementHandlerDocumentSingleton::GetOrCreateAsync(
    content::RenderFrameHost* rfh,
    base::OnceCallback<void(base::WeakPtr<TrackedElementHandler>)> callback) {
  if (!rfh) {
    PostResult(std::move(callback), nullptr);
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    PostResult(std::move(callback), nullptr);
    return;
  }

  auto* user_data =
      TrackedElementHandlerUserData::GetOrCreateForCurrentDocument(rfh);

  const TrackedElementHandlerConfig* const config =
      TrackedElementHandlerConfig::FromWebContents(web_contents);
  if (!config) {
    // Defer until ::Register is called.
    user_data->AddCallback(std::move(callback));
    return;
  }

  user_data->SetOrUpdateConfig(web_contents, config);
  PostResult(std::move(callback), user_data->handler()->GetWeakPtr());
}

}  // namespace ui
