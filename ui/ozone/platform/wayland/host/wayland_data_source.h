// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

struct wl_data_source;
struct gtk_primary_selection_source;
struct zwp_primary_selection_source_v1;

namespace base {
class TimeTicks;
}

namespace wl {
template <typename T>
class DataSource;
}  // namespace wl

namespace ui {

class WaylandConnection;

// DataSource represents the source side of a DataOffer. It is created by the
// source client in a data transfer and provides a way to describe the offered
// data and a way to respond to requests to transfer the data. There are a few
// variants of Wayland protocol objects and extensions supporting different
// features. E.g: regular copy/paste and drag operations are implemented by
// wl_data_source (along with its _device and _offer counterparts), etc.
// Implementation wise, these variants are share a single class template, with
// specializations defined for each underlying supported extensions. Below are
// the type aliases for the variants currently supported.

using WaylandDataSource = wl::DataSource<wl_data_source>;

using GtkPrimarySelectionSource = wl::DataSource<gtk_primary_selection_source>;

using ZwpPrimarySelectionSource = wl::DataSource<zwp_primary_selection_source_v1>;

}  // namespace ui

namespace wl {

// Template class implementing DataSource, whereas T is the underlying source
// type, e.g: wl_data_source, gtk_primary_selection_source, etc. This class
// is not supposed to be used directly, instead use the aliases defined above.
template <typename T>
class DataSource {
 public:
  class Delegate {
   public:
    virtual void OnDataSourceFinish(DataSource<T>* source,
                                    base::TimeTicks timestamp,
                                    bool completed) = 0;
    virtual void OnDataSourceSend(DataSource<T>* source,
                                  const std::string& mime_type,
                                  std::string* contents) = 0;
    // Optional callback intended to be implemented only by dnd-capable delegate
    // implementations.
    virtual void OnDataSourceDropPerformed(DataSource<T>* source,
                                           base::TimeTicks timestamp) {}

   protected:
    virtual ~Delegate() = default;
  };

  // Takes ownership of |data_source|.
  DataSource(T* data_source,
             ui::WaylandConnection* connection,
             Delegate* delegate);
  DataSource(const DataSource<T>&) = delete;
  DataSource& operator=(const DataSource<T>&) = delete;
  ~DataSource();

  void Initialize();
  void Offer(const std::vector<std::string>& mime_types);
  void SetDndActions(uint32_t dnd_actions);

  uint32_t dnd_action() const { return dnd_action_; }
  T* data_source() const { return data_source_.get(); }

 private:
  void HandleFinishEvent(bool completed);
  void HandleDropEvent();
  void HandleSendEvent(const std::string& mime_type, int32_t fd);

  // {T}_listener callbacks:
  static void OnSend(void* data, T* source, const char* mime_type, int32_t fd);
  static void OnCancelled(void* data, T* source);
  static void OnDndFinished(void* data, T* source);
  static void OnAction(void* data, T* source, uint32_t dnd_action);
  static void OnTarget(void* data, T* source, const char* mime_type);
  static void OnDndDropPerformed(void* data, T* source);

  wl::Object<T> data_source_;

  const raw_ptr<ui::WaylandConnection> connection_;

  const raw_ptr<Delegate> delegate_;

  // Action selected by the compositor
  uint32_t dnd_action_ = 0;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_
