// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <algorithm>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/animation/animation_builder.h"

namespace message_center {

namespace {

// Animation duration for kFadeIn and kFadeOut.
constexpr base::TimeDelta kFadeInFadeOutDuration = base::Milliseconds(200);

// Animation duration for kMoveDown.
constexpr base::TimeDelta kMoveDownDuration = base::Milliseconds(120);

}  // namespace

MessagePopupCollection::PopupItem::PopupItem() = default;
MessagePopupCollection::PopupItem::PopupItem(PopupItem&& other) = default;
MessagePopupCollection::PopupItem& MessagePopupCollection::PopupItem::operator=(
    PopupItem&& other) = default;
MessagePopupCollection::PopupItem::~PopupItem() = default;

MessagePopupCollection::MessagePopupCollection()
    : animation_(std::make_unique<gfx::LinearAnimation>(this)),
      weak_ptr_factory_(this) {
  message_center_observation_.Observe(MessageCenter::Get());
}

MessagePopupCollection::~MessagePopupCollection() {
  // Ignore calls to update which can cause crashes.
  is_updating_ = true;
  for (auto& item : popup_items_) {
    ClosePopupItem(item);
  }
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

  if (state_ != State::kIdle)
    TransitionFromAnimation();

  if (state_ == State::kIdle)
    TransitionToAnimation();

  UpdatePopupTimers();

  if (state_ != State::kIdle) {
    // If not in kIdle state, start animation.
    base::TimeDelta animation_duration;
    if (state_ == State::kMoveDown) {
      animation_duration = kMoveDownDuration;
    } else {
      animation_duration = kFadeInFadeOutDuration;
    }
    animation_->SetDuration(
        animation_duration *
        ui::ScopedAnimationDurationScaleMode::duration_multiplier());
    animation_->Start();
    AnimationStarted();
    UpdateByAnimation();
  }

  DCHECK(state_ == State::kIdle || animation_->is_animating());
}

void MessagePopupCollection::ResetBounds() {
  if (is_updating_)
    return;
  {
    base::AutoReset<bool> reset(&is_updating_, true);

    RemoveClosedPopupItems();
    state_ = State::kIdle;
    animation_->End();

    CalculateAndUpdateBounds();

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
  CloseAndRemovePopupFromPopupItem(popup);
}

void MessagePopupCollection::AnimateResize() {
  CalculateAndUpdateBounds();

  views::AnimationBuilder animation_builder;
  for (auto& popup : popup_items_) {
    auto target_bounds = gfx::Rect(
        popup.popup->GetWidget()->GetLayer()->bounds().x(), popup.bounds.y(),
        popup.bounds.width(), popup.bounds.height());
    animation_builder.Once()
        .SetDuration(base::Milliseconds(kNotificationResizeAnimationDurationMs))
        .SetBounds(popup.popup->GetWidget()->GetLayer(), target_bounds,
                   gfx::Tween::EASE_OUT);
  }
}

MessageView* MessagePopupCollection::GetMessageViewForNotificationId(
    const std::string& notification_id) {
  auto it = base::ranges::find_if(popup_items_, [&](const auto& child) {
    // Exit early if the popup ptr has been set to nullptr by
    // `NotifyPopupClosed` but has not been cleared from `popup_items_`.
    if (!child.popup)
      return false;

    auto* widget = child.popup->GetWidget();
    // Do not return popups that are in the process of closing, but have not
    // yet been removed from `popup_items_`.
    if (!widget || widget->IsClosed())
      return false;
    return child.popup->message_view()->notification_id() == notification_id;
  });

  if (it == popup_items_.end())
    return nullptr;

  return it->popup->message_view();
}

void MessagePopupCollection::ConvertNotificationViewToGroupedNotificationView(
    const std::string& ungrouped_notification_id,
    const std::string& new_grouped_notification_id) {
  auto it = base::ranges::find(popup_items_, ungrouped_notification_id,
                               &PopupItem::id);
  if (it == popup_items_.end())
    return;

  it->id = new_grouped_notification_id;
  it->popup->message_view()->set_notification_id(new_grouped_notification_id);
}

void MessagePopupCollection::ConvertGroupedNotificationViewToNotificationView(
    const std::string& grouped_notification_id,
    const std::string& new_single_notification_id) {
  auto it =
      base::ranges::find(popup_items_, grouped_notification_id, &PopupItem::id);
  if (it == popup_items_.end())
    return;

  it->id = new_single_notification_id;
  it->popup->message_view()->set_notification_id(new_single_notification_id);
}

void MessagePopupCollection::OnChildNotificationViewUpdated(
    const std::string& parent_notification_id,
    const std::string& child_notification_id) {
  auto* notification =
      MessageCenter::Get()->FindNotificationById(child_notification_id);
  if (!notification) {
    return;
  }

  auto* parent_popup = GetPopupViewForNotificationID(parent_notification_id);
  if (parent_popup) {
    parent_popup->UpdateContentsForChildNotification(child_notification_id,
                                                     *notification);
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

  // Notify if the incoming notification is silent.
  const Notification* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  if (notification && notification->priority() < DEFAULT_PRIORITY) {
    NotifySilentNotification(notification->id());
  }
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
  const auto& notifications = GetPopupNotifications();
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
  AnimationFinished();
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

size_t MessagePopupCollection::GetPopupItemsCount() {
  return popup_items_.size();
}

MessagePopupView* MessagePopupCollection::CreatePopup(
    const Notification& notification) {
  bool a11_feedback_on_init =
      notification.rich_notification_data()
          .should_make_spoken_feedback_for_popup_updates;
  return new MessagePopupView(new NotificationView(notification), this,
                              a11_feedback_on_init);
}

bool MessagePopupCollection::IsNextEdgeOutsideWorkArea(
    const PopupItem& item) const {
  const int next_edge = GetNextEdge(item);

  const gfx::Rect work_area = GetWorkArea();
  return IsTopDown() ? next_edge > work_area.bottom()
                     : next_edge < work_area.y();
}

void MessagePopupCollection::ClosePopupItem(PopupItem& item) {
  if (MessagePopupView* popup = item.popup) {
    popup->Close();
    // Re-check item.popup since the Close() call may have deleted it.
    if (popup == item.popup) {
      if (!popup->view_added_to_widget()) {
        // Take ownership and delete when leaving scope.
        auto owned_popup = base::WrapUnique(popup);
        // This doesn't delete the delegate, but does ensure notifications about
        // it are still sent.
        owned_popup->DeleteDelegate();
        CloseAndRemovePopupFromPopupItem(owned_popup.get(), true);
      }
    }
    if (item.widget) {
      item.widget.reset();
    }
  }
}

void MessagePopupCollection::MoveDownPopups() {
  CalculateAndUpdateBounds();
  for (auto& item : popup_items_) {
    item.is_animating = true;
  }
}

void MessagePopupCollection::RestartPopupTimers() {
  MessageCenter::Get()->RestartPopupTimers();
}

void MessagePopupCollection::PausePopupTimers() {
  MessageCenter::Get()->PausePopupTimers();
}

void MessagePopupCollection::CloseAllPopupsNow() {
  for (auto& item : popup_items_) {
    // A popup might have already been removed when this is called.
    if (!item.popup) {
      continue;
    }

    item.is_animating = true;

    // Mark the popup as shown so that the popup item will not re-appear after
    // any subsequent calls to `GetPopupNotifications()`.
    MessageCenter::Get()->MarkSinglePopupAsShown(
        item.id, /*mark_notification_as_read=*/true);
  }
  CloseAnimatingPopups();

  state_ = State::kIdle;
  animation_->End();
}

void MessagePopupCollection::TransitionFromAnimation() {
  DCHECK_NE(state_, State::kIdle);
  DCHECK(!animation_->is_animating());

  // The animation of type |state_| is now finished.
  UpdateByAnimation();

  // If kFadeOut animation is finished, remove the animated popup.
  if (state_ == State::kFadeOut) {
    CloseAnimatingPopups();
  }

  if (state_ == State::kFadeIn || state_ == State::kMoveDown ||
      (state_ == State::kFadeOut && popup_items_.empty())) {
    // If the animation is finished, transition to kIdle.
    state_ = State::kIdle;
  } else if (state_ == State::kFadeOut && !popup_items_.empty()) {
    if (HasAddedPopup()) {
      CollapseAllPopups();
    }
    // If kFadeOut animation is finished and we still have remaining popups,
    // we have to kMoveDown them.
    // If we're going to add a new popup after this kMoveDown, do the collapse
    // animation at the same time. Otherwise it will take another kMoveDown.
    state_ = State::kMoveDown;
    MoveDownPopups();
  }
}

void MessagePopupCollection::TransitionToAnimation() {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!animation_->is_animating());

  if (HasRemovedPopup()) {
    MarkRemovedPopup();

    if (CloseTransparentPopups()) {
      // If the popup is already transparent, skip kFadeOut.
      state_ = State::kMoveDown;
      MoveDownPopups();
    } else {
      state_ = State::kFadeOut;
    }
    return;
  }

  if (HasAddedPopup()) {
    if (CollapseAllPopups()) {
      // If we had existing popups that weren't collapsed, first show collapsing
      // animation.
      state_ = State::kMoveDown;
      MoveDownPopups();
      return;
    } else if (AddPopup()) {
      // A popup is actually added. Show kFadein animation.
      state_ = State::kFadeIn;
      return;
    }
  }

  if (resize_requested_) {
    // Resize is requested e.g. a user manually expanded notification.
    resize_requested_ = false;
    state_ = State::kMoveDown;
    MoveDownPopups();

    // This function may be called by a child MessageView when a notification is
    // expanded by the user.  Deleting the pop-up should be delayed so we are
    // out of the child view's call stack. See crbug.com/957033.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MessagePopupCollection::ClosePopupsOutsideWorkArea,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
}

void MessagePopupCollection::UpdatePopupTimers() {
  if (state_ == State::kIdle) {
    if (IsAnyPopupHovered() || IsAnyPopupFocused()) {
      // If any popup is hovered or focused, pause popup timer.
      PausePopupTimers();
    } else {
      // If in kIdle state, restart popup timer.
      RestartPopupTimers();
    }
  } else {
    // If not in kIdle state, pause popup timer.
    PausePopupTimers();
  }
}

void MessagePopupCollection::CalculateAndUpdateBounds() {
  int base = GetBaseline();

  int popup_bounds_origin_x = 0;
  int popup_bounds_origin_y = 0;
  int popup_bounds_height = 0;
  if (IsTopDown()) {
    popup_bounds_origin_y = base;
  }

  int notification_width = GetNotificationWidth();

  for (size_t i = 0; i < popup_items_.size(); ++i) {
    gfx::Size preferred_size(
        notification_width,
        GetPopupItem(i)->popup->GetHeightForWidth(notification_width));

    int origin_x = GetPopupOriginX(gfx::Rect(preferred_size));

    popup_bounds_origin_x = origin_x;

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

    popup_bounds_height += delta;
  }

  if (!IsTopDown()) {
    popup_bounds_origin_y = base + kMarginBetweenPopups;
  }

  int old_popup_collection_height = popup_collection_bounds_.height();

  popup_collection_bounds_ =
      gfx::Rect(popup_bounds_origin_x, popup_bounds_origin_y,
                notification_width, popup_bounds_height - kMarginBetweenPopups);

  if (old_popup_collection_height != popup_collection_bounds_.height()) {
    NotifyPopupCollectionHeightChanged();
  }
}

void MessagePopupCollection::UpdateByAnimation() {
  DCHECK_NE(state_, State::kIdle);

  for (auto& item : popup_items_) {
    if (!item.is_animating)
      continue;

    double value = gfx::Tween::CalculateValue(
        state_ == State::kFadeOut ? gfx::Tween::EASE_IN : gfx::Tween::EASE_OUT,
        animation_->GetCurrentValue());

    if (state_ == State::kFadeIn)
      item.popup->SetOpacity(gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f));
    else if (state_ == State::kFadeOut)
      item.popup->SetOpacity(gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f));

    if (state_ == State::kFadeIn || state_ == State::kMoveDown) {
      item.popup->SetPopupBounds(
          gfx::Tween::RectValueBetween(value, item.start_bounds, item.bounds));
    }
  }
}

std::vector<Notification*> MessagePopupCollection::GetPopupNotifications()
    const {
  std::vector<Notification*> result;
  for (auto* notification : MessageCenter::Get()->GetPopupNotifications()) {
    // Disables popup of custom notification on non-primary displays, since
    // currently custom notification supports only on one display at the same
    // time.
    // TODO(yoshiki): Support custom popup notification on multiple display
    // (https://crbug.com/715370).
    if (!IsPrimaryDisplayForNotification() &&
        notification->type() == NOTIFICATION_TYPE_CUSTOM) {
      continue;
    }

    if (notification->group_child())
      continue;

    if (BlockForMixedFullscreen(*notification))
      continue;

    result.emplace_back(notification);
  }
  return result;
}

bool MessagePopupCollection::AddPopup() {
  std::set<std::string> existing_ids;
  for (const auto& item : popup_items_)
    existing_ids.insert(item.id);

  auto notifications = GetPopupNotifications();
  Notification* new_notification = nullptr;
  // Reverse iterating because notifications are in reverse chronological order.
  for (Notification* notification : base::Reversed(notifications)) {
    if (!existing_ids.count(notification->id())) {
      new_notification = notification;
      break;
    }
  }

  if (!new_notification)
    return false;

  // Reset animation flags of existing popups.
  for (auto& item : popup_items_) {
    item.is_animating = false;
  }

  if (new_notification->group_child())
    return false;

  {
    PopupItem item;
    item.id = new_notification->id();
    item.is_animating = true;
    item.popup = CreatePopup(*new_notification);

    if (IsNextEdgeOutsideWorkArea(item)) {
      ClosePopupItem(item);
      return false;
    }

    item.widget = item.popup->Show();
    popup_items_.push_back(std::move(item));
    NotifyPopupAdded(popup_items_.back().popup);
  }

  MessageCenter::Get()->DisplayedNotification(new_notification->id(),
                                              DISPLAY_SOURCE_POPUP);

  CalculateAndUpdateBounds();

  // We might remove all popup items after update bounds.
  // TODO(b/302172146): Remove this check once we have the long-term solution
  // for notifier collision.
  if (!popup_items_.empty()) {
    auto& item = popup_items_.back();
    item.start_bounds = item.bounds;
    item.start_bounds +=
        gfx::Vector2d((IsFromLeft() ? -1 : 1) * item.bounds.width(), 0);
  }

  return true;
}

void MessagePopupCollection::MarkRemovedPopup() {
  std::set<std::string> existing_ids;
  for (Notification* notification : GetPopupNotifications()) {
    existing_ids.insert(notification->id());
  }

  for (auto& item : popup_items_) {
    bool removing = !existing_ids.count(item.id);
    item.is_animating = removing;
    if (removing)
      NotifyPopupRemoved(item.id);
  }
}

int MessagePopupCollection::GetNextEdge(const PopupItem& item) const {
  const int delta = item.popup->GetHeightForWidth(GetNotificationWidth()) +
                    kMarginBetweenPopups;

  int base = 0;
  if (popup_items_.empty()) {
    base = GetBaseline();
  } else {
    base = IsTopDown() ? popup_items_.back().bounds.bottom()
                       : popup_items_.back().bounds.y();
  }

  return IsTopDown() ? base + delta : base - delta;
}

void MessagePopupCollection::CloseAnimatingPopups() {
  for (auto& item : popup_items_) {
    if (!item.is_animating)
      continue;
    ClosePopupItem(item);
  }
  RemoveClosedPopupItems();
}

bool MessagePopupCollection::CloseTransparentPopups() {
  bool removed = false;
  for (auto& item : popup_items_) {
    if (item.popup->GetOpacity() > 0.0)
      continue;
    ClosePopupItem(item);
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
    ClosePopupItem(item);
  }
  RemoveClosedPopupItems();
}

void MessagePopupCollection::RemoveClosedPopupItems() {
  std::erase_if(popup_items_, [](const auto& item) { return !item.popup; });
}

void MessagePopupCollection::CloseAndRemovePopupFromPopupItem(
    MessagePopupView* popup,
    bool remove_only) {
  for (auto& item : popup_items_) {
    if (item.popup && item.popup == popup) {
      if (!remove_only) {
        popup->Close();
      }
      item.popup = nullptr;
    }
  }
}

bool MessagePopupCollection::CollapseAllPopups() {
  bool changed = false;
  int notification_width = GetNotificationWidth();
  for (auto& item : popup_items_) {
    int old_height = item.popup->GetHeightForWidth(notification_width);

    item.popup->AutoCollapse();

    int new_height = item.popup->GetHeightForWidth(notification_width);
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

  for (Notification* notification : GetPopupNotifications()) {
    if (!existing_ids.count(notification->id())) {
      // A new popup is not added for a group child if it's parent
      // notification has an existing popup.
      if (notification->group_child()) {
        auto* parent_notification =
            MessageCenter::Get()->FindParentNotification(notification);

        return !existing_ids.count(parent_notification->id());
      }
      return true;
    }
  }
  return false;
}

bool MessagePopupCollection::HasRemovedPopup() const {
  std::set<std::string> existing_ids;
  for (Notification* notification : GetPopupNotifications()) {
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

bool MessagePopupCollection::IsAnyPopupFocused() const {
  for (const auto& item : popup_items_) {
    if (item.popup->is_focused())
      return true;
  }
  return false;
}

MessagePopupCollection::PopupItem* MessagePopupCollection::GetPopupItem(
    size_t index_from_top) {
  DCHECK_LT(index_from_top, popup_items_.size());
  return &popup_items_[index_from_top];
}

}  // namespace message_center
