# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# TODO(alcooper): This is a fork of src/sdk/proguard-rules.pro. Once b/264530605
# has been fixed/published roll to pick that up and switch to using that
# instead.
-keep class com.google.cardboard.sdk.DistortionRenderer { *; }
-keep class com.google.cardboard.sdk.HeadTracker { *; }
-keep class com.google.cardboard.sdk.Initialize { *; }
-keep class com.google.cardboard.sdk.LensDistortion { *; }
-keep class com.google.cardboard.sdk.QrCode { *; }
-keep class com.google.cardboard.sdk.qrcode.CardboardParamsUtils { *; }
-keep class com.google.cardboard.sdk.qrcode.CardboardParamsUtils.StorageSource { *; }
-keep class com.google.cardboard.sdk.qrcode.CardboardParamsUtils.UriToParamsStatus { *; }
-keep class com.google.cardboard.sdk.QrCodeCaptureActivity { *; }
-keep class com.google.cardboard.sdk.nativetypes.EyeTextureDescription { *; }
-keep class com.google.cardboard.sdk.nativetypes.EyeType { *; }
-keep class com.google.cardboard.sdk.nativetypes.Mesh { *; }
-keep class com.google.cardboard.sdk.nativetypes.UvPoint { *; }
-keep class com.google.cardboard.sdk.deviceparams.DeviceParamsUtils { *; }
-keep class com.google.cardboard.sdk.screenparams.ScreenParamsUtils { *; }
-keep class com.google.cardboard.sdk.screenparams.ScreenParamsUtils.ScreenPixelDensity { *; }
