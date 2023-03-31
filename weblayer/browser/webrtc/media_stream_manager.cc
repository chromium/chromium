// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webrtc/media_stream_manager.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "weblayer/browser/java/jni/MediaStreamManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

namespace {

constexpr int kWebContentsUserDataKey = 0;

struct UserData : public base::SupportsUserData::Data {
  raw_ptr<MediaStreamManager, DanglingUntriaged> manager = nullptr;
};

}  // namespace

// A class that tracks the lifecycle of a single active media stream. Ownership
// is passed off to MediaResponseCallback.
class MediaStreamManager::StreamUi : public content::MediaStreamUI {
 public:
  StreamUi(base::WeakPtr<MediaStreamManager> manager,
           const blink::mojom::StreamDevicesSet& stream_devices)
      : manager_(manager) {
    DCHECK(manager_);
    DCHECK_EQ(1u, stream_devices.stream_devices.size());
    streaming_audio_ =
        stream_devices.stream_devices[0]->audio_device.has_value();
    streaming_video_ =
        stream_devices.stream_devices[0]->video_device.has_value();
  }
  StreamUi(const StreamUi&) = delete;
  StreamUi& operator=(const StreamUi&) = delete;

  ~StreamUi() override {
    if (manager_)
      manager_->UnregisterStream(this);
  }

  // content::MediaStreamUi:
  gfx::NativeViewId OnStarted(
      base::RepeatingClosure stop,
      SourceCallback source,
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_capture_ids,
      StateChangeCallback state_change) override {
    stop_ = std::move(stop);
    if (manager_)
      manager_->RegisterStream(this);
    return 0;
  }
  void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const content::DesktopMediaID& old_media_id,
      const content::DesktopMediaID& new_media_id) override {}
  void OnDeviceStopped(const std::string& label,
                       const content::DesktopMediaID& media_id) override {}

  bool streaming_audio() const { return streaming_audio_; }

  bool streaming_video() const { return streaming_video_; }

  void Stop() {
    // The `stop_` callback does async processing. This means Stop() may be
    // called multiple times.
    if (stop_)
      std::move(stop_).Run();
  }

 private:
  base::WeakPtr<MediaStreamManager> manager_;
  bool streaming_audio_ = false;
  bool streaming_video_ = false;
  base::OnceClosure stop_;
};

MediaStreamManager::MediaStreamManager(
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jobject>& j_web_contents)
    : j_object_(j_object) {
  auto user_data = std::make_unique<UserData>();
  user_data->manager = this;
  content::WebContents::FromJavaWebContents(j_web_contents)
      ->SetUserData(&kWebContentsUserDataKey, std::move(user_data));
}

MediaStreamManager::~MediaStreamManager() = default;

// static
MediaStreamManager* MediaStreamManager::FromWebContents(
    content::WebContents* contents) {
  UserData* user_data = reinterpret_cast<UserData*>(
      contents->GetUserData(&kWebContentsUserDataKey));
  DCHECK(user_data);
  return user_data->manager;
}

void MediaStreamManager::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  webrtc::MediaStreamDevicesController::RequestPermissions(
      request, nullptr,
      base::BindOnce(&MediaStreamManager::OnMediaAccessPermissionResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaStreamManager::OnClientReadyToStream(JNIEnv* env,
                                               int request_id,
                                               bool allowed) {
  auto request = requests_pending_client_approval_.find(request_id);
  CHECK(request != requests_pending_client_approval_.end());
  if (allowed) {
    std::move(request->second.callback)
        .Run(*request->second.stream_devices_set_, request->second.result,
             std::make_unique<StreamUi>(weak_factory_.GetWeakPtr(),
                                        *request->second.stream_devices_set_));
  } else {
    std::move(request->second.callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::NO_HARDWARE, {});
  }
  requests_pending_client_approval_.erase(request);
}

void MediaStreamManager::StopStreaming(JNIEnv* env) {
  std::set<StreamUi*> active_streams = active_streams_;
  for (auto* stream : active_streams)
    stream->Stop();
}

void MediaStreamManager::OnMediaAccessPermissionResult(
    content::MediaResponseCallback callback,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    bool blocked_by_permissions_policy,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  // TODO(crbug.com/1300883): Generalize to multiple streams.
  DCHECK((result != blink::mojom::MediaStreamRequestResult::OK &&
          stream_devices_set.stream_devices.empty()) ||
         (result == blink::mojom::MediaStreamRequestResult::OK &&
          stream_devices_set.stream_devices.size() == 1u));
  if (result != blink::mojom::MediaStreamRequestResult::OK) {
    std::move(callback).Run(stream_devices_set, result, {});
    return;
  }

  int request_id = next_request_id_++;
  requests_pending_client_approval_[request_id] = RequestPendingClientApproval(
      std::move(callback), stream_devices_set, result);
  Java_MediaStreamManager_prepareToStream(
      base::android::AttachCurrentThread(), j_object_,
      stream_devices_set.stream_devices[0]->audio_device.has_value(),
      stream_devices_set.stream_devices[0]->video_device.has_value(),
      request_id);
}

void MediaStreamManager::RegisterStream(StreamUi* stream) {
  active_streams_.insert(stream);
  Update();
}

void MediaStreamManager::UnregisterStream(StreamUi* stream) {
  active_streams_.erase(stream);
  Update();
}

void MediaStreamManager::Update() {
  bool audio = false;
  bool video = false;
  for (const auto* stream : active_streams_) {
    audio = audio || stream->streaming_audio();
    video = video || stream->streaming_video();
  }

  Java_MediaStreamManager_update(base::android::AttachCurrentThread(),
                                 j_object_, audio, video);
}

static jlong JNI_MediaStreamManager_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jobject>& j_web_contents) {
  return reinterpret_cast<intptr_t>(
      new MediaStreamManager(j_object, j_web_contents));
}

static void JNI_MediaStreamManager_Destroy(JNIEnv* env, jlong native_manager) {
  delete reinterpret_cast<MediaStreamManager*>(native_manager);
}

MediaStreamManager::RequestPendingClientApproval::
    RequestPendingClientApproval() = default;

MediaStreamManager::RequestPendingClientApproval::RequestPendingClientApproval(
    content::MediaResponseCallback callback,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result)
    : callback(std::move(callback)),
      stream_devices_set_(stream_devices_set.Clone()),
      result(result) {}

MediaStreamManager::RequestPendingClientApproval::
    ~RequestPendingClientApproval() = default;

MediaStreamManager::RequestPendingClientApproval&
MediaStreamManager::RequestPendingClientApproval::operator=(
    RequestPendingClientApproval&& other) = default;

}  // namespace weblayer
