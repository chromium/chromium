// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_DOCUMENT_SINGLETON_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_DOCUMENT_SINGLETON_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class RenderFrameHost;
class WebUIController;
}  // namespace content

namespace ui {

class TrackedElementHandler;

// Provides a way to register and retrieve a TrackedElementHandler for a given
// document. This is a singleton wrapper that hides the implementation details
// of how the handler is stored and configured.
class TrackedElementHandlerDocumentSingleton {
 public:
  using ContextGetter = base::RepeatingCallback<ui::ElementContext(void)>;
  // Registers the configuration for TrackedElementHandlers in a WebContents.
  // This should be called by the WebUIController during its construction.
  // If `context_getter` is_null, the `controller` is used as the context.
  static void Register(content::WebUIController* controller,
                       std::vector<ui::ElementIdentifier> identifiers,
                       ContextGetter context_getter = {});

  // Returns the TrackedElementHandler for the given `rfh`, creating it if
  // necessary based on the registration info from `Register`.
  // Returns an empty WeakPtr if no configuration was registered for the
  // WebContents or if the handler could not be created.
  static base::WeakPtr<TrackedElementHandler> GetOrCreate(
      content::RenderFrameHost* rfh);
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_DOCUMENT_SINGLETON_H_
