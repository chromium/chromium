// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_ENTRY_DATA_H_
#define WEBLAYER_BROWSER_NAVIGATION_ENTRY_DATA_H_

#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
class NavigationEntry;
}

namespace weblayer {

// Holds extra data stored on content::NavigationEntry.
class NavigationEntryData : public base::SupportsUserData::Data {
 public:
  ~NavigationEntryData() override;

  // base::SupportsUserData::Data implementation:
  std::unique_ptr<Data> Clone() override;

  static NavigationEntryData* Get(content::NavigationEntry* entry);

  // Stored on the NavigationEntry when we have a cached response from an
  // InputStream.
  struct ResponseData {
    ResponseData();
    ~ResponseData();
    network::mojom::URLResponseHeadPtr response_head;
    std::string data;
    base::Time request_time;
    base::Time response_time;
  };

  void set_response_data(std::unique_ptr<ResponseData> data) {
    response_data_ = std::move(data);
  }

  void reset_response_data() { response_data_.reset(); }

  ResponseData* response_data() { return response_data_.get(); }

  void set_per_navigation_user_agent_override(bool value) {
    per_navigation_user_agent_override_ = value;
  }
  bool per_navigation_user_agent_override() {
    return per_navigation_user_agent_override_;
  }

 private:
  NavigationEntryData();

  std::unique_ptr<ResponseData> response_data_;
  bool per_navigation_user_agent_override_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_ENTRY_DATA_H_
