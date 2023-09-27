/* Copyright 2021 Google LLC. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

package com.google.android.odml.image;

import android.graphics.Bitmap;
import com.google.android.odml.image.MlImage.ImageFormat;

class BitmapImageContainer implements ImageContainer {

  private final Bitmap bitmap;
  private final ImageProperties properties;

  // incompatible argument for parameter config of convertFormatCode.
  @SuppressWarnings("nullness:argument.type.incompatible")
  public BitmapImageContainer(Bitmap bitmap) {
    this.bitmap = bitmap;
    this.properties = ImageProperties.builder()
        .setImageFormat(convertFormatCode(bitmap.getConfig()))
        .setStorageType(MlImage.STORAGE_TYPE_BITMAP)
        .build();
  }

  public Bitmap getBitmap() {
    return bitmap;
  }

  @Override
  public ImageProperties getImageProperties() {
    return properties;
  }

  @Override
  public void close() {
    bitmap.recycle();
  }

  @ImageFormat
  static int convertFormatCode(Bitmap.Config config) {
    switch (config) {
      case ALPHA_8:
        return MlImage.IMAGE_FORMAT_ALPHA;
      case ARGB_8888:
        return MlImage.IMAGE_FORMAT_RGBA;
      default:
        return MlImage.IMAGE_FORMAT_UNKNOWN;
    }
  }
}
