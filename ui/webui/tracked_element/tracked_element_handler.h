// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
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
class WebUIController;
}

namespace user_education {
class HelpBubbleHandlerBase;
}  // namespace user_education

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/view_tracker.h"

namespace views {
class WebView;
}
#endif

namespace ui {

class InteractionTestUtilSimulatorWebUI;
class TrackedElementWebUI;

// Mojo handler that supports tracking elements in WebUIs.
class TrackedElementHandler
    : public tracked_element::mojom::TrackedElementHandler,
      public content::WebContentsObserver {
 public:
  explicit TrackedElementHandler(content::WebUIController* controller);
  TrackedElementHandler(content::WebContents* web_contents,
                        ui::ElementContext context);
  ~TrackedElementHandler() override;
  TrackedElementHandler(const TrackedElementHandler&) = delete;
  TrackedElementHandler& operator=(const TrackedElementHandler&) = delete;

  // Convenience constructor; infers context from `controller` and registers
  // `identifiers`.
  template <typename T>
    requires std::ranges::input_range<T>
  TrackedElementHandler(content::WebUIController* controller,
                        const T& identifiers)
      : TrackedElementHandler(controller) {
    RegisterIdentifiers(identifiers);
  }

  // Convenience constructor; specifies `context` and registers `identifiers`.
  template <typename T>
    requires std::ranges::input_range<T>
  TrackedElementHandler(content::WebContents* web_contents,
                        ui::ElementContext context,
                        const T& identifiers)
      : TrackedElementHandler(web_contents, context) {
    RegisterIdentifiers(identifiers);
  }

  // Register the given identifiers by adding them to the set of known ids.
  template <typename T>
    requires std::ranges::input_range<T>
  void RegisterIdentifiers(const T& identifiers) {
    for (auto id : identifiers) {
      RegisterIdentifier(id);
    }
  }

  base::WeakPtr<TrackedElementHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool is_web_contents_visible() const { return is_web_contents_visible_; }
  user_education::HelpBubbleHandlerBase* GetHelpBubbleHandler() const {
    return help_bubble_handler_.get();
  }
  void set_help_bubble_handler(
      base::WeakPtr<user_education::HelpBubbleHandlerBase>
          help_bubble_handler) {
    help_bubble_handler_ = help_bubble_handler;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Returns the host WebView for this WebUI, if any. Note that for backgrounded
  // tabs, there will not be a WebView, and this should only be used if you
  // absolutely need the view itself (e.g. for screenshotting or anchoring).
  views::WebView* GetWebView() const;
  void SetWebViewForTesting(views::WebView* web_view);
#endif

  void BindInterface(
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
          receiver);

  // Asks the WebUI side to change highlighting of the given element.
  void SetHighlightState(TrackedElementWebUI& element,
                         bool highlight,
                         base::PassKey<TrackedElementWebUI>);

  std::vector<std::string> GetIdentifiers();
  ui::ElementContext context() { return context_; }

  // Flushes the C++ -> WebUI mojo pipe.
  void FlushManagerRemoteForTesting();

  // Interaction simulation methods.
  bool ClickElement(TrackedElementWebUI& element,
                    tracked_element::mojom::InputType input_type,
                    base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return ClickElement(element, input_type);
  }
  bool FocusElement(TrackedElementWebUI& element,
                    base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return FocusElement(element);
  }
  bool SelectTab(TrackedElementWebUI& element,
                 size_t index,
                 base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return SelectTab(element, index);
  }
  bool SelectDropdownItem(TrackedElementWebUI& element,
                          size_t index,
                          base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return SelectDropdownItem(element, index);
  }
  bool EnterText(TrackedElementWebUI& element,
                 const std::u16string& text,
                 tracked_element::mojom::TextEntryMode mode,
                 base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return EnterText(element, text, mode);
  }
  bool Confirm(TrackedElementWebUI& element,
               base::PassKey<InteractionTestUtilSimulatorWebUI>) {
    return Confirm(element);
  }

  // tracked_element::mojom::TrackedElementHandler:
  void SetManager(
      mojo::PendingRemote<tracked_element::mojom::TrackedElementManager>
          manager) override;
  void TrackedElementVisibilityChanged(
      tracked_element::mojom::TrackedElementIdentifierPtr id,
      bool visible,
      const gfx::RectF& rect) override;
  void TrackedElementActivated(
      tracked_element::mojom::TrackedElementIdentifierPtr id) override;
  void TrackedElementCustomEvent(
      tracked_element::mojom::TrackedElementIdentifierPtr id,
      const std::string& event_name) override;
  void TrackedElementCanHighlightChanged(
      tracked_element::mojom::TrackedElementIdentifierPtr id,
      bool can_highlight) override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility new_visibility) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TrackedElementHandlerTest, Interaction);

  // Interaction simulation methods.
  bool ClickElement(TrackedElementWebUI& element,
                    tracked_element::mojom::InputType input_type =
                        tracked_element::mojom::InputType::kDontCare);
  bool FocusElement(TrackedElementWebUI& element);
  bool SelectTab(TrackedElementWebUI& element, size_t index);
  bool SelectDropdownItem(TrackedElementWebUI& element, size_t index);
  bool EnterText(TrackedElementWebUI& element,
                 const std::u16string& text,
                 tracked_element::mojom::TextEntryMode mode);
  bool Confirm(TrackedElementWebUI& element);

  void UpdateAllEffectiveVisibilities();
  void RegisterIdentifier(ui::ElementIdentifier id);
  TrackedElementWebUI* GetElement(
      const tracked_element::mojom::TrackedElementIdentifierPtr& id,
      bool create_if_not_present = false);

  void ReportBadMessage(
      std::string_view description,
      const tracked_element::mojom::TrackedElementIdentifierPtr& id);

  const ui::ElementContext context_;
  template <typename K, typename V>
  using MapType = absl::flat_hash_map<K, V>;
  using SecondaryIdentifierMapType =
      MapType<std::string, std::unique_ptr<TrackedElementWebUI>>;
  MapType<std::string, SecondaryIdentifierMapType> elements_;

  bool is_web_contents_visible_ = false;
  base::WeakPtr<user_education::HelpBubbleHandlerBase> help_bubble_handler_;
#if !BUILDFLAG(IS_ANDROID)
  // Caches the host WebView once located (or set in tests). Marked mutable so
  // it can be lazily populated in the const GetWebView() method, while using
  // ViewTracker to safely handle WebView destruction without dangling pointers.
  mutable views::ViewTracker web_view_tracker_;
#endif
  mojo::Receiver<tracked_element::mojom::TrackedElementHandler> receiver_;
  mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote_;

  base::WeakPtrFactory<TrackedElementHandler> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_HANDLER_H_
