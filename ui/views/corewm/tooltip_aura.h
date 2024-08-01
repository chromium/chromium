// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_AURA_H_
#define UI_VIEWS_COREWM_TOOLTIP_AURA_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_view_aura.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/tooltip_observer.h"

namespace gfx {
class RenderText;
class Size;
}  // namespace gfx

namespace ui {
struct AXNodeData;
struct OwnedWindowAnchor;
}  // namespace ui

namespace views {

class Widget;

namespace corewm {
namespace test {
class TooltipAuraTestApi;
}

// Implementation of Tooltip that shows the tooltip using a Widget and Label.
class VIEWS_EXPORT TooltipAura : public Tooltip, public WidgetObserver {
 public:
  static const char kWidgetName[];
  // TODO(crbug.com/40254494): get cursor offset from actual cursor size.
  static constexpr int kCursorOffsetX = 10;
  static constexpr int kCursorOffsetY = 15;

  using TooltipViewFactory =
      base::RepeatingCallback<std::unique_ptr<TooltipViewAura>(void)>;

  TooltipAura();

  explicit TooltipAura(const TooltipViewFactory& tooltip_view_factory);

  TooltipAura(const TooltipAura&) = delete;
  TooltipAura& operator=(const TooltipAura&) = delete;

  ~TooltipAura() override;

  void AddObserver(wm::TooltipObserver* observer) override;
  void RemoveObserver(wm::TooltipObserver* observer) override;
  void SetMaxWidth(int width) override;

  // Adjusts `anchor_point` to the bottom left of the cursor.
  static void AdjustToCursor(gfx::Rect* anchor_point);

 private:
  class TooltipWidget;

  friend class test::TooltipAuraTestApi;
  const gfx::RenderText* GetRenderTextForTest() const;
  void GetAccessibleNodeDataForTest(ui::AXNodeData* node_data);

  // Adjusts the bounds given by the arguments to fit inside the desktop
  // and returns the adjusted bounds, and also sets anchor information to
  // `anchor`.
  // `anchor_point` is an absolute position, not relative to the window.
  gfx::Rect GetTooltipBounds(const gfx::Size& tooltip_size,
                             const gfx::Point& anchor_point,
                             const TooltipTrigger trigger,
                             ui::OwnedWindowAnchor* anchor);

  // Sets |widget_| to a new instance of TooltipWidget. Additional information
  // that helps to position anchored windows in such backends as Wayland.
  void CreateTooltipWidget(const gfx::Rect& bounds,
                           const ui::OwnedWindowAnchor& anchor);

  // Destroys |widget_|.
  void DestroyWidget();

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override;
  void Update(aura::Window* window,
              const std::u16string& tooltip_text,
              const gfx::Point& position,
              const TooltipTrigger trigger) override;
  void Show() override;
  void Hide() override;
  bool IsVisible() override;

  // A callback to generate a `TooltipViewAura` instance.
  const TooltipViewFactory tooltip_view_factory_;

  // The widget containing the tooltip. May be NULL.
  std::unique_ptr<TooltipWidget> widget_;

  // The window we're showing the tooltip for. Never NULL and valid while
  // showing.
  raw_ptr<aura::Window> tooltip_window_ = nullptr;

  int max_width_ = kTooltipMaxWidth;

  // Observes tooltip state change.
  base::ObserverList<wm::TooltipObserver> observers_;
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_AURA_H_
