// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_TOOLTIP_ICON_H_
#define UI_VIEWS_BUBBLE_TOOLTIP_ICON_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class InfoBubble;

// A tooltip icon that shows a bubble on hover. Looks like (i).
class VIEWS_EXPORT TooltipIcon : public ImageView,
                                 public MouseWatcherListener,
                                 public WidgetObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when tooltip bubble of the TooltipIcon is shown.
    virtual void OnTooltipBubbleShown(TooltipIcon* icon) = 0;

    // Called when the TooltipIcon is being destroyed.
    virtual void OnTooltipIconDestroying(TooltipIcon* icon) = 0;
  };

  METADATA_HEADER(TooltipIcon);

  explicit TooltipIcon(const std::u16string& tooltip,
                       int tooltip_icon_size = 16);

  TooltipIcon(const TooltipIcon&) = delete;
  TooltipIcon& operator=(const TooltipIcon&) = delete;

  ~TooltipIcon() override;

  // ImageView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override;

  void set_bubble_width(int preferred_width) {
    preferred_width_ = preferred_width;
  }

  void set_anchor_point_arrow(BubbleBorder::Arrow arrow) {
    anchor_point_arrow_ = arrow;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Changes the color to reflect the hover node_data.
  void SetDrawAsHovered(bool hovered);

  // Creates and shows |bubble_|. If |bubble_| already exists, just cancels a
  // potential close timer.
  void ShowBubble();

  // Hides |bubble_| if necessary.
  void HideBubble();

  // The text to show in a bubble when hovered.
  std::u16string tooltip_;

  // The size of the tooltip icon, in dip.
  // Must be set in the constructor, otherwise the pre-hovered icon will show
  // the default size.
  int tooltip_icon_size_;

  // The point at which to anchor the tooltip.
  BubbleBorder::Arrow anchor_point_arrow_ = BubbleBorder::TOP_RIGHT;

  // Whether the mouse is inside this tooltip.
  bool mouse_inside_ = false;

  // A bubble shown on hover. Weak; owns itself. NULL while hiding.
  raw_ptr<InfoBubble> bubble_;

  // The width the tooltip prefers to be. Default is 0 (no preference).
  int preferred_width_ = 0;

  // A timer to delay showing |bubble_|.
  base::OneShotTimer show_timer_;

  // A watcher that keeps |bubble_| open if the user's mouse enters it.
  std::unique_ptr<MouseWatcher> mouse_watcher_;

  base::ScopedObservation<Widget, WidgetObserver> observation_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_TOOLTIP_ICON_H_
