// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

class Label;
class TabbedPaneTab;
class TabbedPaneListener;
class TabbedPaneTabStrip;

namespace test {
class TabbedPaneAccessibilityMacTest;
class TabbedPaneWithWidgetTest;
}  // namespace test

// TabbedPane is a view that shows tabs. When the user clicks on a tab, the
// associated view is displayed.
// Support for horizontal-highlight and vertical-border modes is limited and
// may require additional polish.
class VIEWS_EXPORT TabbedPane : public FlexLayoutView {
  METADATA_HEADER(TabbedPane, FlexLayoutView)

 public:
  // The orientation of the tab alignment.
  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  // The style of the tab strip.
  enum class TabStripStyle {
    kBorder,           // Draw border around the selected tab.
    kHighlight,        // Highlight background and text of the selected tab.
    kCompactWithIcon,  // Draw an icon, shrink the highlight bar to icon+text
    kWithIcon,         // Draw an icon, expand the highlight bar to entire tab.
  };

  explicit TabbedPane(Orientation orientation = Orientation::kHorizontal,
                      TabStripStyle style = TabStripStyle::kBorder,
                      bool scrollable = false);

  TabbedPane(const TabbedPane&) = delete;
  TabbedPane& operator=(const TabbedPane&) = delete;

  ~TabbedPane() override;

  TabbedPaneListener* GetListener() const;
  void SetListener(TabbedPaneListener* listener);

  // Returns the index of the currently selected tab, or
  // TabStrip::kNoSelectedTab if no tab is selected.
  size_t GetSelectedTabIndex() const;

  // Returns the number of tabs.
  size_t GetTabCount() const;

  // Adds a new tab at the end of this TabbedPane with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane.
  template <typename T>
  T* AddTab(const std::u16string& title,
            std::unique_ptr<T> contents,
            const gfx::VectorIcon* tab_icon = nullptr) {
    return AddTabAtIndex(GetTabCount(), title, std::move(contents), tab_icon);
  }

  // Adds a new tab at |index| with |title|. |contents| is the view displayed
  // when the tab is selected and is owned by the TabbedPane. If the tabbed pane
  // is currently empty, the new tab is selected.
  template <typename T>
  T* AddTabAtIndex(size_t index,
                   const std::u16string& title,
                   std::unique_ptr<T> contents,
                   const gfx::VectorIcon* tab_icon = nullptr) {
    T* result = contents.get();
    AddTabInternal(index, title, std::move(contents), tab_icon);
    return result;
  }

  // Selects the tab at |index|, which must be valid.
  void SelectTabAt(size_t index, bool animate = true);

  // Selects |tab| (the tabstrip view, not its content) if it is valid.
  void SetTabContentVisibility(size_t tab_index, bool visible);

  // Calls the FocusManager (if it exists), and gives focus to the view at
  // |tab_index|.
  void MaybeSetFocusedView(size_t tab_index);

  // Gets the scroll view containing the tab strip, if it exists
  ScrollView* GetScrollView();

  // Gets the orientation of the tab alignment.
  Orientation GetOrientation() const;

  // Gets the style of the tab strip.
  TabStripStyle GetStyle() const;

  // Returns the tab at the given index.
  TabbedPaneTab* GetTabAt(size_t index);

  // Returns the View associated with a specific tab.
  const views::View* GetTabContents(size_t index) const;
  views::View* GetTabContentsForTesting(size_t index);

  // Updates the view's accessible name based on the currently selected tab's
  // title.
  void UpdateAccessibleName();

  // Sets whether a divider will be drawn underneath the Tab Strip.
  void SetDrawTabDivider(bool draw);

 private:
  friend class FocusTraversalTest;
  friend class TabbedPaneTab;
  friend class TabbedPaneTabStrip;
  friend class test::TabbedPaneWithWidgetTest;
  friend class test::TabbedPaneAccessibilityMacTest;

  // Adds a new tab at |index| with |title|. |contents| is the view displayed
  // when the tab is selected and is owned by the TabbedPane. If the tabbed pane
  // is currently empty, the new tab is selected.
  void AddTabInternal(size_t index,
                      const std::u16string& title,
                      std::unique_ptr<View> contents,
                      const gfx::VectorIcon* tab_icon = nullptr);

  // Get the TabbedPaneTab (the tabstrip view, not its content) at the selected
  // index.
  TabbedPaneTab* GetSelectedTab();

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;

