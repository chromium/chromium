// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager_impl.h"

#include <inttypes.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/ui_android_jni_headers/ResourceManager_jni.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

using base::android::JavaArrayOfIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

base::trace_event::MemoryAllocatorDump* CreateMemoryDump(
    const std::string& name,
    size_t memory_usage,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage);

  static const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name)
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  return dump;
}

}  // namespace

namespace ui {

// static
ResourceManagerImpl* ResourceManagerImpl::FromJavaObject(
    const JavaRef<jobject>& jobj) {
  return reinterpret_cast<ResourceManagerImpl*>(
      Java_ResourceManager_getNativePtr(base::android::AttachCurrentThread(),
                                        jobj));
}

ResourceManagerImpl::ResourceManagerImpl(gfx::NativeWindow native_window)
    : ui_resource_manager_(nullptr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ResourceManager_create(env, native_window->GetJavaObject(),
                                       reinterpret_cast<intptr_t>(this))
               .obj());
  DCHECK(!java_obj_.is_null());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "android::ResourceManagerImpl",
      base::ThreadTaskRunnerHandle::Get());
}

ResourceManagerImpl::~ResourceManagerImpl() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  Java_ResourceManager_destroy(base::android::AttachCurrentThread(), java_obj_);
}

void ResourceManagerImpl::Init(cc::UIResourceManager* ui_resource_manager) {
  DCHECK(!ui_resource_manager_);
  DCHECK(ui_resource_manager);
  ui_resource_manager_ = ui_resource_manager;
}

base::android::ScopedJavaLocalRef<jobject>
ResourceManagerImpl::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

Resource* ResourceManagerImpl::GetResource(AndroidResourceType res_type,
                                           int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  std::unordered_map<int, std::unique_ptr<Resource>>::iterator item =
      resources_[res_type].find(res_id);

  if (item == resources_[res_type].end() ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP) {
    RequestResourceFromJava(res_type, res_id);

    // Check if the resource has been added (some dynamic may not have been).
    item = resources_[res_type].find(res_id);
    if (item == resources_[res_type].end())
      return nullptr;
  }

  return item->second.get();
}

void ResourceManagerImpl::RemoveUnusedTints(
    const std::unordered_set<int>& used_tints) {
  // Iterate over the currently cached tints and remove ones that were not
  // used as defined in |used_tints|.
  for (auto it = tinted_resources_.cbegin(); it != tinted_resources_.cend();) {
    if (used_tints.find(it->first) == used_tints.end()) {
        it = tinted_resources_.erase(it);
    } else {
        ++it;
    }
  }
}

Resource* ResourceManagerImpl::GetStaticResourceWithTint(int res_id,
                                                         SkColor tint_color) {
  if (tinted_resources_.find(tint_color) == tinted_resources_.end()) {
    tinted_resources_[tint_color] = std::make_unique<ResourceMap>();
  }
  ResourceMap* resource_map = tinted_resources_[tint_color].get();

  // If the resource is already cached, use it.
  std::unordered_map<int, std::unique_ptr<Resource>>::iterator item =
      resource_map->find(res_id);
  if (item != resource_map->end())
    return item->second.get();

  Resource* base_image = GetResource(ANDROID_RESOURCE_TYPE_STATIC, res_id);
  DCHECK(base_image);

  std::unique_ptr<Resource> tinted_resource = base_image->CreateForCopy();

  TRACE_EVENT0("browser", "ResourceManagerImpl::GetStaticResourceWithTint");
  SkBitmap tinted_bitmap;
  tinted_bitmap.allocPixels(SkImageInfo::MakeN32Premul(
      base_image->size().width(), base_image->size().height()));

  SkCanvas canvas(tinted_bitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  // Build a color filter to use on the base resource. This filter multiplies
  // the RGB components by the components of the new color but retains the
  // alpha of the original image.
  float color_matrix[20] = {0, 0, 0, 0, SkColorGetR(tint_color) * (1.0f / 255),
                            0, 0, 0, 0, SkColorGetG(tint_color) * (1.0f / 255),
                            0, 0, 0, 0, SkColorGetB(tint_color) * (1.0f / 255),
                            0, 0, 0, 1, 0};
  SkPaint color_filter;
  color_filter.setColorFilter(SkColorFilters::Matrix(color_matrix));

  // Draw the resource and make it immutable.
  base_image->ui_resource()
      ->GetBitmap(base_image->ui_resource()->id(), false)
      .DrawToCanvas(&canvas, &color_filter);
  tinted_bitmap.setImmutable();

  // Create a UI resource from the new bitmap.
  tinted_resource->SetUIResource(
      cc::ScopedUIResource::Create(ui_resource_manager_,
                                   cc::UIResourceBitmap(tinted_bitmap)),
      base_image->size());

  (*resource_map)[res_id].swap(tinted_resource);

  return (*resource_map)[res_id].get();
}

void ResourceManagerImpl::ClearTintedResourceCache(JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  tinted_resources_.clear();
}

void ResourceManagerImpl::PreloadResource(AndroidResourceType res_type,
                                          int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  // Don't send out a query if the resource is already loaded.
  if (resources_[res_type].find(res_id) != resources_[res_type].end())
    return;

  PreloadResourceFromJava(res_type, res_id);
}

void ResourceManagerImpl::OnResourceReady(JNIEnv* env,
                                          const JavaRef<jobject>& jobj,
                                          jint res_type,
                                          jint res_id,
                                          const JavaRef<jobject>& bitmap,
                                          jint width,
                                          jint height,
                                          jlong native_resource) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);
  TRACE_EVENT2("ui", "ResourceManagerImpl::OnResourceReady",
               "resource_type", res_type,
               "resource_id", res_id);

  resources_[res_type][res_id] =
      base::WrapUnique(reinterpret_cast<Resource*>(native_resource));
  Resource* resource = resources_[res_type][res_id].get();

  gfx::JavaBitmap jbitmap(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(jbitmap);
  skbitmap.setImmutable();
  resource->SetUIResource(
      cc::ScopedUIResource::Create(ui_resource_manager_,
                                   cc::UIResourceBitmap(skbitmap)),
      gfx::Size(width, height));
}

void ResourceManagerImpl::RemoveResource(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    jint res_type,
    jint res_id) {
  resources_[res_type].erase(res_id);
}

bool ResourceManagerImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string prefix = base::StringPrintf("ui/resource_manager_0x%" PRIXPTR,
                                          reinterpret_cast<uintptr_t>(this));
  for (uint32_t type = static_cast<uint32_t>(ANDROID_RESOURCE_TYPE_FIRST);
       type <= static_cast<uint32_t>(ANDROID_RESOURCE_TYPE_LAST); ++type) {
    size_t usage = base::trace_event::EstimateMemoryUsage(resources_[type]);
    auto* dump = CreateMemoryDump(
        prefix + base::StringPrintf("/default_resource/0x%u",
                                    static_cast<uint32_t>(type)),
        usage, pmd);
    dump->AddScalar("resource_count", "objects", resources_[type].size());
  }

  size_t tinted_resource_usage =
      base::trace_event::EstimateMemoryUsage(tinted_resources_);
  CreateMemoryDump(prefix + "/tinted_resource", tinted_resource_usage, pmd);
  return true;
}

void ResourceManagerImpl::PreloadResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::PreloadResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_preloadResource(base::android::AttachCurrentThread(),
                                       java_obj_, res_type, res_id);
}

void ResourceManagerImpl::RequestResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::RequestResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_resourceRequested(base::android::AttachCurrentThread(),
                                         java_obj_, res_type, res_id);
}

}  // namespace ui
