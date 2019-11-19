// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_view.h"

namespace message_center {

namespace {

// Animation duration for FADE_IN and FADE_OUT.
constexpr base::TimeDelta kFadeInFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);

// Animation duration for MOVE_DOWN.
constexpr base::TimeDelta kMoveDownDuration =
    base::TimeDelta::FromMilliseconds(120);

}  // namespace

MessagePopupCollection::MessagePopupCollection()
    : animation_(std::make_unique<gfx::LinearAnimation>(this)),
      weak_ptr_factory_(this) {
  MessageCenter::Get()->AddObserver(this);
}

MessagePopupCollection::~MessagePopupCollection() {
  for (const auto& item : popup_items_)
    item.popup->Close();
  MessageCenter::Get()->RemoveObserver(this);
}

void MessagePopupCollection::Update() {
  if (is_updating_)
    return;
  base::AutoReset<bool> reset(&is_updating_, true);

  RemoveClosedPopupItems();

  if (MessageCenter::Get()->IsMessageCenterVisible()) {
    CloseAllPopupsNow();
    return;
  }

  if (animation_->is_animating()) {
    UpdateByAnimation();
    return;
  }

  if (state_ != State::IDLE)
    TransitionFromAnimation();

  if (state_ == State::IDLE)
    TransitionToAnimation();

  UpdatePopupTimers();

  if (state_ != State::IDLE) {
    // If not in IDLE state, start animation.
    animation_->SetDuration(state_ == State::MOVE_DOWN ||
                                    state_ == State::MOVE_UP_FOR_INVERSE
                                ? kMoveDownDuration
                                : kFadeInFadeOutDuration);
    animation_->Start();
    UpdateByAnimation();
  }

  DCHECK(state_ == State::IDLE || animation_->is_animating());
}

void MessagePopupCollection::ResetBounds() {
  if (is_updating_)
    return;
  {
    base::AutoReset<bool> reset(&is_updating_, true);

    RemoveClosedPopupItems();
    ResetHotMode();
    state_ = State::IDLE;
    animation_->End();

    CalculateBounds();

    // Remove popups that are no longer in work area.
    ClosePopupsOutsideWorkArea();

    // Reset bounds and opacity of popups.
    for (auto& item : popup_items_) {
      item.popup->SetPopupBounds(item.bounds);
      item.popup->SetOpacity(1.0);
    }
  }

  // Restart animation for FADE_OUT.
  Update();
}

void MessagePopupCollection::NotifyPopupResized() {
  resize_requested_ = true;
  Update();
}

void MessagePopupCollection::NotifyPopupClosed(MessagePopupView* popup) {
  for (auto& item : popup_items_) {
    if (item.popup == popup)
      item.popup = nullptr;
  }
}

void MessagePopupCollection::OnNotificationAdded(
    const std::string& notification_id) {
  // Should not call MessagePopupCollection::Update here. Because notification
  // may be removed before animation which is triggered by the previous
  // operation on MessagePopupCollection ends. As result, when a new
  // notification with the same ID is created, calling
  // MessagePopupCollection::Update will not update the popup's content. Then
  // the new notification popup fails to show. (see https://crbug.com/921402)
  OnNotificationUpdated(notification_id);
}

void MessagePopupCollection::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  Update();
}

void MessagePopupCollection::OnNotificationUpdated(
    const std::string& notification_id) {
  if (is_updating_)
    return;

  // Find Notification object with |notification_id|.
  const auto& notifications = MessageCenter::Get()->GetPopupNotifications();
  auto it = notifications.begin();
  while (it != notifications.end()) {
    if ((*it)->id() == notification_id)
      break;
    ++it;
  }

  if (it == notifications.end()) {
    // If not found, probably |notification_id| is removed from popups by
    // timeout.
    Update();
    return;
  }

  {
    base::AutoReset<bool> reset(&is_updating_, true);

    RemoveClosedPopupItems();

    // Update contents of the notification.
    for (const auto& item : popup_items_) {
      if (item.id == notification_id) {
        item.popup->UpdateContents(**it);
        break;
      }
    }
  }

  Update();
}

void MessagePopupCollection::OnCenterVisibilityChanged(Visibility visibility) {
  Update();
}

void MessagePopupCollection::OnBlockingStateChanged(
    NotificationBlocker* blocker) {
  Update();
}

void MessagePopupCollection::AnimationEnded(const gfx::Animation* animation) {
  Update();
}

void MessagePopupCollection::AnimationProgressed(
    const gfx::Animation* animation) {
  Update();
}

