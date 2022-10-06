// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/storage_test_runner.h"

#include <cassert>
#include <cstddef>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/callback.h"

namespace autofill {

using ::i18n::addressinput::Storage;

StorageTestRunner::StorageTestRunner(Storage* storage)
    : storage_(storage),
      success_(false),
      key_(),
      data_() {}

StorageTestRunner::~StorageTestRunner() {}

void StorageTestRunner::RunAllTests() {
  EXPECT_NO_FATAL_FAILURE(GetWithoutPutReturnsEmptyData());
  EXPECT_NO_FATAL_FAILURE(GetReturnsWhatWasPut());
  EXPECT_NO_FATAL_FAILURE(SecondPutOverwritesData());
}

void StorageTestRunner::ClearValues() {
  success_ = false;
  key_.clear();
  data_.clear();
}

std::unique_ptr<Storage::Callback> StorageTestRunner::BuildCallback() {
  return std::unique_ptr<Storage::Callback>(::i18n::addressinput::BuildCallback(
      this, &StorageTestRunner::OnDataReady));
}

void StorageTestRunner::OnDataReady(bool success,
                                    const std::string& key,
                                    std::string* data) {
  assert(!success || data != NULL);
  success_ = success;
  key_ = key;
  if (data != NULL) {
    data_ = *data;
    delete data;
  }
}

void StorageTestRunner::GetWithoutPutReturnsEmptyData() {
  ClearValues();
  std::unique_ptr<Storage::Callback> callback(BuildCallback());
  storage_->Get("key", *callback);

  EXPECT_FALSE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_TRUE(data_.empty());
}

void StorageTestRunner::GetReturnsWhatWasPut() {
  ClearValues();
  storage_->Put("key", new std::string("value"));
  std::unique_ptr<Storage::Callback> callback(BuildCallback());
  storage_->Get("key", *callback);

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("value", data_);
}

void StorageTestRunner::SecondPutOverwritesData() {
  ClearValues();
  storage_->Put("key", new std::string("bad-value"));
  storage_->Put("key", new std::string("good-value"));
  std::unique_ptr<Storage::Callback> callback(BuildCallback());
  storage_->Get("key", *callback);

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("good-value", data_);
}

}  // namespace autofill
