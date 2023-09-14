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

import com.google.android.odml.image.annotation.KeepForSdk;

/** Manages internal image data storage. The interface is package-private. */
@KeepForSdk
interface ImageContainer {
  /** Returns the properties of the contained image. */
  @KeepForSdk
  ImageProperties getImageProperties();

  /** Close the image container and releases the image resource inside. */
  @KeepForSdk
  void close();
}
