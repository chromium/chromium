// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/resource_mapper.h"

#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/resources/android/theme_resources.h"
#include "weblayer/browser/java/jni/ResourceMapper_jni.h"

namespace weblayer {
namespace {
using ResourceMap = std::map<int, int>;

ResourceMap* GetIdMap() {
  static base::NoDestructor<ResourceMap> id_map;
  return id_map.get();
}

// Create the mapping. IDs start at 0 to correspond to the array that gets
// built in the corresponding ResourceID Java class.
void ConstructMap() {
  DCHECK(GetIdMap()->empty());
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jintArray> java_id_array =
      Java_ResourceMapper_getResourceIdList(env);
  std::vector<int> resource_id_list;
  base::android::JavaIntArrayToIntVector(env, java_id_array, &resource_id_list);
  size_t next_id = 0;

#define LINK_RESOURCE_ID(c_id, java_id) \
  (*GetIdMap())[c_id] = resource_id_list[next_id++];
#define DECLARE_RESOURCE_ID(c_id, java_id) \
  (*GetIdMap())[c_id] = resource_id_list[next_id++];
#include "components/resources/android/blocked_content_resource_id.h"
#include "components/resources/android/page_info_resource_id.h"
#include "components/resources/android/permissions_resource_id.h"
#include "components/resources/android/sms_resource_id.h"
#include "components/resources/android/webxr_resource_id.h"
#undef LINK_RESOURCE_ID
#undef DECLARE_RESOURCE_ID
  // Make sure ID list sizes match up.
  DCHECK_EQ(next_id, resource_id_list.size());
}

}  // namespace

int MapToJavaDrawableId(int resource_id) {
  if (GetIdMap()->empty()) {
    ConstructMap();
  }

  ResourceMap::iterator iterator = GetIdMap()->find(resource_id);
  if (iterator != GetIdMap()->end()) {
    return iterator->second;
  }

  // The resource couldn't be found.
  // If you've landed here, please ensure that the header that declares the
  // mapping of your native resource to Java resource is listed both in
  // ResourceId.tempalate and in this file, next to other *resource_id.h
  // headers.
  NOTREACHED();
  return 0;
}

}  // namespace weblayer
