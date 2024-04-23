// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_STORE_BACKEND_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_STORE_BACKEND_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace wolvic {

class WolvicPasswordStoreBackend
    : public password_manager::PasswordStoreBackend {
 public:
  WolvicPasswordStoreBackend();
  ~WolvicPasswordStoreBackend() override;

  WolvicPasswordStoreBackend(const WolvicPasswordStoreBackend&) = delete;
  WolvicPasswordStoreBackend& operator=(
    const WolvicPasswordStoreBackend&) = delete;

  void OnCompleteWithLogins(
      JNIEnv* env,
      int reply_id,
      const base::android::JavaParamRef<jobjectArray>& array);
  void OnLoginChanged(JNIEnv* env, int reply_id);
  void OnError(JNIEnv* env, int reply_id,
               const base::android::JavaParamRef<jstring> jError);

 private:
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Implements password_manager::PasswordStoreBackend interface.
  void InitBackend(
      password_manager::AffiliatedMatchHelper* affiliated_match_helper,
      RemoteChangesReceived remote_form_changes_received,
      base::RepeatingClosure sync_enabled_or_disabled_cb,
      base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  bool IsAbleToSavePasswords() override;
  void GetAllLoginsAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(
      password_manager::LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(
      std::string account,
      password_manager::LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      password_manager::LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<password_manager::PasswordFormDigest>& forms) override;
  void GetGroupedMatchingLoginsAsync(
      const password_manager::PasswordFormDigest& form_digest,
      password_manager::LoginsOrErrorReply callback) override;
  void AddLoginAsync(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  password_manager::SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  void RecordAddLoginAsyncCalledFromTheStore() override;
  void RecordUpdateLoginAsyncCalledFromTheStore() override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

  void GetAllLoginsInternal(password_manager::LoginsOrErrorReply callback);
  void GetLoginsAsync(
      const password_manager::PasswordFormDigest& form,
      bool include_psl,
      password_manager::LoginsOrErrorReply callback);
  void GetAutofillableLoginsAsyncInternal(
      password_manager::LoginsOrErrorReply callback);
  void RemoveLoginInternal(
      const password_manager::PasswordForm& form,
      password_manager::PasswordChangesOrErrorReply callback);
  void FilterAndRemoveLogins(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordChangesOrErrorReply reply,
      password_manager::LoginsResultOrError result);
  void FilterAndDisableAutoSignIn(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      password_manager::PasswordChangesOrErrorReply completion,
      password_manager::LoginsResultOrError result);

  template <typename T> void AddReplayCallback(T callback);
  template <typename T> absl::optional<T> GetAndEraseCallback(int reply_id);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  uint64_t reply_id_ = 0;
  // Using a small_map should ensure that we handle rare cases with many jobs
  // like a bulk deletion just as well as the normal, rather small job load.
  base::small_map<std::unordered_map<uint64_t, absl::variant<
      password_manager::LoginsOrErrorReply,
      password_manager::PasswordChangesOrErrorReply>>> reply_map_
          GUARDED_BY_CONTEXT(main_sequence_checker_);
  raw_ptr<password_manager::AffiliatedMatchHelper> affiliated_match_helper_;

  // TaskRunner to run responses on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<WolvicPasswordStoreBackend> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_PASSWORD_STORE_BACKEND_H_
