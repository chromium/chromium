# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Keep classes, methods, and fields that are accessed with JNI.
-keep class com.google.cardboard.sdk.UsedByNative
-keepclasseswithmembers,includedescriptorclasses class ** {
  @com.google.cardboard.sdk.UsedByNative *;
}
# Keep proto methods that are directly queried over JNI since proguard cannot
# determine which methods those are.
-keep,includedescriptorclasses class com.google.cardboard.proto.CardboardDevice$DeviceParams$VerticalAlignmentType {
  ordinal();
}
-keep,includedescriptorclasses class com.google.cardboard.proto.CardboardDevice$DeviceParams {
  getScreenToLensDistance();
  getInterLensDistance();
  getTrayToLensDistance();
  getVerticalAlignment();
  getDistortionCoefficients(int);
  getDistortionCoefficientsCount();
  getLeftEyeFieldOfViewAngles(int);
}
# Prevent native methods which are used from being obfuscated.
-keepclasseswithmembernames,includedescriptorclasses,allowaccessmodification class com.google.cardboard.sdk.** {
  native <methods>;
}
