// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"

#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/public/google_account_access_token_fetch_delegate.h"

namespace weblayer {
namespace {

// Callback passed to SafeBrowsingTokenFetcherImpl to be invoked on
// access token fetch completion.
void OnAccessTokenFetched(base::OnceClosure quit_closure,
                          std::string* target_token,
                          const std::string& received_token) {
  *target_token = received_token;
  std::move(quit_closure).Run();
}

// Test implementation of GoogleAccountAccessTokenFetchDelegate.
class TestAccessTokenFetchDelegate
    : public GoogleAccountAccessTokenFetchDelegate {
 public:
  TestAccessTokenFetchDelegate() {}
  ~TestAccessTokenFetchDelegate() override {}

  TestAccessTokenFetchDelegate(const TestAccessTokenFetchDelegate&) = delete;
  TestAccessTokenFetchDelegate& operator=(const TestAccessTokenFetchDelegate&) =
      delete;

  // GoogleAccountAccessTokenFetchDelegate:
  void FetchAccessToken(const std::set<std::string>& scopes,
                        OnTokenFetchedCallback callback) override {
    most_recent_request_id_++;

    // All access token requests made by SafeBrowsingTokenFetcherImpl should be
    // for the safe browsing scope.
    std::set<std::string> expected_scopes = {
        GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
    EXPECT_EQ(expected_scopes, scopes);

    outstanding_callbacks_[most_recent_request_id_] = std::move(callback);
  }

  void OnAccessTokenIdentifiedAsInvalid(const std::set<std::string>& scopes,
                                        const std::string& token) override {
    // All invalid token notifications originating from
    // SafeBrowsingTokenFetcherImpl should be for the safe browsing scope.
    std::set<std::string> expected_scopes = {
        GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
    EXPECT_EQ(expected_scopes, scopes);

    invalid_token_ = token;
  }

  int get_num_outstanding_requests() { return outstanding_callbacks_.size(); }

  int get_most_recent_request_id() { return most_recent_request_id_; }

  const std::string& get_most_recent_invalid_token() { return invalid_token_; }

  void RespondWithTokenForRequest(int request_id, const std::string& token) {
    ASSERT_TRUE(outstanding_callbacks_.count(request_id));

    auto callback = std::move(outstanding_callbacks_[request_id]);
    outstanding_callbacks_.erase(request_id);

    std::move(callback).Run(token);
  }

 private:
  int most_recent_request_id_ = 0;
  std::map<int, OnTokenFetchedCallback> outstanding_callbacks_;
  std::string invalid_token_;
};

}  // namespace

class SafeBrowsingTokenFetcherImplTest : public testing::Test {
 public:
  SafeBrowsingTokenFetcherImplTest() = default;

  SafeBrowsingTokenFetcherImplTest(const SafeBrowsingTokenFetcherImplTest&) =
      delete;
  SafeBrowsingTokenFetcherImplTest& operator=(
      const SafeBrowsingTokenFetcherImplTest&) = delete;

 protected:
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that SafeBrowsingTokenFetcherImpl responds with an empty token when
// there is no delegate available to fetch tokens from.
TEST_F(SafeBrowsingTokenFetcherImplTest, NoDelegate) {
  base::RunLoop run_loop;
  std::string access_token = "dummy";

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      []() -> GoogleAccountAccessTokenFetchDelegate* { return nullptr; }));

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                               &access_token));

  run_loop.Run();
  EXPECT_EQ("", access_token);
}

TEST_F(SafeBrowsingTokenFetcherImplTest, SuccessfulTokenFetch) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop;
  std::string access_token = "";
  std::string kTokenFromResponse = "token";

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                               &access_token));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("", access_token);

  delegate.RespondWithTokenForRequest(delegate.get_most_recent_request_id(),
                                      kTokenFromResponse);

  run_loop.Run();
  EXPECT_EQ(kTokenFromResponse, access_token);
}

