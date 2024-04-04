// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_password_store_backend.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"
#include "wolvic/browser/autocomplete/wolvic_password_form_util.h"
#include "wolvic/jni_headers/PasswordStoreBackend_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace wolvic {

namespace {
// Time in seconds by which calls to the password store happening on startup
// should be delayed.
constexpr base::TimeDelta kPasswordStoreCallDelaySeconds = base::Seconds(5);

std::string FormToSignonRealmQuery(
    const password_manager::PasswordFormDigest& form, bool include_psl) {
  if (include_psl) {
    // Check PSL matches and matches for exact signon realm.
    return password_manager::GetRegistryControlledDomain(GURL(form.signon_realm));
  }
  if (form.scheme == password_manager::PasswordForm::Scheme::kHtml &&
      !password_manager::IsValidAndroidFacetURI(form.signon_realm)) {
    // Check federated matches and matches for exact signon realm.
    return form.url.host();
  }
  // Check matches for exact signon realm.
  return form.signon_realm;
}

password_manager::LoginsResultOrError JoinRetrievedLoginsOrError(
    std::vector<password_manager::LoginsResultOrError> results) {
  password_manager::LoginsResult joined_logins;
  for (auto& result : results) {
    // If one of retrievals ended with an error, pass on the error.
    if (absl::holds_alternative<
        password_manager::PasswordStoreBackendError>(result)) {
      return std::move(absl::get<
              password_manager::PasswordStoreBackendError>(result));
    }
    password_manager::LoginsResult logins =
        std::move(absl::get<password_manager::LoginsResult>(result));
    std::move(logins.begin(), logins.end(), std::back_inserter(joined_logins));
  }
  return joined_logins;
}

}  // namespace

WolvicPasswordStoreBackend::WolvicPasswordStoreBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  java_obj_ = Java_PasswordStoreBackend_create(
      AttachCurrentThread(),  reinterpret_cast<intptr_t>(this));
}

WolvicPasswordStoreBackend::~WolvicPasswordStoreBackend()  {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Java_PasswordStoreBackend_destroy(AttachCurrentThread(), java_obj_);
}

void WolvicPasswordStoreBackend::OnCompleteWithLogins(
    JNIEnv* env,
    int reply_id,
    const base::android::JavaParamRef<jobjectArray>& array) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  password_manager::LoginsResult passwords;
  if (array) {
    size_t length = base::android::SafeGetArrayLength(
              base::android::AttachCurrentThread(), array);
    for (size_t i = 0; i < length; ++i) {
      ScopedJavaLocalRef<jobject> j_password_form(
          env,
          static_cast<jobject>(env->GetObjectArrayElement(array.obj(), i)));
      passwords.push_back(GetPasswordFormFromJavaObject(env, j_password_form));
    }
  }

  auto reply =
      GetAndEraseCallback<password_manager::LoginsOrErrorReply>(reply_id);
  if (!reply.has_value()) {
    LOG(ERROR) << "OnCompleteWithLogins does not have reply in map by id["
               << reply_id << "]";
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*reply), std::move(passwords)));
}

void WolvicPasswordStoreBackend::OnLoginChanged(JNIEnv* env, int reply_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto reply =
      GetAndEraseCallback<password_manager::PasswordChangesOrErrorReply>(
          reply_id);

  if (!reply.has_value()) {
    LOG(ERROR) << "OnLoginChanged does not have reply in map by id["
               << reply_id << "]";
    return;
  }
  main_task_runner_->PostTask(FROM_HERE,
      base::BindOnce(std::move(*reply), absl::nullopt));
}

