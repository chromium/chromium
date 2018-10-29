// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/test_window_tree.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/mus/test_window_tree_delegate.h"

namespace aura {

TestWindowTree::TestWindowTree() = default;

TestWindowTree::~TestWindowTree() = default;

bool TestWindowTree::WasEventAcked(uint32_t event_id) const {
  for (const AckedEvent& acked_event : acked_events_) {
    if (acked_event.event_id == event_id)
      return true;
  }
  return false;
}

ws::mojom::EventResult TestWindowTree::GetEventResult(uint32_t event_id) const {
  for (const AckedEvent& acked_event : acked_events_) {
    if (acked_event.event_id == event_id)
      return acked_event.result;
  }
  return ws::mojom::EventResult::UNHANDLED;
}

base::Optional<std::vector<uint8_t>> TestWindowTree::GetLastPropertyValue() {
  return std::move(last_property_value_);
}

base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>
TestWindowTree::GetLastNewWindowProperties() {
  return std::move(last_new_window_properties_);
}

void TestWindowTree::AddScheduledEmbedToken(
    const base::UnguessableToken& token) {
  DCHECK_NE(token, scheduled_embed_);
  scheduled_embed_ = token;
}

void TestWindowTree::AddEmbedRootForToken(const base::UnguessableToken& token) {
  DCHECK_EQ(token, scheduled_embed_);
  scheduled_embed_ = base::UnguessableToken();

  ws::mojom::WindowDataPtr embedder_window_data = ws::mojom::WindowData::New();
  const uint64_t kFakeEmbedderClientId = 1u;
  const uint64_t kFakeEmbedderWindowId = 1u;
  embedder_window_data->window_id =
      (kFakeEmbedderClientId << 32) | kFakeEmbedderWindowId;
  embedder_window_data->bounds = gfx::Rect(320, 240);

  client_->OnEmbedFromToken(token, std::move(embedder_window_data), 0,
                            base::nullopt);
}

void TestWindowTree::RemoveEmbedderWindow(ws::Id embedder_window_id) {
  client_->OnUnembed(embedder_window_id);
}

void TestWindowTree::AckAllChanges() {
  while (!changes_.empty()) {
    client_->OnChangeCompleted(changes_[0].id, true);
    changes_.erase(changes_.begin());
  }
}

bool TestWindowTree::AckSingleChangeOfType(WindowTreeChangeType type,
                                           bool result) {
  auto match = changes_.end();
  for (auto iter = changes_.begin(); iter != changes_.end(); ++iter) {
    if (iter->type == type) {
      if (match == changes_.end())
        match = iter;
      else
        return false;
    }
  }
  if (match == changes_.end())
    return false;
  const uint32_t change_id = match->id;
  changes_.erase(match);
  client_->OnChangeCompleted(change_id, result);
  return true;
}

bool TestWindowTree::AckFirstChangeOfType(WindowTreeChangeType type,
                                          bool result) {
  uint32_t change_id;
  if (!GetAndRemoveFirstChangeOfType(type, &change_id))
    return false;
  client_->OnChangeCompleted(change_id, result);
  return true;
}

void TestWindowTree::AckAllChangesOfType(WindowTreeChangeType type,
                                         bool result) {
  for (size_t i = 0; i < changes_.size();) {
    if (changes_[i].type != type) {
      ++i;
      continue;
    }
    const uint32_t change_id = changes_[i].id;
    changes_.erase(changes_.begin() + i);
    client_->OnChangeCompleted(change_id, result);
  }
}

bool TestWindowTree::GetAndRemoveFirstChangeOfType(WindowTreeChangeType type,
                                                   uint32_t* change_id) {
  for (auto iter = changes_.begin(); iter != changes_.end(); ++iter) {
    if (iter->type != type)
      continue;
    *change_id = iter->id;
    changes_.erase(iter);
    return true;
  }
  return false;
}

size_t TestWindowTree::GetChangeCountForType(WindowTreeChangeType type) {
  size_t count = 0;
  for (const auto& change : changes_) {
    if (change.type == type)
      ++count;
  }
  return count;
}

void TestWindowTree::OnChangeReceived(uint32_t change_id,
                                      WindowTreeChangeType type) {
  changes_.push_back({type, change_id});
}

void TestWindowTree::NewWindow(
    uint32_t change_id,
    ws::Id window_id,
    const base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>&
        properties) {
  last_new_window_properties_ = properties;
  OnChangeReceived(change_id, WindowTreeChangeType::NEW_WINDOW);
}

void TestWindowTree::NewTopLevelWindow(
    uint32_t change_id,
    ws::Id window_id,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  last_new_window_properties_.emplace(properties);
  window_id_ = window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::NEW_TOP_LEVEL);
}

