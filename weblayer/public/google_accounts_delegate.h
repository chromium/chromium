// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_GOOGLE_ACCOUNTS_DELEGATE_H_
#define WEBLAYER_PUBLIC_GOOGLE_ACCOUNTS_DELEGATE_H_

#include <string>

namespace signin {
struct ManageAccountsParams;
}

namespace weblayer {

// Used to intercept interaction with GAIA accounts.
class GoogleAccountsDelegate {
 public:
  // Called when a user wants to change the state of their GAIA account. This
  // could be a signin, signout, or any other action. See
  // signin::GAIAServiceType for all the possible actions.
  virtual void OnGoogleAccountsRequest(
      const signin::ManageAccountsParams& params) = 0;

  // The current GAIA ID the user is signed in with, or empty if the user is
  // signed out. This can be provided on a best effort basis if the ID is not
  // available immediately.
  virtual std::string GetGaiaId() = 0;

 protected:
  virtual ~GoogleAccountsDelegate() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_GOOGLE_ACCOUNTS_DELEGATE_H_
