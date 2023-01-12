// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/google_account_access_token_fetch_delegate.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browsertests_jni/AccessTokenFetchTestBridge_jni.h"
#include "weblayer/test/weblayer_browsertests_jni/GoogleAccountAccessTokenFetcherTestStub_jni.h"

namespace weblayer {

// Callback passed to GoogleAccountAccessTokenFetcherDelegate to be invoked on
// access token fetch completion.
void OnAccessTokenFetched(base::OnceClosure quit_closure,
                          std::string* target_token,
                          const std::string& received_token) {
  *target_token = received_token;
  std::move(quit_closure).Run();
}

// Utility to convert an array of Java strings to a set of std::string.
std::set<std::string> ConvertJavaStringArrayToStringSet(
    const base::android::JavaRef<jobjectArray>& jstring_array) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<std::string> string_array;
  base::android::AppendJavaStringArrayToStringVector(env, jstring_array,
                                                     &string_array);

  return std::set<std::string>(string_array.begin(), string_array.end());
}

// Utility to return the scopes from the most recent access token request to
// |java_client_stub|.
std::set<std::string> GetMostRecentRequestScopes(
    base::android::ScopedJavaLocalRef<jobject> java_client_stub) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return ConvertJavaStringArrayToStringSet(
      Java_GoogleAccountAccessTokenFetcherTestStub_getMostRecentRequestScopes(
          env, java_client_stub));
}

// Utility to return the scopes from the most recent invalid access token
// notification to |java_client_stub|.
std::set<std::string> GetScopesForMostRecentInvalidToken(
    base::android::ScopedJavaLocalRef<jobject> java_client_stub) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return ConvertJavaStringArrayToStringSet(
      Java_GoogleAccountAccessTokenFetcherTestStub_getScopesForMostRecentInvalidToken(
          env, java_client_stub));
}

// Utility to return the token from the most recent invalid access token
// notification to |java_client_stub|.
std::string GetMostRecentInvalidToken(
    base::android::ScopedJavaLocalRef<jobject> java_client_stub) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return base::android::ConvertJavaStringToUTF8(
      env,
      Java_GoogleAccountAccessTokenFetcherTestStub_getMostRecentInvalidToken(
          env, java_client_stub));
}

// Tests proper operation of access token fetches initiated from C++ when the
// embedder has set a client for access token fetches in Java.
class AccessTokenFetchBrowserTest : public WebLayerBrowserTest {
 public:
  AccessTokenFetchBrowserTest() {}
  ~AccessTokenFetchBrowserTest() override = default;

 protected:
  ProfileImpl* profile() {
    auto* tab_impl = static_cast<TabImpl*>(shell()->tab());
    return tab_impl->profile();
  }

  GoogleAccountAccessTokenFetchDelegate* access_token_fetch_delegate() {
    return profile()->access_token_fetch_delegate();
  }
};

// Tests that when the embedder hasn't set a
// GoogleAccountAccessTokenFetcherClient instance in Java, access token fetches
// from C++ on Android result in the empty string being returned.
IN_PROC_BROWSER_TEST_F(AccessTokenFetchBrowserTest,
                       NoGoogleAccountAccessTokenFetcherClientSetInJava) {
  base::RunLoop run_loop;
  std::string access_token = "dummy";

  access_token_fetch_delegate()->FetchAccessToken(
      {"scope1"}, base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                                 &access_token));

  run_loop.Run();

  EXPECT_EQ("", access_token);
}

// Tests that when the embedder sets a Java client that returns a given access
// token, access token fetches initiated from C++ resolve to that token.
IN_PROC_BROWSER_TEST_F(AccessTokenFetchBrowserTest, SuccessfulFetch) {
  base::RunLoop run_loop;
  std::string access_token = "";
  std::string kTokenFromResponse = "token";
  std::set<std::string> kScopes = {"scope1", "scope2"};

  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> java_client_stub =
      Java_AccessTokenFetchTestBridge_installGoogleAccountAccessTokenFetcherTestStub(
          env, profile()->GetJavaProfile());

  access_token_fetch_delegate()->FetchAccessToken(
      kScopes, base::BindOnce(&OnAccessTokenFetched, run_loop.QuitClosure(),
                              &access_token));
  EXPECT_EQ("", access_token);

  EXPECT_EQ(
      1, Java_GoogleAccountAccessTokenFetcherTestStub_getNumOutstandingRequests(
             env, java_client_stub));
  int request_id =
      Java_GoogleAccountAccessTokenFetcherTestStub_getMostRecentRequestId(
          env, java_client_stub);

  auto most_recent_request_scopes =
      GetMostRecentRequestScopes(java_client_stub);
  EXPECT_EQ(kScopes, most_recent_request_scopes);

  Java_GoogleAccountAccessTokenFetcherTestStub_respondWithTokenForRequest(
      env, java_client_stub, request_id,
      base::android::ConvertUTF8ToJavaString(env, kTokenFromResponse));
  run_loop.Run();

  EXPECT_EQ(kTokenFromResponse, access_token);
}

