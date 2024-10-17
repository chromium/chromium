// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_permission_manager.h"

#include <jni.h>
#include <algorithm>
#include <memory>
#include <span>
#include <string>

#include "absl/types/optional.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "wolvic/jni_headers/PermissionManagerBridge_jni.h"

namespace wolvic {
namespace {

constexpr char kMediaSourceClass[] = "org/chromium/wolvic/PermissionManagerBridge$MediaSource";

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

// Has to be kept in sync with PermissionManagerBridge.java
enum class MediaSourceType {
  kCamera = 0,
  kScreen = 1,
  kMicrophone = 2,
  kAudiocapture = 3,
  kOther = 4,
};

// Has to be kept in sync with PermissionManagerBridge.java
enum class MediaType {
  kVideo = 0,
  kAudio = 1,
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

blink::PermissionType MediaStreamTypeToContentPermission(
    blink::mojom::MediaStreamType type) {
  using namespace blink::mojom;

  switch (type) {
    case MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return blink::PermissionType::AUDIO_CAPTURE;
    case MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return blink::PermissionType::VIDEO_CAPTURE;
    case MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case MediaStreamType::DISPLAY_AUDIO_CAPTURE:
    case MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
    case MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return blink::PermissionType::DISPLAY_CAPTURE;
    default:
      NOTREACHED_NORETURN();
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

MediaSourceType ToWolvicMediaSourceType(blink::mojom::MediaStreamType type) {
  using namespace blink::mojom;

  switch (type) {
    case MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return MediaSourceType::kMicrophone;
    case MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return MediaSourceType::kCamera;
    case MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return MediaSourceType::kAudiocapture;
    case MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
    case MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return MediaSourceType::kScreen;
    default:
      return MediaSourceType::kOther;
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaMediaSource(
    JNIEnv* env,
    const std::string& id,
    const std::string& name,
    MediaSourceType source,
    MediaType type) {
  jclass cls = env->FindClass(kMediaSourceClass);
  jmethodID constructor = env->GetMethodID(
      cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;II)V");
  jobject media_source = env->NewObject(
      cls, constructor, base::android::ConvertUTF8ToJavaString(env, id).obj(),
      base::android::ConvertUTF8ToJavaString(env, name).obj(),
      static_cast<jint>(source), static_cast<jint>(type));
  return base::android::ScopedJavaLocalRef<jobject>(env, media_source);
}

void ToJavaMediaSources(
    JNIEnv* env,
    const content::MediaStreamRequest& request,
    base::android::ScopedJavaLocalRef<jobjectArray>* video,
    base::android::ScopedJavaLocalRef<jobjectArray>* audio) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> video_sources;
  for (const auto& device_id : request.requested_video_device_ids) {
    video_sources.push_back(CreateJavaMediaSource(
        env, device_id, "", ToWolvicMediaSourceType(request.video_type),
        MediaType::kVideo));
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> audio_sources;
  for (const auto& device_id : request.requested_audio_device_ids) {
    audio_sources.push_back(CreateJavaMediaSource(
        env, device_id, "", ToWolvicMediaSourceType(request.audio_type),
        MediaType::kAudio));
  }

  auto media_source_class = base::android::GetClass(env, kMediaSourceClass);
  *video = base::android::ToTypedJavaArrayOfObjects(env, video_sources,
                                                    media_source_class.obj());
  *audio = base::android::ToTypedJavaArrayOfObjects(env, audio_sources,
                                                    media_source_class.obj());
}

// this method is copied from
// android_webview/browser/permission/media_access_permission_request.cc
// Return the device specified by |device_id| if exists, otherwise the first
// available device is returned.
const blink::MediaStreamDevice* GetDeviceByIdOrFirstAvailable(
    const blink::MediaStreamDevices& devices,
    const std::string& device_id) {
  if (devices.empty())
    return nullptr;

  if (!device_id.empty()) {
    for (const auto& device : devices) {
      if (device.id == device_id)
        return &device;
    }
  }

  return &devices[0];
}

WolvicPermissionManager* g_instance = nullptr;
WolvicPermissionManager* g_off_the_record_instance = nullptr;

std::vector<content::PermissionStatus> CombineStatuses(
    const std::vector<content::PermissionStatus>& content_statuses,
    const std::vector<content::PermissionStatus>& android_statuses) {
  DCHECK(content_statuses.size() == android_statuses.size());
  std::vector<content::PermissionStatus> result(content_statuses.size());
  for (size_t i = 0; i < content_statuses.size(); ++i) {
    if (content_statuses[i] == content::PermissionStatus::GRANTED &&
        android_statuses[i] == content::PermissionStatus::GRANTED) {
      result[i] = content::PermissionStatus::GRANTED;
    } else {
      result[i] = content::PermissionStatus::DENIED;
    }
  }
  return result;
}

}  // namespace

InProgressRequest::InProgressRequest(
    const content::PermissionRequestDescription& description,
    absl::optional<PermissionRequestCallback> callback,
    absl::optional<content::MediaStreamRequest> media_request,
    absl::optional<content::MediaResponseCallback> media_callback)
    : description(description),
      media_request(media_request),
      media_callback(std::move(media_callback)) {
  if (callback)
    callbacks.emplace_back(std::move(callback.value()));
}

InProgressRequest::~InProgressRequest() = default;

WolvicPermissionManager::WolvicPermissionManager(
    bool off_the_record)
    : off_the_record_(off_the_record) {}

WolvicPermissionManager::~WolvicPermissionManager() = default;

WolvicPermissionManager* WolvicPermissionManager::GetInstance(
    bool off_the_record) {
  if (off_the_record) {
      if (!g_off_the_record_instance)
	g_off_the_record_instance = new WolvicPermissionManager(true);
      return g_off_the_record_instance;
  }
  if (!g_instance)
    g_instance = new WolvicPermissionManager(false);
  return g_instance;
}

void WolvicPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    PermissionRequestCallback callback) {
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

  RequestContentPermissions(env, in_progress_requests_.back().get());
}

void WolvicPermissionManager::ResetPermission(blink::PermissionType permission,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin) {}

void WolvicPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    PermissionRequestCallback callback) {
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
WolvicPermissionManager::SubscribeToPermissionStatusChange(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void WolvicPermissionManager::UnsubscribeFromPermissionStatusChange(
    SubscriptionId subscription_id) {}

void WolvicPermissionManager::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  std::vector<blink::PermissionType> permissions;
  if (request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    permissions.push_back(
        MediaStreamTypeToContentPermission(request.audio_type));
  }
  if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    permissions.push_back(
        MediaStreamTypeToContentPermission(request.video_type));
  }

  content::PermissionRequestDescription description(
      permissions, /*user_gesture=*/false, request.security_origin);
  JNIEnv* env = base::android::AttachCurrentThread();
  in_progress_requests_.emplace_back(std::make_unique<InProgressRequest>(
      description, /*callback=*/absl::nullopt, request, std::move(callback)));
  RequestContentPermissions(env, in_progress_requests_.back().get());
}

bool WolvicPermissionManager::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  auto it = allowed_media_permissions_cache_.find(security_origin);
  if (it == allowed_media_permissions_cache_.end())
    return false;

  return it->second.find(type) != it->second.end();
}

void WolvicPermissionManager::OnContentPermissionResult(
    JNIEnv* env,
    InProgressRequest* in_progress_request,
    const std::vector<content::PermissionStatus>& result) {
  in_progress_request->content_results = result;
  RequestAndroidPermissions(env, in_progress_request);
}

void WolvicPermissionManager::OnAndroidPermissionResult(
    InProgressRequest* in_progress_request,
    const std::vector<content::PermissionStatus>& result) {
  in_progress_request->android_results = result;

  // If this was part of media request, proceed with requesting media permissions.
  if (in_progress_request->media_callback) {
    OnMediaContentPermissionResult(
        in_progress_request,
        CombineStatuses(in_progress_request->content_results.value(),
                        in_progress_request->android_results.value()));
    return;
  }

  // Otherwise, complete the request.
  CompleteRequest(in_progress_request);
}

void WolvicPermissionManager::OnMediaContentPermissionResult(
    InProgressRequest* in_progress_request,
    const std::vector<content::PermissionStatus>& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> video_sources;
  base::android::ScopedJavaLocalRef<jobjectArray> audio_sources;
  ToJavaMediaSources(env, in_progress_request->media_request.value(),
                     &video_sources, &audio_sources);
  auto url = base::android::ConvertUTF8ToJavaString(
      env, in_progress_request->media_request.value().security_origin.spec());
  Java_PermissionManagerBridge_onMediaPermissionRequest(
      env, video_sources, audio_sources, url,
      off_the_record_,
      reinterpret_cast<jlong>(in_progress_request));
}

void WolvicPermissionManager::OnMediaPermissionResult(
    InProgressRequest* in_progress_request,
    bool granted,
    const absl::optional<std::string>& video_id,
    const absl::optional<std::string>& audio_id) {
  blink::mojom::StreamDevicesSet stream_devices_set;
  if (!granted) {
    std::move(in_progress_request->media_callback.value()).Run(
        stream_devices_set,
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  stream_devices_set.stream_devices.emplace_back(blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices.back();

  if (video_id) {
    const blink::MediaStreamDevice* device = GetDeviceByIdOrFirstAvailable(
        content::MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices(),
        *video_id);
    if (device)
      devices.video_device = *device;
  }

  if (audio_id) {
    const blink::MediaStreamDevice* device = GetDeviceByIdOrFirstAvailable(
        content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices(),
        *audio_id);
    if (device)
      devices.audio_device = *device;
  }

  auto result = blink::mojom::MediaStreamRequestResult::NO_HARDWARE;

  if (devices.video_device.has_value()) {
    result = blink::mojom::MediaStreamRequestResult::OK;
    allowed_media_permissions_cache_[in_progress_request->media_request
                                         ->security_origin]
        .insert(in_progress_request->media_request->video_type);
  }

  if (devices.audio_device.has_value()) {
    result = blink::mojom::MediaStreamRequestResult::OK;
    allowed_media_permissions_cache_[in_progress_request->media_request
                                         ->security_origin]
        .insert(in_progress_request->media_request->audio_type);
  }

  if (result == blink::mojom::MediaStreamRequestResult::NO_HARDWARE)
    stream_devices_set.stream_devices.clear();

  std::move(in_progress_request->media_callback.value())
      .Run(stream_devices_set, result,
           std::unique_ptr<content::MediaStreamUI>());
  CompleteRequest(in_progress_request);
}

void WolvicPermissionManager::CompleteRequest(
    InProgressRequest* in_progress_request) {
  auto it = std::find_if(in_progress_requests_.begin(),
                         in_progress_requests_.end(), [&](const auto& request) {
                           return request.get() == in_progress_request;
                         });
  CHECK(it != in_progress_requests_.end());
  CHECK(in_progress_request->content_results &&
        in_progress_request->android_results);

  auto result = CombineStatuses(in_progress_request->content_results.value(),
                                in_progress_request->android_results.value());
  for (auto& callback : in_progress_request->callbacks) {
    std::move(callback).Run(result);
  }
  in_progress_requests_.erase(it);
}

void WolvicPermissionManager::RequestContentPermissions(
    JNIEnv* env,
    InProgressRequest* in_progress_request) {
  auto url_java_string = base::android::ScopedJavaGlobalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(
          env, in_progress_request->description.requesting_origin.spec()));
  auto permissions =
      ToJavaWolvicPermissionTypes(in_progress_request->description.permissions);
  auto permissions_java_array = base::android::ScopedJavaGlobalRef<jintArray>(
      base::android::ToJavaIntArray(
          env, std::span(permissions.begin(), permissions.end())));
  Java_PermissionManagerBridge_onContentPermissionRequest(
      env, permissions_java_array, url_java_string,
      off_the_record_,
      reinterpret_cast<jlong>(in_progress_request));
}

void WolvicPermissionManager::RequestAndroidPermissions(
    JNIEnv* env,
    InProgressRequest* in_progress_request) {
  auto android_permissions =
      ToAndroidPermissionTypes(in_progress_request->description.permissions);
  auto java_android_permissions =
      base::android::ScopedJavaGlobalRef<jobjectArray>(
          base::android::ToJavaArrayOfStrings(
              env, std::span(android_permissions.begin(),
                             android_permissions.end())));

  Java_PermissionManagerBridge_onAndroidPermissionRequest(
      env, java_android_permissions, off_the_record_,
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
  return it == in_progress_requests_.end() ? nullptr : it->get();
}

static void JNI_PermissionManagerBridge_OnContentPermissionResult(
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

  auto* permission_manager =
      WolvicPermissionManager::GetInstance(is_off_the_record);
  permission_manager->OnContentPermissionResult(
      env, in_progress_request,
      FromJavaWolvicPermissionStatuses(java_permission_results));
}

static void JNI_PermissionManagerBridge_OnAndroidPermissionResult(
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

  auto* permission_manager =
      WolvicPermissionManager::GetInstance(is_off_the_record);
  permission_manager->OnAndroidPermissionResult(
      in_progress_request,
      FromJavaWolvicPermissionStatuses(java_permission_results));
}

static void JNI_PermissionManagerBridge_OnMediaPermissionResult(
    JNIEnv* env,
    jboolean is_off_the_record,
    jlong in_progress_request_ptr,
    jboolean granted,
    const base::android::JavaParamRef<jstring>& java_video_id,
    const base::android::JavaParamRef<jstring>& java_audio_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* in_progress_request =
      reinterpret_cast<InProgressRequest*>(in_progress_request_ptr);
  auto* permission_manager =
      WolvicPermissionManager::GetInstance(is_off_the_record);
  absl::optional<std::string> video_id;
  absl::optional<std::string> audio_id;
  if (java_video_id)
    video_id = base::android::ConvertJavaStringToUTF8(java_video_id);
  if (java_audio_id)
    audio_id = base::android::ConvertJavaStringToUTF8(java_audio_id);

  permission_manager->OnMediaPermissionResult(
      in_progress_request, granted, video_id, audio_id);
}

}  // namespace wolvic