void TestWindowTree::DeleteWindow(uint32_t change_id, ws::Id window_id) {
  OnChangeReceived(change_id);
}

void TestWindowTree::SetWindowBounds(
    uint32_t change_id,
    ws::Id window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  window_id_ = window_id;
  last_local_surface_id_ = local_surface_id;
  last_set_window_bounds_ = bounds;
  OnChangeReceived(change_id, WindowTreeChangeType::BOUNDS);
}

void TestWindowTree::SetWindowTransform(uint32_t change_id,
                                        ws::Id window_id,
                                        const gfx::Transform& transform) {
  OnChangeReceived(change_id, WindowTreeChangeType::TRANSFORM);
}

void TestWindowTree::SetClientArea(
    ws::Id window_id,
    const gfx::Insets& insets,
    const base::Optional<std::vector<gfx::Rect>>& additional_client_areas) {
  last_client_area_ = insets;
}

void TestWindowTree::SetHitTestInsets(ws::Id window_id,
                                      const gfx::Insets& mouse,
                                      const gfx::Insets& touch) {
  last_mouse_hit_test_insets_ = mouse;
  last_touch_hit_test_insets_ = touch;
}

void TestWindowTree::SetCanAcceptDrops(ws::Id window_id, bool accepts_drops) {}

void TestWindowTree::SetWindowVisibility(uint32_t change_id,
                                         ws::Id window_id,
                                         bool visible) {
  OnChangeReceived(change_id, WindowTreeChangeType::VISIBLE);
}

void TestWindowTree::SetWindowProperty(
    uint32_t change_id,
    ws::Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  last_property_value_ = value;
  OnChangeReceived(change_id, WindowTreeChangeType::PROPERTY);
}

void TestWindowTree::SetWindowOpacity(uint32_t change_id,
                                      ws::Id window_id,
                                      float opacity) {
  OnChangeReceived(change_id);
}

void TestWindowTree::AttachCompositorFrameSink(
    ws::Id window_id,
    mojo::InterfaceRequest<viz::mojom::CompositorFrameSink> surface,
    viz::mojom::CompositorFrameSinkClientPtr client) {}

void TestWindowTree::AddWindow(uint32_t change_id,
                               ws::Id parent,
                               ws::Id child) {
  OnChangeReceived(change_id);
}

void TestWindowTree::RemoveWindowFromParent(uint32_t change_id,
                                            ws::Id window_id) {
  OnChangeReceived(change_id);
}

void TestWindowTree::AddTransientWindow(uint32_t change_id,
                                        ws::Id window_id,
                                        ws::Id transient_window_id) {
  transient_data_.parent_id = window_id;
  transient_data_.child_id = transient_window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::ADD_TRANSIENT);
}

void TestWindowTree::RemoveTransientWindowFromParent(
    uint32_t change_id,
    ws::Id transient_window_id) {
  transient_data_.parent_id = kInvalidServerId;
  transient_data_.child_id = transient_window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::REMOVE_TRANSIENT);
}

void TestWindowTree::SetModalType(uint32_t change_id,
                                  ws::Id window_id,
                                  ui::ModalType modal_type) {
  OnChangeReceived(change_id, WindowTreeChangeType::MODAL);
}

void TestWindowTree::ReorderWindow(uint32_t change_id,
                                   ws::Id window_id,
                                   ws::Id relative_window_id,
                                   ws::mojom::OrderDirection direction) {
  OnChangeReceived(change_id, WindowTreeChangeType::REORDER);
}

void TestWindowTree::GetWindowTree(ws::Id window_id,
                                   GetWindowTreeCallback callback) {}

void TestWindowTree::SetCapture(uint32_t change_id, ws::Id window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::CAPTURE);
}

void TestWindowTree::ReleaseCapture(uint32_t change_id, ws::Id window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::CAPTURE);
}

void TestWindowTree::ObserveEventTypes(
    const std::vector<ui::mojom::EventType>& types) {}

void TestWindowTree::Embed(ws::Id window_id,
                           ws::mojom::WindowTreeClientPtr client,
                           uint32_t flags,
                           EmbedCallback callback) {}

void TestWindowTree::ScheduleEmbed(ws::mojom::WindowTreeClientPtr client,
                                   ScheduleEmbedCallback callback) {}

void TestWindowTree::EmbedUsingToken(ws::Id window_id,
                                     const base::UnguessableToken& token,
                                     uint32_t embed_flags,
                                     EmbedUsingTokenCallback callback) {
  if (token != scheduled_embed_) {
    std::move(callback).Run(false);
    return;
  }

  scheduled_embed_ = base::UnguessableToken();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](EmbedUsingTokenCallback callback) {
                       std::move(callback).Run(true);
                     },
                     std::move(callback)));
}

