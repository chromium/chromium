// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
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

namespace user_education {
class HelpBubbleHandlerBase;
}  // namespace user_education

namespace ui {

class TrackedElementWebUI;
class TrackedElementVisibilityLock;

// Mojo handler that supports tracking elements in WebUIs.
class TrackedElementHandler
    : public tracked_element::mojom::TrackedElementHandler,
      public content::WebContentsObserver {
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

  bool is_web_contents_visible() const { return is_web_contents_visible_; }
  user_education::HelpBubbleHandlerBase* help_bubble_handler() const {
    return help_bubble_handler_;
  }
  void set_help_bubble_helper(
      user_education::HelpBubbleHandlerBase* help_bubble_handler) {
    help_bubble_handler_ = help_bubble_handler;
  }

  // Asks the WebUI side to change highlighting of the given element.
  void SetHighlightState(const std::string& identifier_name, bool highlight);

  // Returns a visibility lock for the given element.
  std::unique_ptr<TrackedElementVisibilityLock> LockVisible(
      const std::string& identifier_name);

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

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility new_visibility) override;

 private:
  void UpdateAllEffectiveVisibilities();
  TrackedElementWebUI* GetElement(const std::string& identifier_name);

  const ui::ElementContext context_;
  absl::flat_hash_map<std::string, std::unique_ptr<TrackedElementWebUI>>
      elements_;

  bool is_web_contents_visible_ = false;
  raw_ptr<user_education::HelpBubbleHandlerBase> help_bubble_handler_;
  mojo::Receiver<tracked_element::mojom::TrackedElementHandler> receiver_;
  mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote_;

  base::WeakPtrFactory<TrackedElementHandler> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