// Tests correct operation when there are multiple ongoing fetches.
IN_PROC_BROWSER_TEST_F(AccessTokenFetchBrowserTest, MultipleFetches) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  std::string access_token1 = "";
  std::string access_token2 = "";
  std::string kTokenFromResponse1 = "token1";
  std::string kTokenFromResponse2 = "token2";
  std::set<std::string> kScopes1 = {"scope1", "scope2"};
  std::set<std::string> kScopes2 = {"scope3"};

  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> java_client_stub =
      Java_AccessTokenFetchTestBridge_installGoogleAccountAccessTokenFetcherTestStub(
          env, profile()->GetJavaProfile());

  access_token_fetch_delegate()->FetchAccessToken(
      kScopes1, base::BindOnce(&OnAccessTokenFetched, run_loop1.QuitClosure(),
                               &access_token1));
  EXPECT_EQ(
      1, Java_GoogleAccountAccessTokenFetcherTestStub_getNumOutstandingRequests(
             env, java_client_stub));
  int request_id1 =
      Java_GoogleAccountAccessTokenFetcherTestStub_getMostRecentRequestId(
          env, java_client_stub);
  auto most_recent_request_scopes1 =
      GetMostRecentRequestScopes(java_client_stub);
  EXPECT_EQ(kScopes1, most_recent_request_scopes1);

  EXPECT_EQ("", access_token1);
  EXPECT_EQ("", access_token2);

  access_token_fetch_delegate()->FetchAccessToken(
      kScopes2, base::BindOnce(&OnAccessTokenFetched, run_loop2.QuitClosure(),
                               &access_token2));

  EXPECT_EQ(
      2, Java_GoogleAccountAccessTokenFetcherTestStub_getNumOutstandingRequests(
             env, java_client_stub));
  int request_id2 =
      Java_GoogleAccountAccessTokenFetcherTestStub_getMostRecentRequestId(
          env, java_client_stub);
  auto most_recent_request_scopes2 =
      GetMostRecentRequestScopes(java_client_stub);
  EXPECT_EQ(kScopes2, most_recent_request_scopes2);

  EXPECT_EQ("", access_token1);
  EXPECT_EQ("", access_token2);

  Java_GoogleAccountAccessTokenFetcherTestStub_respondWithTokenForRequest(
      env, java_client_stub, request_id2,
      base::android::ConvertUTF8ToJavaString(env, kTokenFromResponse2));
  run_loop2.Run();

  EXPECT_EQ("", access_token1);
  EXPECT_EQ(kTokenFromResponse2, access_token2);

  Java_GoogleAccountAccessTokenFetcherTestStub_respondWithTokenForRequest(
      env, java_client_stub, request_id1,
      base::android::ConvertUTF8ToJavaString(env, kTokenFromResponse1));
  run_loop1.Run();

  EXPECT_EQ(kTokenFromResponse1, access_token1);
  EXPECT_EQ(kTokenFromResponse2, access_token2);
}

// Tests that when the C++ delegate is notified that an access token has been
// identified as invalid, that information is forwarded to the Java side.
IN_PROC_BROWSER_TEST_F(AccessTokenFetchBrowserTest,
                       AccessTokenIdentifiedAsInvalid) {
  const std::string kInvalidToken = "token";
  const std::set<std::string> kScopesForInvalidToken = {"scope1", "scope2"};

  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> java_client_stub =
      Java_AccessTokenFetchTestBridge_installGoogleAccountAccessTokenFetcherTestStub(
          env, profile()->GetJavaProfile());

  EXPECT_EQ(std::set<std::string>(),
            GetScopesForMostRecentInvalidToken(java_client_stub));
  EXPECT_EQ("", GetMostRecentInvalidToken(java_client_stub));

  access_token_fetch_delegate()->OnAccessTokenIdentifiedAsInvalid(
      kScopesForInvalidToken, kInvalidToken);

  EXPECT_EQ(kScopesForInvalidToken,
            GetScopesForMostRecentInvalidToken(java_client_stub));
  EXPECT_EQ(kInvalidToken, GetMostRecentInvalidToken(java_client_stub));
}

}  // namespace weblayer
