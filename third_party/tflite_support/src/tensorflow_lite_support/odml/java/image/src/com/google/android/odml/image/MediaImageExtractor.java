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

import android.media.Image;
import android.os.Build.VERSION_CODES;
import androidx.annotation.RequiresApi;

/**
 * Utility for extracting {@link android.media.Image} from {@link MlImage}.
 *
 * <p>Currently it only supports {@link MlImage} with {@link MlImage#STORAGE_TYPE_MEDIA_IMAGE},
 * otherwise {@link IllegalArgumentException} will be thrown.
 */
@RequiresApi(VERSION_CODES.KITKAT)
public class MediaImageExtractor {

  private MediaImageExtractor() {}

  /**
   * Extracts a {@link android.media.Image} from an {@link MlImage}. Currently it only works for
   * {@link MlImage} that built from {@link MediaMlImageBuilder}.
   *
   * <p>Notice: Properties of the {@code image} like rotation will not take effects.
   *
   * @param image the image to extract {@link android.media.Image} from.
   * @return {@link android.media.Image} that stored in {@link MlImage}.
   * @throws IllegalArgumentException if the extraction failed.
   */
  public static Image extract(MlImage image) {
    ImageContainer container;
    if ((container = image.getContainer(MlImage.STORAGE_TYPE_MEDIA_IMAGE)) != null) {
      return ((MediaImageContainer) container).getImage();
    }
    throw new IllegalArgumentException(
        "Extract Media Image from an MlImage created by objects other than Media Image"
            + " is not supported");
  }
}
