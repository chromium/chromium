// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"

#include <memory>
#include <optional>
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

  ui::ElementContext context() { return context_getter_.Run(); }
  const std::vector<ui::ElementIdentifier>& identifiers() const {
    return identifiers_;
  }

 private:
  friend class content::WebContentsUserData<TrackedElementHandlerConfig>;
  TrackedElementHandlerConfig(content::WebContents* web_contents,
                              ContextGetter context_getter,
                              std::vector<ui::ElementIdentifier> identifiers)
      : content::WebContentsUserData<TrackedElementHandlerConfig>(
            *web_contents),
        context_getter_(std::move(context_getter)),
        identifiers_(std::move(identifiers)) {}
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  ContextGetter context_getter_;
  const std::vector<ui::ElementIdentifier> identifiers_;
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
  explicit TrackedElementHandlerUserData(content::RenderFrameHost* rfh,
                                         TrackedElementHandlerConfig* config)
      : content::DocumentUserData<TrackedElementHandlerUserData>(rfh) {
    CHECK(config);
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    CHECK(web_contents);
    handler_ = std::make_unique<TrackedElementHandler>(
        web_contents, config->context(), config->identifiers());
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

  auto* user_data = TrackedElementHandlerUserData::GetForCurrentDocument(rfh);
  if (!user_data) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents) {
      return nullptr;
    }

    TrackedElementHandlerConfig* config =
        TrackedElementHandlerConfig::FromWebContents(web_contents);
    if (!config) {
      return nullptr;
    }

    TrackedElementHandlerUserData::CreateForCurrentDocument(rfh, config);
    user_data = TrackedElementHandlerUserData::GetForCurrentDocument(rfh);
  }
  CHECK(user_data);
  return user_data->handler() ? user_data->handler()->GetWeakPtr() : nullptr;
}

}  // namespace ui
