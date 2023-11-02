// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_entry_data.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_entry.h"

namespace weblayer {

namespace {

const char kCacheKey[] = "weblayer_navigation_entry_data";

}  // namespace

NavigationEntryData::ResponseData::ResponseData() = default;
NavigationEntryData::ResponseData::~ResponseData() = default;

NavigationEntryData::NavigationEntryData() = default;
NavigationEntryData::~NavigationEntryData() = default;

std::unique_ptr<base::SupportsUserData::Data> NavigationEntryData::Clone() {
  auto rv = base::WrapUnique(new NavigationEntryData);
  rv->per_navigation_user_agent_override_ = per_navigation_user_agent_override_;
  if (response_data_) {
    rv->response_data_ = std::make_unique<ResponseData>();
    rv->response_data_->response_head = response_data_->response_head.Clone();
    rv->response_data_->data = response_data_->data;
    rv->response_data_->request_time = response_data_->request_time;
    rv->response_data_->response_time = response_data_->response_time;
  }
  return rv;
}

NavigationEntryData* NavigationEntryData::Get(content::NavigationEntry* entry) {
  auto* data = static_cast<NavigationEntryData*>(entry->GetUserData(kCacheKey));
  if (!data) {
    auto data_object = base::WrapUnique(new NavigationEntryData);
    data = data_object.get();
    entry->SetUserData(kCacheKey, std::move(data_object));
  }
  return data;
}

}  // namespace weblayer