void MessagePopupCollection::AnimationCanceled(
    const gfx::Animation* animation) {
  Update();
}

MessagePopupView* MessagePopupCollection::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  for (const auto& item : popup_items_) {
    if (item.id == notification_id)
      return item.popup;
  }
  return nullptr;
}

MessagePopupView* MessagePopupCollection::CreatePopup(
    const Notification& notification) {
  return new MessagePopupView(notification, this);
}

void MessagePopupCollection::RestartPopupTimers() {
  MessageCenter::Get()->RestartPopupTimers();
}

void MessagePopupCollection::PausePopupTimers() {
  MessageCenter::Get()->PausePopupTimers();
}

void MessagePopupCollection::TransitionFromAnimation() {
  DCHECK_NE(state_, State::IDLE);
  DCHECK(!animation_->is_animating());

  // The animation of type |state_| is now finished.
  UpdateByAnimation();

  // If FADE_OUT animation is finished, remove the animated popup.
  if (state_ == State::FADE_OUT)
    CloseAnimatingPopups();

  if (state_ == State::FADE_IN || state_ == State::MOVE_DOWN ||
      (state_ == State::FADE_OUT && popup_items_.empty())) {
    // If the animation is finished, transition to IDLE.
    state_ = State::IDLE;
  } else if (state_ == State::FADE_OUT && !popup_items_.empty()) {
    if ((HasAddedPopup() && CollapseAllPopups()) || !inverse_) {
      // If FADE_OUT animation is finished and we still have remaining popups,
      // we have to MOVE_DOWN them.
      // If we're going to add a new popup after this MOVE_DOWN, do the collapse
      // animation at the same time. Otherwise it will take another MOVE_DOWN.
      state_ = State::MOVE_DOWN;
      MoveDownPopups();
    } else {
      // If there's no collapsable popups and |inverse_| is on, there's nothing
      // to do after FADE_OUT.
      state_ = State::IDLE;
    }
  } else if (state_ == State::MOVE_UP_FOR_INVERSE) {
    for (auto& item : popup_items_)
      item.is_animating = item.will_fade_in;
    state_ = State::FADE_IN;
  }
}

void MessagePopupCollection::TransitionToAnimation() {
  DCHECK_EQ(state_, State::IDLE);
  DCHECK(!animation_->is_animating());

  if (HasRemovedPopup()) {
    MarkRemovedPopup();

    // Start hot mode to allow a user to continually close many notifications.
    StartHotMode();

    if (CloseTransparentPopups()) {
      // If the popup is already transparent, skip FADE_OUT.
      state_ = State::MOVE_DOWN;
      MoveDownPopups();
    } else {
      state_ = State::FADE_OUT;
    }
    return;
  }

  if (HasAddedPopup()) {
    if (CollapseAllPopups()) {
      // If we had existing popups that weren't collapsed, first show collapsing
      // animation.
      state_ = State::MOVE_DOWN;
      MoveDownPopups();
      return;
    } else if (AddPopup()) {
      // A popup is actually added.
      if (inverse_ && popup_items_.size() > 1) {
        // If |inverse_| is on and there are existing notifications that have to
        // be moved up (existing ones + new one, so > 1), transition to
        // MOVE_UP_FOR_INVERSE.
        state_ = State::MOVE_UP_FOR_INVERSE;
      } else {
        // Show FADE_IN animation.
        state_ = State::FADE_IN;
      }
      return;
    }
  }

  if (resize_requested_) {
    // Resize is requested e.g. a user manually expanded notification.
    resize_requested_ = false;
    state_ = State::MOVE_DOWN;
    MoveDownPopups();

    // This function may be called by a child MessageView when a notification is
    // expanded by the user.  Deleting the pop-up should be delayed so we are
    // out of the child view's call stack. See crbug.com/957033.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MessagePopupCollection::ClosePopupsOutsideWorkArea,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!IsAnyPopupHovered() && is_hot_) {
    // Reset hot mode and animate to the normal positions.
    state_ = State::MOVE_DOWN;
    ResetHotMode();
    MoveDownPopups();
  }
}

void MessagePopupCollection::UpdatePopupTimers() {
  if (state_ == State::IDLE) {
    if (IsAnyPopupHovered() || IsAnyPopupActive()) {
      // If any popup is hovered or activated, pause popup timer.
      PausePopupTimers();
    } else {
      // If in IDLE state, restart popup timer.
      RestartPopupTimers();
    }
  } else {
    // If not in IDLE state, pause popup timer.
    PausePopupTimers();
  }
}

