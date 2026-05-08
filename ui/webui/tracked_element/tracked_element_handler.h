// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"

namespace content {
class WebContents;
}

namespace ui {

class TrackedElementWebUI;

// Mojo handler that supports tracking elements in WebUIs.
class TrackedElementHandler
    : public tracked_element::mojom::TrackedElementHandler {
 public:
  TrackedElementHandler(
      content::WebContents* web_contents,
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
          receiver,
      ui::ElementContext context,
      const std::vector<ui::ElementIdentifier>& identifiers);
  ~TrackedElementHandler() override;
  TrackedElementHandler(const TrackedElementHandler&) = delete;
  TrackedElementHandler& operator=(const TrackedElementHandler&) = delete;

  content::WebContents* web_contents() const { return web_contents_; }

  // Asks the WebUI side to change highlighting of the given element.
  void SetHighlightState(const std::string& identifier_name, bool highlight);

  // Flushes the C++ -> WebUI mojo pipe.
  void FlushManagerRemoteForTesting();

  // Interaction simulation methods.
  bool ClickElement(const std::string& identifier_name);
  bool FocusElement(const std::string& identifier_name);
  bool SelectTab(const std::string& identifier_name, size_t index);
  bool SelectDropdownItem(const std::string& identifier_name, size_t index);
  bool EnterText(const std::string& identifier_name,
                 const std::u16string& text,
                 tracked_element::mojom::TextEntryMode mode);
  bool Confirm(const std::string& identifier_name);

  // tracked_element::mojom::TrackedElementHandler:
  void SetManager(
      mojo::PendingRemote<tracked_element::mojom::TrackedElementManager>
          manager) override;
  void TrackedElementVisibilityChanged(const std::string& identifier_name,
                                       bool visible,
                                       const gfx::RectF& rect) override;
  void TrackedElementActivated(const std::string& identifier_name) override;
  void TrackedElementCustomEvent(const std::string& identifier_name,
                                 const std::string& event_name) override;
  void TrackedElementCanHighlightChanged(const std::string& identifier_name,
                                         bool can_highlight) override;

 private:
  TrackedElementWebUI* GetElement(const std::string& identifier_name);

  const ui::ElementContext context_;
  absl::flat_hash_map<std::string, std::unique_ptr<TrackedElementWebUI>>
      elements_;

  const raw_ptr<content::WebContents> web_contents_;
  mojo::Receiver<tracked_element::mojom::TrackedElementHandler> receiver_;
  mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote_;
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
