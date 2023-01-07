// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/fallback_data_store.h"

#include <cstddef>
#include <ctime>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/src/util/json.h"
#include "third_party/libaddressinput/src/cpp/src/validating_util.h"

namespace autofill {

using i18n::addressinput::Json;
using i18n::addressinput::ValidatingUtil;

TEST(FallbackDataStore, Parsability) {
  std::string data;
  ASSERT_TRUE(FallbackDataStore::Get("data/US", &data));

  // Should be stale.
  EXPECT_FALSE(ValidatingUtil::UnwrapTimestamp(&data, time(NULL)));

  // Should be uncorrupted.
  EXPECT_TRUE(ValidatingUtil::UnwrapChecksum(&data));

  // Should be valid JSON.
  Json json;
  ASSERT_TRUE(json.ParseObject(data));

  // Should not have a string for "data/US", because "data/US" is a dictionary.
  std::string value;
  EXPECT_FALSE(json.GetStringValueForKey("data/US", &value));

  // Should have a dictionary with "data/US" identifier.
  const std::vector<const Json*>& sub_dicts = json.GetSubDictionaries();
  bool key_found = false;
  for (std::vector<const Json*>::const_iterator it = sub_dicts.begin();
       it != sub_dicts.end(); ++it) {
    const Json* sub_dict = *it;
    EXPECT_TRUE(sub_dict->GetStringValueForKey("id", &value));
    key_found |= value == "data/US";
  }
  EXPECT_TRUE(key_found);
}

}  // namespace autofill