  // The tab strip and contents container. The child indices of these members
  // correspond to match each TabbedPaneTab with its respective content View.
  raw_ptr<TabbedPaneTabStrip> tab_strip_ = nullptr;
  raw_ptr<View> contents_ = nullptr;

  // The scroll view containing the tab strip, if |scrollable| is specified on
  // creation.
  raw_ptr<ScrollView> scroll_view_ = nullptr;
};

// The tab view shown in the tab strip.
class VIEWS_EXPORT TabbedPaneTab : public View {
  METADATA_HEADER(TabbedPaneTab, View)

 public:
  static constexpr int kDefaultIconSize = 16;
  static constexpr int kDefaultTitleLeftMargin = kDefaultIconSize / 2;
  static constexpr int kDefaultHorizontalTabHeight = 32;
  static constexpr int kMinimumVerticalTabWidth = 192;

  TabbedPaneTab(TabbedPaneTabStrip* tab_strip,
                const std::u16string& title,
                const gfx::VectorIcon* tab_icon);

  TabbedPaneTab(const TabbedPaneTab&) = delete;
  TabbedPaneTab& operator=(const TabbedPaneTab&) = delete;

  ~TabbedPaneTab() override;

  bool selected() const { return selected_; }
  void SetSelected(bool selected);

  std::u16string_view GetTitleText() const;
  void SetTitleText(std::u16string_view text);

  void SetTitleMargin(const gfx::Insets& margin);
  void SetIconMargin(const gfx::Insets& margin);
  void SetTabOutsets(const gfx::Outsets& outsets);
  void SetHeight(int height);

  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnThemeChanged() override;

  void UpdateEnabledColor(bool enabled);

 private:
  enum class State {
    kInactive,
    kActive,
    kHovered,
  };

  void SetState(State state);

  // Called whenever |state_| changes.
  void OnStateChanged();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  void UpdatePreferredTitleWidth();
  void UpdateTitleColor();

  void UpdateIconColor();

  void UpdateAccessibleName();
  void UpdateAccessibleSelection();

  ui::ImageModel GetImageModelForTab(ui::ColorId color_id) const;
  ui::ColorId GetIconTitleColor() const;

  raw_ptr<TabbedPaneTabStrip> tab_strip_;
  raw_ptr<const gfx::VectorIcon> icon_for_tab_;
  raw_ptr<ImageView> icon_view_ = nullptr;
  raw_ptr<Label> title_ = nullptr;
  gfx::Outsets tab_outsets_ = gfx::Outsets::VH(0, 0);
  // The preferred title width is the maximum width between inactive and active
  // states (font changes). See UpdatePreferredTitleWidth() for more details.
  int preferred_title_width_;
  int height_ = kDefaultHorizontalTabHeight;
  State state_ = State::kActive;
  bool selected_ = false;

  base::CallbackListSubscription title_text_changed_callback_;
};

