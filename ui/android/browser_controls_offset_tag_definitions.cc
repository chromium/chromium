// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/browser_controls_offset_tag_definitions.h"

#include "base/android/scoped_java_ref.h"
#include "cc/input/android/offset_tag_android.h"
#include "ui/android/browser_controls_offset_tag_constraints.h"
#include "ui/android/browser_controls_offset_tag_definitions.h"
#include "ui/android/ui_android_jni_headers/BrowserControlsOffsetTagConstraints_jni.h"
#include "ui/android/ui_android_jni_headers/BrowserControlsOffsetTagDefinitions_jni.h"
#include "ui/android/ui_android_jni_headers/OffsetTagConstraints_jni.h"

namespace {

viz::OffsetTagConstraints FromJavaOffsetTagConstraints(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& joffset_tag_constraints) {
  if (!joffset_tag_constraints) {
    return viz::OffsetTagConstraints();
  }
  return viz::OffsetTagConstraints(
      Java_OffsetTagConstraints_minX(env, joffset_tag_constraints),
      Java_OffsetTagConstraints_maxX(env, joffset_tag_constraints),
      Java_OffsetTagConstraints_minY(env, joffset_tag_constraints),
      Java_OffsetTagConstraints_maxY(env, joffset_tag_constraints));
}

ui::BrowserControlsOffsetTagConstraints
FromJavaBrowserControlsOffsetTagConstraints(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        jbrowser_controls_offset_tag_constraints) {
  ui::BrowserControlsOffsetTagConstraints offset_tag_constraints;
  if (!jbrowser_controls_offset_tag_constraints) {
    return offset_tag_constraints;
  }

  const base::android::JavaRef<jobject>& jtop_controls_constraints =
      Java_BrowserControlsOffsetTagConstraints_getTopControlsConstraints(
          env, jbrowser_controls_offset_tag_constraints);
  viz::OffsetTagConstraints top_controls_constraints =
      FromJavaOffsetTagConstraints(env, jtop_controls_constraints);
  offset_tag_constraints.top_controls_constraints = top_controls_constraints;

  const base::android::JavaRef<jobject>& jcontent_constraints =
      Java_BrowserControlsOffsetTagConstraints_getContentConstraints(
          env, jbrowser_controls_offset_tag_constraints);
  viz::OffsetTagConstraints content_constraints =
      FromJavaOffsetTagConstraints(env, jcontent_constraints);
  offset_tag_constraints.content_constraints = content_constraints;

  const base::android::JavaRef<jobject>& jbottom_controls_constraints =
      Java_BrowserControlsOffsetTagConstraints_getBottomControlsConstraints(
          env, jbrowser_controls_offset_tag_constraints);
  viz::OffsetTagConstraints bottom_controls_constraints =
      FromJavaOffsetTagConstraints(env, jbottom_controls_constraints);
  offset_tag_constraints.bottom_controls_constraints =
      bottom_controls_constraints;

  return offset_tag_constraints;
}

}  // namespace

namespace ui {

BrowserControlsOffsetTagDefinitions FromJavaBrowserControlsOffsetTagDefinitions(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        jbrowser_controls_offset_tag_definitions) {
  BrowserControlsOffsetTagDefinitions offset_tag_definitions;
  if (!jbrowser_controls_offset_tag_definitions) {
    return offset_tag_definitions;
  }

  const base::android::JavaRef<jobject>& jtags =
      Java_BrowserControlsOffsetTagDefinitions_getTags(
          env, jbrowser_controls_offset_tag_definitions);
  cc::BrowserControlsOffsetTags tags =
      cc::android::FromJavaBrowserControlsOffsetTags(env, jtags);
  offset_tag_definitions.tags = tags;

  const base::android::JavaRef<jobject>& jconstraints =
      Java_BrowserControlsOffsetTagDefinitions_getConstraints(
          env, jbrowser_controls_offset_tag_definitions);
  BrowserControlsOffsetTagConstraints constraints =
      FromJavaBrowserControlsOffsetTagConstraints(env, jconstraints);
  offset_tag_definitions.constraints = constraints;

  return offset_tag_definitions;
}

}  // namespace ui