// Verifies that destruction of a SafeBrowsingTokenFetcherImpl instance from
// within the client callback that the token was fetched doesn't cause a crash.
TEST_F(SafeBrowsingTokenFetcherImplTest,
       FetcherDestroyedFromWithinOnTokenFetchedCallback) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop;
  std::string access_token = "";
  std::string kTokenFromResponse = "token";

  // Destroyed in the token fetch callback.
  auto* fetcher = new SafeBrowsingTokenFetcherImpl(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher->Start(base::BindOnce(
      [](base::OnceClosure quit_closure, std::string* target_token,
         SafeBrowsingTokenFetcherImpl* fetcher, const std::string& token) {
        *target_token = token;
        delete fetcher;

        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &access_token, fetcher));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("", access_token);

  delegate.RespondWithTokenForRequest(delegate.get_most_recent_request_id(),
                                      kTokenFromResponse);

  run_loop.Run();
  EXPECT_EQ(kTokenFromResponse, access_token);
}

// Tests correct operation in the case of concurrent requests to
// SafeBrowsingTokenFetcherImpl.
TEST_F(SafeBrowsingTokenFetcherImplTest, ConcurrentRequests) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  std::string access_token1 = "";
  std::string access_token2 = "";
  std::string kTokenFromResponse1 = "token1";
  std::string kTokenFromResponse2 = "token2";

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop1.QuitClosure(),
                               &access_token1));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  int request_id1 = delegate.get_most_recent_request_id();

  EXPECT_EQ("", access_token1);
  EXPECT_EQ("", access_token2);

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop2.QuitClosure(),
                               &access_token2));
  EXPECT_EQ(2, delegate.get_num_outstanding_requests());
  int request_id2 = delegate.get_most_recent_request_id();

  EXPECT_EQ("", access_token1);
  EXPECT_EQ("", access_token2);

  delegate.RespondWithTokenForRequest(request_id2, kTokenFromResponse2);

  run_loop2.Run();
  EXPECT_EQ("", access_token1);
  EXPECT_EQ(kTokenFromResponse2, access_token2);

  delegate.RespondWithTokenForRequest(request_id1, kTokenFromResponse1);

  run_loop1.Run();
  EXPECT_EQ(kTokenFromResponse1, access_token1);
  EXPECT_EQ(kTokenFromResponse2, access_token2);
}

TEST_F(SafeBrowsingTokenFetcherImplTest, TokenFetchTimeout) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop;
  std::string access_token = "dummy";
  std::string kTokenFromResponse = "token";

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                               &access_token));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("dummy", access_token);

  // Fast-forward to trigger the token fetch timeout.
  task_environment()->FastForwardBy(base::Milliseconds(
      safe_browsing::kTokenFetchTimeoutDelayFromMilliseconds));

  // Even though the delegate has not yet responded,
  // SafeBrowsingTokenFetcherImpl should have responded to its request with the
  // empty token.
  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("", access_token);

  // Check that the delegate responding at this point has no adverse effect.
  delegate.RespondWithTokenForRequest(delegate.get_most_recent_request_id(),
                                      kTokenFromResponse);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", access_token);
}

// Verifies that destruction of a SafeBrowsingTokenFetcherImpl instance from
// within the client callback that the token was fetched doesn't cause a crash
// when invoked due to the token fetch timing out.
TEST_F(SafeBrowsingTokenFetcherImplTest,
       FetcherDestroyedFromWithinOnTokenFetchedCallbackInvokedOnTimeout) {
  TestAccessTokenFetchDelegate delegate;
  std::string access_token;
  bool callback_invoked = false;

  // Destroyed in the token fetch callback, which is invoked on timeout.
  auto* fetcher = new SafeBrowsingTokenFetcherImpl(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher->Start(base::BindOnce(
      [](bool* on_invoked_flag, std::string* target_token,
         SafeBrowsingTokenFetcherImpl* fetcher, const std::string& token) {
        *on_invoked_flag = true;
        *target_token = token;
        delete fetcher;
      },
      &callback_invoked, &access_token, fetcher));

  // Trigger a timeout of the fetch, which will invoke the client callback
  // passed to the fetcher.
  task_environment()->FastForwardBy(base::Milliseconds(
      safe_browsing::kTokenFetchTimeoutDelayFromMilliseconds));
  ASSERT_TRUE(callback_invoked);
  ASSERT_TRUE(access_token.empty());
}

TEST_F(SafeBrowsingTokenFetcherImplTest, FetcherDestroyedBeforeFetchReturns) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop;
  std::string access_token = "dummy";
  std::string kTokenFromResponse = "token";

  auto fetcher =
      std::make_unique<SafeBrowsingTokenFetcherImpl>(base::BindRepeating(
          [](TestAccessTokenFetchDelegate* delegate)
              -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
          &delegate));

  fetcher->Start(base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                                &access_token));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("dummy", access_token);

  fetcher.reset();

  // The fetcher should have responded to the outstanding request with the empty
  // token on its destruction.
  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  EXPECT_EQ("", access_token);

  // Check that the delegate responding at this point has no adverse effect.
  delegate.RespondWithTokenForRequest(delegate.get_most_recent_request_id(),
                                      kTokenFromResponse);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", access_token);
}

