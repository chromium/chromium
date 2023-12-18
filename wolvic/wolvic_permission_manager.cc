// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_permission_manager.h"

#include <jni.h>
#include <algorithm>
#include <memory>
#include <span>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "wolvic/jni_headers/PermissionManagerBridge_jni.h"

namespace wolvic {
namespace {

// Has to be kept in sync with PermissionManagerBridge.java
enum class WolvicPermissionType {
  kGeolocation = 0,
  kDesktopNotification = 1,
  kPersistentStorage = 2,
  kXr = 3,
  kAutoplayInaudible = 4,
  kAutoplayAudible = 5,
  kMediaKeySystemAccess = 6,
  kTracking = 7,
  kStorageAccess = 8,

  kNotSupported = 9,
};

// Has to be kept in sync with PermissionManagerBridge.java
enum class WolvicPermissionStatus {
  kPrompt = 0,
  kDeny = 1,
  kAllow = 2,
};

WolvicPermissionType ToWolvicPermissionType(blink::PermissionType permission) {
  switch (permission) {
    case blink::PermissionType::GEOLOCATION:
      return WolvicPermissionType::kGeolocation;
    case blink::PermissionType::NOTIFICATIONS:
      return WolvicPermissionType::kDesktopNotification;
    case blink::PermissionType::DURABLE_STORAGE:
      return WolvicPermissionType::kPersistentStorage;
    case blink::PermissionType::VR:
    case blink::PermissionType::AR:
      return WolvicPermissionType::kXr;
    case blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return WolvicPermissionType::kMediaKeySystemAccess;
    case blink::PermissionType::STORAGE_ACCESS_GRANT:
      return WolvicPermissionType::kStorageAccess;
    default:
      // TODO(voit): Where to we get kAutoplayInaudible, kAutoplayAudible and
      // kTracking from?
      return WolvicPermissionType::kNotSupported;
  }

  NOTREACHED();
  return WolvicPermissionType::kNotSupported;
}

// These strings are taken from the android.Manifest class shipped with the
// Android SDK.
std::string ToAndroidPermission(blink::PermissionType permission) {
  switch (permission) {
    case blink::PermissionType::GEOLOCATION:
      return "android.permission.ACCESS_FINE_LOCATION";
    case blink::PermissionType::AUDIO_CAPTURE:
      return "android.permission.RECORD_AUDIO";
    case blink::PermissionType::VIDEO_CAPTURE:
      return "android.permission.CAMERA";
    default:
      // This is a custom permission type defined in
      // PermissionManagerBridge.java. It's used to tell Wolvic that the given
      // permission type doesn't require any Android permissions.
      return "org.chromium.wolvic.NO_ANDROID_PERMISSION";
  }
}

content::PermissionStatus FromWolvicPermissionStatus(
    WolvicPermissionStatus status) {
  switch (status) {
    case WolvicPermissionStatus::kPrompt:
      return content::PermissionStatus::ASK;
    case WolvicPermissionStatus::kDeny:
      return content::PermissionStatus::DENIED;
    case WolvicPermissionStatus::kAllow:
      return content::PermissionStatus::GRANTED;
  }

  NOTREACHED();
  return content::PermissionStatus::DENIED;
}

std::vector<int> ToJavaWolvicPermissionTypes(
    const std::vector<blink::PermissionType>& permissions) {
  std::vector<int> result(permissions.size());
  for (size_t i = 0; i < permissions.size(); ++i) {
    result[i] = static_cast<int>(ToWolvicPermissionType(permissions[i]));
  }
  return result;
}

std::vector<std::string> ToAndroidPermissionTypes(
    const std::vector<blink::PermissionType>& permissions) {
  std::vector<std::string> result(permissions.size());
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = ToAndroidPermission(permissions[i]);
  }
  return result;
}

std::vector<content::PermissionStatus> FromJavaWolvicPermissionStatuses(
    const std::vector<int>& statuses) {
  std::vector<content::PermissionStatus> result(statuses.size());
  for (size_t i = 0; i < statuses.size(); ++i) {
    result[i] = FromWolvicPermissionStatus(
        static_cast<WolvicPermissionStatus>(statuses[i]));
  }
  return result;
}

wolvic::WolvicPermissionManager* g_instance = nullptr;
wolvic::WolvicPermissionManager* g_off_the_record_instance = nullptr;

WolvicPermissionManager* GetPermissionManager() {
  DCHECK(g_instance);
  return g_instance;
}

WolvicPermissionManager* GetOffTheRecordPermissionManager() {
  DCHECK(g_off_the_record_instance);
  return g_off_the_record_instance;
}

}  // namespace

InProgressRequest::InProgressRequest(
    const content::PermissionRequestDescription& description,
    base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
        callback)
    : description(description) {
  callbacks.emplace_back(std::move(callback));
}

InProgressRequest::~InProgressRequest() = default;

