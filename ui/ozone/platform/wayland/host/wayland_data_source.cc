// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

#include <gtk-primary-selection-client-protocol.h>

#include <cstdint>
#include <vector>

#include "base/files/file_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace wl {

template <typename T>
DataSource<T>::DataSource(T* data_source,
                          ui::WaylandConnection* connection,
                          Delegate* delegate)
    : data_source_(data_source), connection_(connection), delegate_(delegate) {
  DCHECK(data_source_);
  DCHECK(connection_);
  DCHECK(delegate_);

  Initialize();
}

template <typename T>
void DataSource<T>::HandleFinishEvent(bool completed) {
  delegate_->OnDataSourceFinish(/*completed=*/false);
}

template <typename T>
void DataSource<T>::HandleSendEvent(const std::string& mime_type, int32_t fd) {
  std::string contents;
  delegate_->OnDataSourceSend(mime_type, &contents);
  bool done = base::WriteFileDescriptor(fd, contents.data(), contents.length());
  DCHECK(done);
  close(fd);
}

// static
template <typename T>
void DataSource<T>::OnSend(void* data,
                           T* source,
                           const char* mime_type,
                           int32_t fd) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->HandleSendEvent(mime_type, fd);
}

template <typename T>
void DataSource<T>::OnCancel(void* data, T* source) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->HandleFinishEvent(/*completed=*/false);
}

template <typename T>
void DataSource<T>::OnDnDFinished(void* data, T* source) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->HandleFinishEvent(/*completed=*/true);
}

template <typename T>
void DataSource<T>::OnAction(void* data, T* source, uint32_t dnd_action) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->dnd_action_ = dnd_action;
}

template <typename T>
void DataSource<T>::OnTarget(void* data, T* source, const char* mime_type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

template <typename T>
void DataSource<T>::OnDnDDropPerformed(void* data, T* source) {
  NOTIMPLEMENTED_LOG_ONCE();
}

//////////////////////////////////////////////////////////////////////////////
// wl_data_source specializations and instantiation
//////////////////////////////////////////////////////////////////////////////

template <>
void DataSource<wl_data_source>::Initialize() {
  static const struct wl_data_source_listener kDataSourceListener = {
      DataSource<wl_data_source>::OnTarget,
      DataSource<wl_data_source>::OnSend,
      DataSource<wl_data_source>::OnCancel,
      DataSource<wl_data_source>::OnDnDDropPerformed,
      DataSource<wl_data_source>::OnDnDFinished,
      DataSource<wl_data_source>::OnAction};
  wl_data_source_add_listener(data_source_.get(), &kDataSourceListener, this);
}

template <>
void DataSource<wl_data_source>::Offer(
    const std::vector<std::string>& mime_types) {
  for (auto& mime_type : mime_types)
    wl_data_source_offer(data_source_.get(), mime_type.c_str());
  connection_->ScheduleFlush();
}

template <typename T>
void DataSource<T>::SetAction(int operation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

template <>
void DataSource<wl_data_source>::SetAction(int operation) {
  if (wl::get_version_of_object(data_source_.get()) >=
      WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION) {
    uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (operation & ui::DragDropTypes::DRAG_COPY)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    if (operation & ui::DragDropTypes::DRAG_MOVE)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    wl_data_source_set_actions(data_source_.get(), dnd_actions);
  }
}

template class DataSource<wl_data_source>;

//////////////////////////////////////////////////////////////////////////////
// gtk_primary_selection_source specializations and instantiation
//////////////////////////////////////////////////////////////////////////////

template <>
void DataSource<gtk_primary_selection_source>::Initialize() {
  static const struct gtk_primary_selection_source_listener
      kDataSourceListener = {
          DataSource<gtk_primary_selection_source>::OnSend,
          DataSource<gtk_primary_selection_source>::OnCancel};
  gtk_primary_selection_source_add_listener(data_source_.get(),
                                            &kDataSourceListener, this);
}

template <>
void DataSource<gtk_primary_selection_source>::Offer(
    const std::vector<std::string>& mime_types) {
  for (const auto& mime_type : mime_types)
    gtk_primary_selection_source_offer(data_source_.get(), mime_type.c_str());
  connection_->ScheduleFlush();
}

template class DataSource<gtk_primary_selection_source>;

}  // namespace wl
