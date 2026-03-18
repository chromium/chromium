// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

#include <fcntl.h>
#include <gtk-primary-selection-client-protocol.h>
#include <primary-selection-unstable-v1-client-protocol.h>

#include <cstdint>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace wl {

namespace {

// Writes `data` to file descriptor `fd`. This is performed on a background
// thread to avoid blocking the UI thread.
void WriteData(base::ScopedFD fd, std::string data) {
  int flags = fcntl(fd.get(), F_GETFL);
  if (flags != -1 && (flags & O_NONBLOCK)) {
    fcntl(fd.get(), F_SETFL, flags & ~O_NONBLOCK);
  }

  bool done = base::WriteFileDescriptor(fd.get(), data);
  VPLOG_IF(1, !done) << "Failed to write";
}

}  // namespace

template <typename T>
DataSource<T>::DataSource(T* data_source,
                          ui::WaylandConnection* connection,
                          Delegate* delegate)
    : data_source_(data_source), connection_(connection), delegate_(delegate) {
  DCHECK(data_source_);
  DCHECK(connection_);
  DCHECK(delegate_);

  Initialize();
  VLOG(1) << "DataSource created:" << this;
}

template <typename T>
DataSource<T>::~DataSource() {
  VLOG(1) << "DataSource deleted:" << this;
}

template <typename T>
void DataSource<T>::HandleDropEvent() {
  VLOG(1) << "OnDataSourceDropPerformed in WaylandDataSource";
  // No timestamp for these events. Use EventTimeForNow(), for now.
  delegate_->OnDataSourceDropPerformed(this, ui::EventTimeForNow());
}

template <typename T>
void DataSource<T>::HandleFinishEvent(bool completed) {
  VLOG(1) << "OnDataSourceFinish in WaylandDataSource";
  // No timestamp for these events. Use EventTimeForNow(), for now.
  delegate_->OnDataSourceFinish(this, ui::EventTimeForNow(), completed);
}

template <typename T>
void DataSource<T>::HandleSendEvent(const std::string& mime_type, int32_t fd) {
  auto callback = base::BindOnce(
      [](base::ScopedFD fd, std::string contents) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&WriteData, std::move(fd), std::move(contents)));
      },
      base::ScopedFD(fd));
  delegate_->OnDataSourceSend(this, mime_type, std::move(callback));
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
void DataSource<T>::OnCancelled(void* data, T* source) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->HandleFinishEvent(/*completed=*/false);
}

template <typename T>
void DataSource<T>::OnDndFinished(void* data, T* source) {
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
void DataSource<T>::OnDndDropPerformed(void* data, T* source) {
  auto* self = static_cast<DataSource<T>*>(data);
  self->HandleDropEvent();
}

//////////////////////////////////////////////////////////////////////////////
// wl_data_source specializations and instantiation
//////////////////////////////////////////////////////////////////////////////

template <>
void DataSource<wl_data_source>::Initialize() {
  static constexpr wl_data_source_listener kDataSourceListener = {
      .target = &OnTarget,
      .send = &OnSend,
      .cancelled = &OnCancelled,
      .dnd_drop_performed = &OnDndDropPerformed,
      .dnd_finished = &OnDndFinished,
      .action = &OnAction};
  wl_data_source_add_listener(data_source_.get(), &kDataSourceListener, this);
}

template <>
void DataSource<wl_data_source>::Offer(
    const std::vector<std::string>& mime_types) {
  for (auto& mime_type : mime_types)
    wl_data_source_offer(data_source_.get(), mime_type.c_str());
  connection_->Flush();
}

template <typename T>
void DataSource<T>::SetDndActions(uint32_t dnd_actions) {
  NOTIMPLEMENTED_LOG_ONCE();
}

template <>
void DataSource<wl_data_source>::SetDndActions(uint32_t dnd_actions) {
  if (wl::get_version_of_object(data_source_.get()) >=
      WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION) {
    wl_data_source_set_actions(data_source_.get(), dnd_actions);
  }
}

template class DataSource<wl_data_source>;

//////////////////////////////////////////////////////////////////////////////
// gtk_primary_selection_source specializations and instantiation
//////////////////////////////////////////////////////////////////////////////

template <>
void DataSource<gtk_primary_selection_source>::Initialize() {
  static constexpr gtk_primary_selection_source_listener kDataSourceListener = {
      .send = &OnSend, .cancelled = &OnCancelled};
  gtk_primary_selection_source_add_listener(data_source_.get(),
                                            &kDataSourceListener, this);
}

template <>
void DataSource<gtk_primary_selection_source>::Offer(
    const std::vector<std::string>& mime_types) {
  for (const auto& mime_type : mime_types)
    gtk_primary_selection_source_offer(data_source_.get(), mime_type.c_str());
  connection_->Flush();
}

template <>
void DataSource<zwp_primary_selection_source_v1>::Initialize() {
  static constexpr zwp_primary_selection_source_v1_listener
      kDataSourceListener = {
          .send = DataSource<zwp_primary_selection_source_v1>::OnSend,
          .cancelled =
              DataSource<zwp_primary_selection_source_v1>::OnCancelled};
  zwp_primary_selection_source_v1_add_listener(data_source_.get(),
                                               &kDataSourceListener, this);
}

template <>
void DataSource<zwp_primary_selection_source_v1>::Offer(
    const std::vector<std::string>& mime_types) {
  for (const auto& mime_type : mime_types)
    zwp_primary_selection_source_v1_offer(data_source_.get(),
                                          mime_type.c_str());
  connection_->Flush();
}

template class DataSource<gtk_primary_selection_source>;
template class DataSource<zwp_primary_selection_source_v1>;

}  // namespace wl
