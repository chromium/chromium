// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/prefs/pref_store.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

class WriteablePrefStore;

namespace autofill {

// An implementation of the Storage interface which passes through to an
// underlying WriteablePrefStore.
class ChromeStorageImpl : public ::i18n::addressinput::Storage,
                          public PrefStore::Observer {
 public:
  // |store| must outlive |this|.
  explicit ChromeStorageImpl(WriteablePrefStore* store);
  virtual ~ChromeStorageImpl();

  // ::i18n::addressinput::Storage implementation.
  virtual void Put(const std::string& key, std::string* data) override;
  virtual void Get(const std::string& key, const Callback& data_ready)
      const override;

  // PrefStore::Observer implementation.
  virtual void OnPrefValueChanged(const std::string& key) override;
  virtual void OnInitializationCompleted(bool succeeded) override;

 private:
  struct Request {
    Request(const std::string& key, const Callback& callback);

    std::string key;
    const Callback& callback;
  };

  // Non-const version of Get().
  void DoGet(const std::string& key, const Callback& data_ready);

  WriteablePrefStore* backing_store_;  // weak

  // Get requests that haven't yet been serviced.
  std::vector<std::unique_ptr<Request>> outstanding_requests_;

  ScopedObserver<PrefStore, PrefStore::Observer> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeStorageImpl);
};

}  // namespace autofill

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_