void WolvicPasswordStoreBackend::OnError(
    JNIEnv* env,
    int reply_id,
    const base::android::JavaParamRef<jstring> jError) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  std::string error = base::android::ConvertJavaStringToUTF8(jError);
  LOG(ERROR) << "PasswordStoreAndroidBackend::OnError message=" + error;

  auto iter = reply_map_.find(reply_id);
  if (iter == reply_map_.end()) {
    LOG(ERROR) << "OnError does not have reply in map by id["
               << reply_id << "]";
    return;
  }

  auto reported_error = password_manager::PasswordStoreBackendError(
      password_manager::PasswordStoreBackendErrorType::kUncategorized,
      password_manager::PasswordStoreBackendErrorRecoveryType::kUnrecoverable);



  if (absl::holds_alternative<
      password_manager::LoginsOrErrorReply>(iter->second)) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(absl::get<password_manager::LoginsOrErrorReply>(
            std::move(iter->second)), reported_error));
  } else if (absl::holds_alternative<
      password_manager::PasswordChangesOrErrorReply>(iter->second)) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            absl::get<password_manager::PasswordChangesOrErrorReply>(
            std::move(iter->second)), reported_error));
  } else {
    NOTREACHED();
  }
  reply_map_.erase(iter);
}

void WolvicPasswordStoreBackend::InitBackend(
    password_manager::AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  affiliated_match_helper_ = affiliated_match_helper;
  std::move(completion).Run(/*success=*/true);

  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  // Calling the remote form changes with a nullopt means that changes are not
  // available and the store should request all logins asynchronously to
  // invoke `PasswordStoreInterface::Observer::OnLoginsRetained`.
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(remote_form_changes_received, absl::nullopt),
      kPasswordStoreCallDelaySeconds);
}

void WolvicPasswordStoreBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  affiliated_match_helper_ = nullptr;
  std::move(shutdown_completed).Run();
}

void WolvicPasswordStoreBackend::GetAllLoginsAsync(
    password_manager::LoginsOrErrorReply callback) {
  GetAllLoginsInternal(std::move(callback));
}

void WolvicPasswordStoreBackend::GetAutofillableLoginsAsync(
    password_manager::LoginsOrErrorReply callback) {
  GetAutofillableLoginsAsyncInternal(std::move(callback));
}

void WolvicPasswordStoreBackend::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    password_manager::LoginsOrErrorReply callback) {
  GetAllLoginsAsync(std::move(callback));
}

void WolvicPasswordStoreBackend::FillMatchingLoginsAsync(
    password_manager::LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<password_manager::PasswordFormDigest>& forms) {
  if (forms.empty()) {
    std::move(callback).Run(password_manager::LoginsResult());
    return;
  }

  // Create a barrier callback that aggregates results of a multiple
  // calls to GetLoginsAsync.
  auto barrier_callback =
      base::BarrierCallback<password_manager::LoginsResultOrError>(
          forms.size(), base::BindOnce(&JoinRetrievedLoginsOrError)
                            .Then(std::move(callback)));

  // Create and run a callbacks chain that retrieves logins and invokes
  // |barrier_callback| afterwards.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const auto& form : forms) {
    callbacks_chain = base::BindOnce(
        &WolvicPasswordStoreBackend::GetLoginsAsync,
        weak_ptr_factory_.GetWeakPtr(), std::move(form), include_psl,
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void WolvicPasswordStoreBackend::GetGroupedMatchingLoginsAsync(
    const password_manager::PasswordFormDigest& form_digest,
    password_manager::LoginsOrErrorReply callback) {
  DCHECK(affiliated_match_helper_);
  GetLoginsWithAffiliationsRequestHandler(
      form_digest, this, affiliated_match_helper_.get(), std::move(callback));
}

void WolvicPasswordStoreBackend::AddLoginAsync(
    const password_manager::PasswordForm& form,
    password_manager::PasswordChangesOrErrorReply callback) {
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_addLogin(
      env, java_obj_, reply_id_, CreatePasswordFormJavaObject(env, form));
}

void WolvicPasswordStoreBackend::UpdateLoginAsync(
    const password_manager::PasswordForm& form,
    password_manager::PasswordChangesOrErrorReply callback) {
  DCHECK(!form.blocked_by_user ||
         (form.username_value.empty() && form.password_value.empty()));
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_updateLogin(
      env, java_obj_, reply_id_, CreatePasswordFormJavaObject(env, form));
}

void WolvicPasswordStoreBackend::RemoveLoginAsync(
    const password_manager::PasswordForm& form,
    password_manager::PasswordChangesOrErrorReply callback) {
  RemoveLoginInternal(form, std::move(callback));
}

void WolvicPasswordStoreBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    password_manager::PasswordChangesOrErrorReply callback) {
  GetAllLoginsInternal(
      base::BindOnce(&WolvicPasswordStoreBackend::FilterAndRemoveLogins,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_filter),
                     delete_begin, delete_end,std::move(callback)));
}

void WolvicPasswordStoreBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordChangesOrErrorReply callback) {
  GetAllLoginsInternal(
      base::BindOnce(&WolvicPasswordStoreBackend::FilterAndRemoveLogins,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Include all urls.
                     base::BindRepeating([](const GURL&) { return true; }),
                     delete_begin, delete_end,
                     std::move(callback)));
}

void WolvicPasswordStoreBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  password_manager::PasswordChangesOrErrorReply wrap_completion =
      base::BindOnce(
          [](base::OnceClosure completion,
             password_manager::PasswordChangesOrError changes) {
            std::move(completion).Run();
          }, std::move(completion));

  GetAllLoginsInternal(
      base::BindOnce(&WolvicPasswordStoreBackend::FilterAndDisableAutoSignIn,
                     weak_ptr_factory_.GetWeakPtr(), origin_filter,
                     std::move(wrap_completion)));
}

password_manager::SmartBubbleStatsStore*
WolvicPasswordStoreBackend::GetSmartBubbleStatsStore() {
  // Not implemented
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
WolvicPasswordStoreBackend::CreateSyncControllerDelegate() {
  // Not implemented
  return nullptr;
}

void WolvicPasswordStoreBackend::ClearAllLocalPasswords() {
  password_manager::LoginsOrErrorReply cleaning_callback = base::BindOnce(
      [](base::WeakPtr<WolvicPasswordStoreBackend> weak_self,
         password_manager::LoginsResultOrError logins_or_error) {
        if (!weak_self ||
            absl::holds_alternative<
                password_manager::PasswordStoreBackendError>(logins_or_error)) {
          return;
        }

        base::OnceClosure callbacks_chain = base::DoNothing();
        for (const auto& login :
            absl::get<password_manager::LoginsResult>(logins_or_error)) {
          base::OnceCallback removal_result =
              base::BindOnce(
                    [](password_manager::PasswordChangesOrError change_list) {
                if (absl::holds_alternative<
                    password_manager::PasswordStoreBackendError>(change_list)) {
                  LOG(ERROR) << "Failure to remove by error";
                } else if (absl::get<password_manager::PasswordChanges>(change_list)
                            .value_or(password_manager::PasswordStoreChangeList())
                            .empty()) {
                  LOG(ERROR) << "Failure to remove by empty result";
                }
              });;
          callbacks_chain = base::BindOnce(
              &WolvicPasswordStoreBackend::RemoveLoginInternal,
              weak_self, std::move(*login),
              std::move(removal_result).Then(std::move(callbacks_chain)));
        }

        std::move(callbacks_chain).Run();
      },
      weak_ptr_factory_.GetWeakPtr());

  GetAllLoginsInternal(std::move(cleaning_callback));
}

void WolvicPasswordStoreBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {}

void WolvicPasswordStoreBackend::GetAllLoginsInternal(
    password_manager::LoginsOrErrorReply callback) {
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_getAllLogins(env, java_obj_, reply_id_);
}

void WolvicPasswordStoreBackend::GetLoginsAsync(
    const password_manager::PasswordFormDigest& form,
    bool include_psl,
    password_manager::LoginsOrErrorReply callback) {
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_getLoginsForSignonRealm(
      env, java_obj_, reply_id_,
      base::android::ConvertUTF8ToJavaString(
          env, FormToSignonRealmQuery(form, include_psl)));
}

void WolvicPasswordStoreBackend::GetAutofillableLoginsAsyncInternal(
    password_manager::LoginsOrErrorReply callback) {
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_getAutofillableLogins(env, java_obj_, reply_id_);
}

void WolvicPasswordStoreBackend::RemoveLoginInternal(
    const password_manager::PasswordForm& form,
    password_manager::PasswordChangesOrErrorReply callback) {
  AddReplayCallback(std::move(callback));

  JNIEnv* env = AttachCurrentThread();
  Java_PasswordStoreBackend_removeLogin(
      env, java_obj_, reply_id_, CreatePasswordFormJavaObject(env, form));
}

void WolvicPasswordStoreBackend::FilterAndRemoveLogins(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordChangesOrErrorReply reply,
    password_manager::LoginsResultOrError result) {
  if (absl::holds_alternative<
      password_manager::PasswordStoreBackendError>(result)) {
    std::move(reply).Run(
        std::move(absl::get<
            password_manager::PasswordStoreBackendError>(result)));
    return;
  }

  password_manager::LoginsResult logins =
      std::move(absl::get<password_manager::LoginsResult>(result));
  std::vector<password_manager::PasswordForm> logins_to_remove;
  for (const auto& login : logins) {
    if (login->date_created >= delete_begin &&
        login->date_created < delete_end && url_filter.Run(login->url)) {
      logins_to_remove.push_back(std::move(*login));
    }
  }

  // Create a barrier callback that aggregates results of a multiple
  // calls to RemoveLoginAsync.
  auto barrier_callback =
      base::BarrierCallback<password_manager::PasswordChangesOrError>(
          logins_to_remove.size(),
          base::BindOnce(&password_manager::JoinPasswordStoreChanges)
              .Then(std::move(reply)));

  // Create and run the callback chain that removes the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const auto& login : logins_to_remove) {
    callbacks_chain = base::BindOnce(
        &WolvicPasswordStoreBackend::RemoveLoginInternal,
        weak_ptr_factory_.GetWeakPtr(), std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void WolvicPasswordStoreBackend::FilterAndDisableAutoSignIn(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    password_manager::PasswordChangesOrErrorReply completion,
    password_manager::LoginsResultOrError result) {
  if (absl::holds_alternative<
      password_manager::PasswordStoreBackendError>(result)) {
    std::move(completion)
        .Run(std::move(
                absl::get<password_manager::PasswordStoreBackendError>(result)));
    return;
  }

  password_manager::LoginsResult logins =
      std::move(absl::get<password_manager::LoginsResult>(result));
  std::vector<password_manager::PasswordForm> logins_to_update;
  for (std::unique_ptr<password_manager::PasswordForm>& login : logins) {
    // Update login if it matches |origin_filer| and has autosignin enabled.
    if (origin_filter.Run(login->url) && !login->skip_zero_click) {
      logins_to_update.push_back(std::move(*login));
      logins_to_update.back().skip_zero_click = true;
    }
  }

  auto barrier_callback =
      base::BarrierCallback<password_manager::PasswordChangesOrError>(
          logins_to_update.size(),
          base::BindOnce(&password_manager::JoinPasswordStoreChanges)
              .Then(std::move(completion)));

  // Create and run a callbacks chain that updates the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (password_manager::PasswordForm& login : logins_to_update) {
    callbacks_chain = base::BindOnce(
        &WolvicPasswordStoreBackend::UpdateLoginAsync,
        weak_ptr_factory_.GetWeakPtr(), std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

template <typename T>
void WolvicPasswordStoreBackend::AddReplayCallback(T callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  reply_map_.emplace(++reply_id_, std::move(callback));
}

template <typename T>
absl::optional<T> WolvicPasswordStoreBackend::GetAndEraseCallback(int reply_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto iter = reply_map_.find(reply_id);
  if (iter == reply_map_.end())
    return absl::nullopt;

  absl::optional<T> reply = absl::get<T>(std::move(iter->second));
  reply_map_.erase(iter);
  return reply;
}

}  // namespace password_manager