void MessagePopupCollection::CalculateBounds() {
  int base = GetBaseline();
  for (size_t i = 0; i < popup_items_.size(); ++i) {
    gfx::Size preferred_size(
        kNotificationWidth,
        GetPopupItem(i)->popup->GetHeightForWidth(kNotificationWidth));

    // Align the top of i-th popup to |hot_top_|.
    if (is_hot_ && hot_index_ == i) {
      base = hot_top_;
      if (!IsTopDown())
        base += preferred_size.height();
    }

    int origin_x = GetToastOriginX(gfx::Rect(preferred_size));

    int origin_y = base;
    if (!IsTopDown())
      origin_y -= preferred_size.height();

    GetPopupItem(i)->start_bounds = GetPopupItem(i)->bounds;
    GetPopupItem(i)->bounds =
        gfx::Rect(gfx::Point(origin_x, origin_y), preferred_size);

    const int delta = preferred_size.height() + kMarginBetweenPopups;
    if (IsTopDown())
      base += delta;
    else
      base -= delta;
  }
}

void MessagePopupCollection::UpdateByAnimation() {
  DCHECK_NE(state_, State::IDLE);

  for (auto& item : popup_items_) {
    if (!item.is_animating)
      continue;

    double value = gfx::Tween::CalculateValue(
        state_ == State::FADE_OUT ? gfx::Tween::EASE_IN : gfx::Tween::EASE_OUT,
        animation_->GetCurrentValue());

    if (state_ == State::FADE_IN)
      item.popup->SetOpacity(gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f));
    else if (state_ == State::FADE_OUT)
      item.popup->SetOpacity(gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f));

    if (state_ == State::FADE_IN || state_ == State::MOVE_DOWN ||
        state_ == State::MOVE_UP_FOR_INVERSE) {
      item.popup->SetPopupBounds(
          gfx::Tween::RectValueBetween(value, item.start_bounds, item.bounds));
    }
  }
}

bool MessagePopupCollection::AddPopup() {
  std::set<std::string> existing_ids;
  for (const auto& item : popup_items_)
    existing_ids.insert(item.id);

  auto notifications = MessageCenter::Get()->GetPopupNotifications();
  Notification* new_notification = nullptr;
  // Reverse iterating because notifications are in reverse chronological order.
  for (auto it = notifications.rbegin(); it != notifications.rend(); ++it) {
    // Disables popup of custom notification on non-primary displays, since
    // currently custom notification supports only on one display at the same
    // time.
    // TODO(yoshiki): Support custom popup notification on multiple display
    // (https://crbug.com/715370).
    if (!IsPrimaryDisplayForNotification() &&
        (*it)->type() == NOTIFICATION_TYPE_CUSTOM) {
      continue;
    }

    if (!existing_ids.count((*it)->id())) {
      new_notification = *it;
      break;
    }
  }

  if (!new_notification)
    return false;

  // Reset animation flags of existing popups.
  for (auto& item : popup_items_) {
    item.is_animating = false;
    item.will_fade_in = false;
  }

  {
    PopupItem item;
    item.id = new_notification->id();
    item.is_animating = true;
    item.popup = CreatePopup(*new_notification);

    if (IsNextEdgeOutsideWorkArea(item)) {
      item.popup->Close();
      return false;
    }

    item.popup->Show();
    popup_items_.push_back(item);
  }

  // There are existing notifications that have to be moved up (existing ones +
  // new one, so > 1).
  if (inverse_ && popup_items_.size() > 1) {
    for (auto& item : popup_items_) {
      item.will_fade_in = item.is_animating;
      item.is_animating = !item.is_animating;
    }
  }

  MessageCenter::Get()->DisplayedNotification(new_notification->id(),
                                              DISPLAY_SOURCE_POPUP);

  CalculateBounds();

  auto& item = popup_items_.back();
  item.start_bounds = item.bounds;
  item.start_bounds +=
      gfx::Vector2d((IsFromLeft() ? -1 : 1) * item.bounds.width(), 0);
  return true;
}

void MessagePopupCollection::MarkRemovedPopup() {
  std::set<std::string> existing_ids;
  for (Notification* notification :
       MessageCenter::Get()->GetPopupNotifications()) {
    existing_ids.insert(notification->id());
  }

  for (auto& item : popup_items_)
    item.is_animating = !existing_ids.count(item.id);
}

void MessagePopupCollection::MoveDownPopups() {
  CalculateBounds();
  for (auto& item : popup_items_)
    item.is_animating = true;
}

