// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DROP_TARGET_EVENT_H_
#define UI_BASE_DRAGDROP_DROP_TARGET_EVENT_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"

namespace ui {

// Note: This object must not outlive the OSExchangeData used to construct it,
// as it stores that by reference.
class COMPONENT_EXPORT(UI_BASE) DropTargetEvent : public LocatedEvent {
 public:
  DropTargetEvent(const OSExchangeData& data,
                  const gfx::PointF& location,
                  const gfx::PointF& root_location,
                  int source_operations);
  DropTargetEvent(const DropTargetEvent& other);

  const OSExchangeData& data() const { return *data_; }
  int source_operations() const { return source_operations_; }

  // Event:
  std::unique_ptr<Event> Clone() const override;

 private:
  // Data associated with the drag/drop session.
  const raw_ref<const OSExchangeData, AcrossTasksDanglingUntriaged> data_;

  // Bitmask of supported DragDropTypes::DragOperation by the source.
  int source_operations_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DROP_TARGET_EVENT_H_
