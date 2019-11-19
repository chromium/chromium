// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
#define UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/trace_event/memory_dump_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class UIResourceManager;
}  // namespace cc

namespace ui {

class UI_ANDROID_EXPORT ResourceManagerImpl
    : public ResourceManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  static ResourceManagerImpl* FromJavaObject(
      const base::android::JavaRef<jobject>& jobj);

  explicit ResourceManagerImpl(gfx::NativeWindow native_window);
  ~ResourceManagerImpl() override;

  void Init(cc::UIResourceManager* ui_resource_manager);

  // ResourceManager implementation.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  Resource* GetResource(AndroidResourceType res_type, int res_id) override;
  Resource* GetStaticResourceWithTint(
      int res_id, SkColor tint_color) override;
  void RemoveUnusedTints(const std::unordered_set<int>& used_tints) override;
  void PreloadResource(AndroidResourceType res_type, int res_id) override;

  // Called from Java
  // ----------------------------------------------------------
  void OnResourceReady(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj,
                       jint res_type,
                       jint res_id,
                       const base::android::JavaRef<jobject>& bitmap,
                       jint width,
                       jint height,
                       jlong native_resource);
  void RemoveResource(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      jint res_type,
      jint res_id);
  void ClearTintedResourceCache(JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class TestResourceManagerImpl;

  // Start loading the resource. virtual for testing.
  virtual void PreloadResourceFromJava(AndroidResourceType res_type,
                                       int res_id);
  virtual void RequestResourceFromJava(AndroidResourceType res_type,
                                       int res_id);

  using ResourceMap = std::unordered_map<int, std::unique_ptr<Resource>>;
  using TintedResourceMap =
      std::unordered_map<SkColor, std::unique_ptr<ResourceMap>>;

  cc::UIResourceManager* ui_resource_manager_;
  ResourceMap resources_[ANDROID_RESOURCE_TYPE_COUNT];
  TintedResourceMap tinted_resources_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerImpl);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