int MessagePopupCollection::GetNextEdge(const PopupItem& item) const {
  const int delta =
      item.popup->GetHeightForWidth(kNotificationWidth) + kMarginBetweenPopups;

  int base = 0;
  if (popup_items_.empty()) {
    base = GetBaseline();
  } else if (inverse_) {
    base = IsTopDown() ? popup_items_.front().bounds.bottom()
                       : popup_items_.front().bounds.y();
  } else {
    base = IsTopDown() ? popup_items_.back().bounds.bottom()
                       : popup_items_.back().bounds.y();
  }

  return IsTopDown() ? base + delta : base - delta;
}

bool MessagePopupCollection::IsNextEdgeOutsideWorkArea(
    const PopupItem& item) const {
  const int next_edge = GetNextEdge(item);
  const gfx::Rect work_area = GetWorkArea();
  return IsTopDown() ? next_edge > work_area.bottom()
                     : next_edge < work_area.y();
}

void MessagePopupCollection::StartHotMode() {
  for (size_t i = 0; i < popup_items_.size(); ++i) {
    if (GetPopupItem(i)->is_animating && GetPopupItem(i)->popup->is_hovered()) {
      is_hot_ = true;
      hot_index_ = i;
      hot_top_ = GetPopupItem(i)->bounds.y();
      break;
    }
  }
}

void MessagePopupCollection::ResetHotMode() {
  is_hot_ = false;
  hot_index_ = 0;
  hot_top_ = 0;
}

void MessagePopupCollection::CloseAnimatingPopups() {
  for (auto& item : popup_items_) {
    if (!item.is_animating)
      continue;
    item.popup->Close();
  }
  RemoveClosedPopupItems();
}

bool MessagePopupCollection::CloseTransparentPopups() {
  bool removed = false;
  for (auto& item : popup_items_) {
    if (item.popup->GetOpacity() > 0.0)
      continue;
    item.popup->Close();
    removed = true;
  }
  RemoveClosedPopupItems();
  return removed;
}

void MessagePopupCollection::ClosePopupsOutsideWorkArea() {
  const gfx::Rect work_area = GetWorkArea();
  for (auto& item : popup_items_) {
    if (work_area.Contains(item.bounds))
      continue;
    item.popup->Close();
  }
  RemoveClosedPopupItems();
}

void MessagePopupCollection::RemoveClosedPopupItems() {
  base::EraseIf(popup_items_, [](const auto& item) { return !item.popup; });
}

void MessagePopupCollection::CloseAllPopupsNow() {
  for (auto& item : popup_items_)
    item.is_animating = true;
  CloseAnimatingPopups();

  ResetHotMode();
  state_ = State::IDLE;
  animation_->End();
}

bool MessagePopupCollection::CollapseAllPopups() {
  bool changed = false;
  for (auto& item : popup_items_) {
    int old_height = item.popup->GetHeightForWidth(kNotificationWidth);

    item.popup->AutoCollapse();

    int new_height = item.popup->GetHeightForWidth(kNotificationWidth);
    if (old_height != new_height)
      changed = true;
  }

  resize_requested_ = false;
  return changed;
}

bool MessagePopupCollection::HasAddedPopup() const {
  std::set<std::string> existing_ids;
  for (const auto& item : popup_items_)
    existing_ids.insert(item.id);

  for (Notification* notification :
       MessageCenter::Get()->GetPopupNotifications()) {
    if (!existing_ids.count(notification->id()))
      return true;
  }
  return false;
}

bool MessagePopupCollection::HasRemovedPopup() const {
  std::set<std::string> existing_ids;
  for (Notification* notification :
       MessageCenter::Get()->GetPopupNotifications()) {
    existing_ids.insert(notification->id());
  }

  for (const auto& item : popup_items_) {
    if (!existing_ids.count(item.id))
      return true;
  }
  return false;
}

bool MessagePopupCollection::IsAnyPopupHovered() const {
  for (const auto& item : popup_items_) {
    if (item.popup->is_hovered())
      return true;
  }
  return false;
}

bool MessagePopupCollection::IsAnyPopupActive() const {
  for (const auto& item : popup_items_) {
    if (item.popup->is_active())
      return true;
  }
  return false;
}

MessagePopupCollection::PopupItem* MessagePopupCollection::GetPopupItem(
    size_t index_from_top) {
  DCHECK_LT(index_from_top, popup_items_.size());
  return &popup_items_[inverse_ ? popup_items_.size() - index_from_top - 1
                                : index_from_top];
}

}  // namespace message_center
