// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/prefs/writeable_pref_store.h"
#include "third_party/libaddressinput/chromium/fallback_data_store.h"

namespace autofill {

ChromeStorageImpl::ChromeStorageImpl(WriteablePrefStore* store)
    : backing_store_(store) {
  scoped_observation_.Observe(backing_store_);
}

ChromeStorageImpl::~ChromeStorageImpl() {}

void ChromeStorageImpl::Put(const std::string& key, std::string* data) {
  DCHECK(data);
  backing_store_->SetValue(key, base::Value(*data),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void ChromeStorageImpl::Get(const std::string& key,
                            const Storage::Callback& data_ready) const {
  // |Get()| should not be const, so this is just a thunk that fixes that.
  const_cast<ChromeStorageImpl*>(this)->DoGet(key, data_ready);
}

void ChromeStorageImpl::OnInitializationCompleted(bool succeeded) {
  for (const auto& request : outstanding_requests_)
    DoGet(request->key, request->callback);

  outstanding_requests_.clear();
}

void ChromeStorageImpl::DoGet(const std::string& key,
                              const Storage::Callback& data_ready) {
  if (!backing_store_->IsInitializationComplete()) {
    outstanding_requests_.push_back(std::make_unique<Request>(key, data_ready));
    return;
  }

  const base::Value* value = NULL;
  std::unique_ptr<std::string> data(new std::string);
  if (backing_store_->GetValue(key, &value) && value->is_string()) {
    *data = value->GetString();
    data_ready(true, key, data.release());
  } else if (FallbackDataStore::Get(key, data.get())) {
    data_ready(true, key, data.release());
  } else {
    data_ready(false, key, NULL);
  }
}

ChromeStorageImpl::Request::Request(const std::string& key,
                                    const Callback& callback)
    : key(key),
      callback(callback) {}

}  // namespace autofill