WolvicPermissionManager::WolvicPermissionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  if (browser_context_->IsOffTheRecord()) {
    DCHECK(!g_off_the_record_instance);
    g_off_the_record_instance = this;
  } else {
    DCHECK(!g_instance);
    g_instance = this;
  }
}

WolvicPermissionManager::~WolvicPermissionManager() = default;

void WolvicPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        request_description.permissions.size(),
        blink::mojom::PermissionStatus::DENIED));
    return;
  }

  // If the same permission request is already in progress, we don't want to
  // start another user prompt. Instead, we attach callback to the same existing
  // request.
  auto* existing_request = FindInProgressRequest(request_description);
  if (existing_request) {
    existing_request->callbacks.emplace_back(std::move(callback));
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  in_progress_requests_.emplace_back(std::make_unique<InProgressRequest>(
      request_description, std::move(callback)));

  RequestContentPermissions(env, in_progress_requests_.back().get(),
                            request_description);
}

void WolvicPermissionManager::ResetPermission(blink::PermissionType permission,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin) {}

void WolvicPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
        callback) {
  RequestPermissions(render_frame_host, request_description,
                     std::move(callback));
}

blink::mojom::PermissionStatus WolvicPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // We don't save any permissions statuses on Chromium side, so return 'ASK'
  // here to make sure that any permissions are requested from Wolvic.
  return blink::mojom::PermissionStatus::ASK;
}

content::PermissionResult
WolvicPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status = GetPermissionStatus(
      permission, requesting_origin.GetURL(), embedding_origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
WolvicPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    return blink::mojom::PermissionStatus::DENIED;
  }
  return GetPermissionStatus(
      permission,
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host),
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()));
}

blink::mojom::PermissionStatus
WolvicPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatus(permission, worker_origin, worker_origin);
}

blink::mojom::PermissionStatus
WolvicPermissionManager::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    return blink::mojom::PermissionStatus::DENIED;
  }
  return GetPermissionStatus(
      permission, overridden_origin.GetURL(),
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()));
}

WolvicPermissionManager::SubscriptionId
WolvicPermissionManager::SubscribePermissionStatusChange(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void WolvicPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

void WolvicPermissionManager::OnPermissionResult(
    InProgressRequest* in_progress_request,
    const std::vector<content::PermissionStatus>& result) {
  auto it = std::find_if(in_progress_requests_.begin(),
                         in_progress_requests_.end(), [&](const auto& request) {
                           return request.get() == in_progress_request;
                         });
  CHECK(it != in_progress_requests_.end());
  for (auto& callback : in_progress_request->callbacks) {
    std::move(callback).Run(result);
  }
  in_progress_requests_.erase(it);
}

void WolvicPermissionManager::RequestContentPermissions(
    JNIEnv* env,
    InProgressRequest* in_progress_request,
    const content::PermissionRequestDescription& request_description) {
  auto url_java_string = base::android::ScopedJavaGlobalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(
          env, request_description.requesting_origin.spec()));
  auto permissions =
      ToJavaWolvicPermissionTypes(request_description.permissions);
  auto permissions_java_array = base::android::ScopedJavaGlobalRef<jintArray>(
      base::android::ToJavaIntArray(
          env, std::span(permissions.begin(), permissions.end())));
  auto android_permissions =
      ToAndroidPermissionTypes(request_description.permissions);
  auto java_android_permissions =
      base::android::ScopedJavaGlobalRef<jobjectArray>(
          base::android::ToJavaArrayOfStrings(
              env, std::span(android_permissions.begin(),
                             android_permissions.end())));

  Java_PermissionManagerBridge_onPermissionRequest(
      env, permissions_java_array, java_android_permissions, url_java_string,
      browser_context_->IsOffTheRecord(),
      reinterpret_cast<jlong>(in_progress_request));
}

InProgressRequest* WolvicPermissionManager::FindInProgressRequest(
    const content::PermissionRequestDescription& description) {
  auto it = std::find_if(in_progress_requests_.begin(),
                         in_progress_requests_.end(), [&](const auto& request) {
                           return request->description.requesting_origin ==
                                      description.requesting_origin &&
                                  request->description.permissions ==
                                      description.permissions;
                         });
  return it == in_progress_requests_.end() ?  nullptr : it->get();
}

static void JNI_PermissionManagerBridge_OnPermissionResult(
    JNIEnv* env,
    jboolean is_off_the_record,
    jlong in_progress_request_ptr,
    const base::android::JavaParamRef<jintArray>& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* in_progress_request =
      reinterpret_cast<InProgressRequest*>(in_progress_request_ptr);

  std::vector<int> java_permission_results;
  base::android::JavaIntArrayToIntVector(env, results,
                                         &java_permission_results);

  auto* permission_manager = is_off_the_record
                                 ? GetOffTheRecordPermissionManager()
                                 : GetPermissionManager();
  permission_manager->OnPermissionResult(
      in_progress_request,
      FromJavaWolvicPermissionStatuses(java_permission_results));
}

}  // namespace wolvic
