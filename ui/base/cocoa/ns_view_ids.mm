// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/ns_view_ids.h"

#import <Cocoa/Cocoa.h>
#include <map>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"

namespace ui {

std::map<uint64_t, NSView*>& GetNSViewIdMap() {
  static base::NoDestructor<std::map<uint64_t, NSView*>> instance;
  return *instance;
}

// static
uint64_t NSViewIds::GetNewId() {
  static uint64_t next_id = 1;
  return next_id++;
}

// static
NSView* NSViewIds::GetNSView(uint64_t ns_view_id) {
  auto& view_map = GetNSViewIdMap();
  auto found = view_map.find(ns_view_id);
  if (found == view_map.end())
    return nil;
  return found->second;
}

ScopedNSViewIdMapping::ScopedNSViewIdMapping(uint64_t ns_view_id, NSView* view)
    : ns_view_id_(ns_view_id) {
  auto result = GetNSViewIdMap().insert(std::make_pair(ns_view_id, view));
  DCHECK(result.second);
}

ScopedNSViewIdMapping::~ScopedNSViewIdMapping() {
  auto& view_map = GetNSViewIdMap();
  auto found = view_map.find(ns_view_id_);
  DCHECK(found != view_map.end());
  view_map.erase(found);
}

}  // namespace ui