// The tab strip shown above/left of the tab contents.
class VIEWS_EXPORT TabbedPaneTabStrip : public View,
                                        public gfx::AnimationDelegate {
  METADATA_HEADER(TabbedPaneTabStrip, View)

 public:
  // The return value of GetSelectedTabIndex() when no tab is selected.
  static constexpr size_t kNoSelectedTab = static_cast<size_t>(-1);

  TabbedPaneTabStrip(TabbedPane::Orientation orientation,
                     TabbedPane::TabStripStyle style,
                     raw_ptr<TabbedPane> tabbed_pane);

  TabbedPaneTabStrip(const TabbedPaneTabStrip&) = delete;
  TabbedPaneTabStrip& operator=(const TabbedPaneTabStrip&) = delete;

  ~TabbedPaneTabStrip() override;

  TabbedPaneListener* listener() { return listener_; }
  void set_listener(TabbedPaneListener* listener) { listener_ = listener; }

  // Adds a new TabbedPaneTab as a child of this View. This method should only
  // be used when TabbedPaneTabStrip is instantiated as a standalone component.
  TabbedPaneTab* AddTab(const std::u16string& title,
                        const gfx::VectorIcon* tab_icon);
  TabbedPaneTab* AddTabAt(const std::u16string& title,
                          const gfx::VectorIcon* tab_icon,
                          size_t index);

  // AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Called by TabbedPaneTabStrip when the selected tab changes. This function
  // is only called if |from_tab| is not null, i.e., there was a previously
  // selected tab.
  void OnSelectedTabChanged(TabbedPaneTab* from_tab,
                            TabbedPaneTab* to_tab,
                            bool animate = true);

  // Attempts to select the provided tab. Returns true if the new tab was
  // selected, or false if no work was done (i.e., |new_selected_tab| is the
  // currently selected tab).
  bool SelectTab(TabbedPaneTab* new_selected_tab, bool animate = true);

  // Updates the visibility of the content associated with the tab at
  // |tab_index| if there is a |tabbed_pane_| parented to this view.
  void MaybeUpdateTabContentVisibility(size_t tab_index, bool visible);

  // Dispatches an accessibility event to the parent |tabbed_pane_| if it
  // exists, otherwise the event is dispatched on behalf of this View.
  void NotifyNewAccessibilityEvent(ax::mojom::Event event_type,
                                   bool send_native_event);

  // Moves the selection by |delta| tabs, where negative delta means leftwards
  // and positive delta means rightwards. Returns whether the move was
  // performed. This only fails if the delta results in currently selected tab.
  bool MoveSelectionBy(int delta);

  TabbedPaneTab* GetSelectedTab() const;
  TabbedPaneTab* GetTabAtDeltaFromSelected(int delta) const;
  TabbedPaneTab* GetTabAtIndex(size_t index) const;
  size_t GetIndexForTab(TabbedPaneTab* index) const;
  size_t GetSelectedTabIndex() const;
  size_t GetTabCount() const;

  // Sets the default flex of the tab strip. Useful for adding custom padding
  // instead of expecting the tab strip to stretch across its parent container.
  void SetDefaultFlex(int flex);

  // Sets how far apart the tabs will be positioned.
  void SetTabSpacing(int spacing);

  TabbedPane::Orientation GetOrientation() const;
  TabbedPane::TabStripStyle GetStyle() const;

  // Returns whether an Icon should be rendered for the TabbedPaneTab children.
  bool HasIconStyle() const;

  // Updates its own accessible name, and also calls TabbedPane's
  // UpdateAccessibleName method if |tabbed_pane_| is defined.
  void UpdateAccessibleName();

  // Sets whether a divider will be drawn underneath the Tab Strip.
  void SetDrawTabDivider(bool draw);

 protected:
  // View:
  void OnPaintBorder(gfx::Canvas* canvas) override;

 private:
  struct Coordinates {
    int start, end;
  };

  // Returns the beginning and ending distances for the icon+label in a tab.
  // start is the distance from the origin to the left-side of the icon,
  // and end is the distance from the origin to the right-side of the text.
  //                    (x) Label
  // -------start-------^       ^
  // -------end-----------------^
  Coordinates GetIconLabelStartEndingX(TabbedPaneTab* tab);

  // view::View
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // The orientation of the tab alignment.
  const TabbedPane::Orientation orientation_;

  // The style of the tab strip.
  const TabbedPane::TabStripStyle style_;

  // Animations for expanding and contracting the selection bar. When changing
  // selections, the selection bar first grows to encompass both the old and new
  // selections, then shrinks to encompass only the new selection. The rates of
  // expansion and contraction each follow the cubic bezier curves used in
  // gfx::Tween; see TabStrip::OnPaintBorder for details.
  std::unique_ptr<gfx::LinearAnimation> expand_animation_ =
      std::make_unique<gfx::LinearAnimation>(this);
  std::unique_ptr<gfx::LinearAnimation> contract_animation_ =
      std::make_unique<gfx::LinearAnimation>(this);

  // The x-coordinate ranges of the old selection and the new selection.
  Coordinates animating_from_;
  Coordinates animating_to_;

  // A listener notified when tab selection changes. Weak, not owned.
  raw_ptr<TabbedPaneListener> listener_ = nullptr;

  // An optional parent container which connects Tabs in the TabStrip to
  // content views.
  raw_ptr<TabbedPane> tabbed_pane_;

  // Whether to draw the unselected divider below the tabs. Useful for when
  // the caller wants to use a custom divider instead.
  bool draw_tab_divider_ = true;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, TabbedPane, FlexLayoutView)
VIEW_BUILDER_METHOD_ALIAS(AddTab,
                          AddTab<View>,
                          const std::u16string&,
                          std::unique_ptr<View>,
                          const gfx::VectorIcon*)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, TabbedPane)

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