void TestWindowTree::ScheduleEmbedForExistingClient(
    ws::ClientSpecificId window_id,
    ScheduleEmbedForExistingClientCallback callback) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  DCHECK_NE(token, scheduled_embed_);

  scheduled_embed_ = token;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ScheduleEmbedForExistingClientCallback callback,
                        const base::UnguessableToken& token) {
                       std::move(callback).Run(token);
                     },
                     std::move(callback), token));
}

void TestWindowTree::AttachFrameSinkId(uint64_t window_id,
                                       const viz::FrameSinkId& frame_sink_id) {}

void TestWindowTree::UnattachFrameSinkId(uint64_t window_id) {}

void TestWindowTree::SetFocus(uint32_t change_id, ws::Id window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::FOCUS);
}

void TestWindowTree::SetCanFocus(ws::Id window_id, bool can_focus) {}

void TestWindowTree::SetEventTargetingPolicy(
    ws::Id window_id,
    ws::mojom::EventTargetingPolicy policy) {}

void TestWindowTree::SetCursor(uint32_t change_id,
                               ws::Id transport_window_id,
                               ui::CursorData cursor_data) {
  OnChangeReceived(change_id);
}

void TestWindowTree::SetWindowTextInputState(
    ws::Id window_id,
    ui::mojom::TextInputStatePtr state) {}

void TestWindowTree::SetImeVisibility(ws::Id window_id,
                                      bool visible,
                                      ui::mojom::TextInputStatePtr state) {}

void TestWindowTree::OnWindowInputEventAck(uint32_t event_id,
                                           ws::mojom::EventResult result) {
  EXPECT_FALSE(WasEventAcked(event_id));
  acked_events_.push_back({event_id, result});
}

void TestWindowTree::DeactivateWindow(ws::Id window_id) {}

void TestWindowTree::StackAbove(uint32_t change_id,
                                ws::Id above_id,
                                ws::Id below_id) {}

void TestWindowTree::StackAtTop(uint32_t change_id, ws::Id window_id) {}

void TestWindowTree::BindWindowManagerInterface(
    const std::string& name,
    ws::mojom::WindowManagerAssociatedRequest window_manager) {}

void TestWindowTree::GetCursorLocationMemory(
    GetCursorLocationMemoryCallback callback) {
  std::move(callback).Run(mojo::ScopedSharedBufferHandle());
}

void TestWindowTree::PerformDragDrop(
    uint32_t change_id,
    ws::Id source_window_id,
    const gfx::Point& screen_location,
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
    const gfx::ImageSkia& drag_image,
    const gfx::Vector2d& drag_image_offset,
    uint32_t drag_operation,
    ui::mojom::PointerKind source) {
  OnChangeReceived(change_id);
}

void TestWindowTree::CancelDragDrop(ws::Id window_id) {}

void TestWindowTree::PerformWindowMove(uint32_t change_id,
                                       ws::Id window_id,
                                       ws::mojom::MoveLoopSource source,
                                       const gfx::Point& cursor_location) {
  OnChangeReceived(change_id);
}

void TestWindowTree::CancelWindowMove(ws::Id window_id) {}

void TestWindowTree::ObserveTopmostWindow(ws::mojom::MoveLoopSource source,
                                          ws::Id window_id) {}
void TestWindowTree::StopObservingTopmostWindow() {}

void TestWindowTree::CancelActiveTouchesExcept(ws::Id not_cancelled_window_id) {
  last_not_cancelled_window_id_ = not_cancelled_window_id;
}

void TestWindowTree::CancelActiveTouches(ws::Id window_id) {
  last_cancelled_window_id_ = window_id;
}
void TestWindowTree::TransferGestureEventsTo(ws::Id current_id,
                                             ws::Id new_id,
                                             bool should_cancel) {
  last_transfer_current_ = current_id;
  last_transfer_new_ = new_id;
  last_transfer_should_cancel_ = should_cancel;
}

void TestWindowTree::TrackOcclusionState(ws::Id window_id) {
  DCHECK(delegate_);
  delegate_->TrackOcclusionState(window_id);
}

void TestWindowTree::PauseWindowOcclusionTracking() {
  // |delegate_| could reset during shutdown.
  if (delegate_)
    delegate_->PauseWindowOcclusionTracking();
}

void TestWindowTree::UnpauseWindowOcclusionTracking() {
  // |delegate_| could reset during shutdown.
  if (delegate_)
    delegate_->UnpauseWindowOcclusionTracking();
}

}  // namespace aura