// Tests correct operation in the case of concurrent requests to
// SafeBrowsingTokenFetcherImpl made at different times, with an earlier one
// timing out and a later one being fulfilled.
TEST_F(SafeBrowsingTokenFetcherImplTest, ConcurrentRequestsAtDifferentTimes) {
  TestAccessTokenFetchDelegate delegate;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  std::string access_token1 = "dummy";
  std::string access_token2 = "dummy";
  std::string kTokenFromResponse1 = "token1";
  std::string kTokenFromResponse2 = "token2";
  int delay_before_second_request_from_ms =
      safe_browsing::kTokenFetchTimeoutDelayFromMilliseconds / 2;

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop1.QuitClosure(),
                               &access_token1));

  EXPECT_EQ(1, delegate.get_num_outstanding_requests());
  int request_id1 = delegate.get_most_recent_request_id();

  EXPECT_EQ("dummy", access_token1);
  EXPECT_EQ("dummy", access_token2);

  task_environment()->FastForwardBy(
      base::Milliseconds(delay_before_second_request_from_ms));
  fetcher.Start(base::BindOnce(&OnAccessTokenFetched, run_loop2.QuitClosure(),
                               &access_token2));
  EXPECT_EQ(2, delegate.get_num_outstanding_requests());
  int request_id2 = delegate.get_most_recent_request_id();

  EXPECT_EQ("dummy", access_token1);
  EXPECT_EQ("dummy", access_token2);

  // Fast-forward to trigger the first request's timeout threshold, but not the
  // second.
  int time_to_trigger_first_timeout_from_ms =
      safe_browsing::kTokenFetchTimeoutDelayFromMilliseconds -
      delay_before_second_request_from_ms;
  task_environment()->FastForwardBy(
      base::Milliseconds(time_to_trigger_first_timeout_from_ms));

  // Verify that the first request's timeout was handled by
  // SafeBrowsingTokenFetcherImpl.
  EXPECT_EQ(2, delegate.get_num_outstanding_requests());
  EXPECT_EQ("", access_token1);
  EXPECT_EQ("dummy", access_token2);

  // Verify that the second request can still be fulfilled and that there is no
  // adverse effect from the delegate now responding to the first request.
  delegate.RespondWithTokenForRequest(request_id1, kTokenFromResponse1);
  delegate.RespondWithTokenForRequest(request_id2, kTokenFromResponse2);

  run_loop2.Run();
  EXPECT_EQ("", access_token1);
  EXPECT_EQ(kTokenFromResponse2, access_token2);
}

// Tests that the fetcher calls through to GoogleAccountAccessTokenFetchDelegate
// on being notified of an invalid token.
TEST_F(SafeBrowsingTokenFetcherImplTest, OnInvalidAccessToken) {
  TestAccessTokenFetchDelegate delegate;
  const std::string kInvalidToken = "dummy";

  SafeBrowsingTokenFetcherImpl fetcher(base::BindRepeating(
      [](TestAccessTokenFetchDelegate* delegate)
          -> GoogleAccountAccessTokenFetchDelegate* { return delegate; },
      &delegate));

  EXPECT_EQ("", delegate.get_most_recent_invalid_token());

  fetcher.OnInvalidAccessToken(kInvalidToken);

  EXPECT_EQ(kInvalidToken, delegate.get_most_recent_invalid_token());
}

}  // namespace weblayer
