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

 private:
  friend class content::DocumentUserData<TrackedElementHandlerUserData>;
  explicit TrackedElementHandlerUserData(
      content::RenderFrameHost* rfh,
      const TrackedElementHandlerConfig* config)
      : content::DocumentUserData<TrackedElementHandlerUserData>(rfh) {
    CHECK(config);
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    CHECK(web_contents);
    handler_ = std::make_unique<TrackedElementHandler>(web_contents,
                                                       config->context());
    handler_->RegisterIdentifiers(config->identifiers());
  }

  DOCUMENT_USER_DATA_KEY_DECL();

  std::unique_ptr<TrackedElementHandler> handler_;
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
  const TrackedElementHandlerConfig* const config =
      web_contents ? TrackedElementHandlerConfig::FromWebContents(web_contents)
                   : nullptr;
  auto* user_data = TrackedElementHandlerUserData::GetForCurrentDocument(rfh);
  if (user_data) {
    // Re-registering the config, so make sure the identifier list is current.
    // If this is not done then adding a second component which uses identifiers
    // could be ignored if another component got here first.
    if (config) {
      user_data->handler()->RegisterIdentifiers(config->identifiers());
    }
  } else if (!web_contents || !config) {
    return nullptr;
  } else {
    TrackedElementHandlerUserData::CreateForCurrentDocument(rfh, config);
    user_data = TrackedElementHandlerUserData::GetForCurrentDocument(rfh);
  }
  CHECK(user_data);
  return user_data->handler() ? user_data->handler()->GetWeakPtr() : nullptr;
}

}  // namespace ui
