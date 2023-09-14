/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.processor;

import androidx.annotation.Nullable;
import com.google.auto.value.AutoValue;
import java.io.File;

/** Options to configure Searcher API. */
@AutoValue
public abstract class SearcherOptions {
  private static final boolean DEFAULT_L2_NORMALIZE = false;
  private static final boolean DEFAULT_QUANTIZE = false;
  private static final int DEFAULT_MAX_RESULTS = 5;

  public abstract boolean getL2Normalize();

  public abstract boolean getQuantize();

  @Nullable
  public abstract File getIndexFile();

  public abstract int getMaxResults();

  public static Builder builder() {
    return new AutoValue_SearcherOptions.Builder()
        .setL2Normalize(DEFAULT_L2_NORMALIZE)
        .setQuantize(DEFAULT_QUANTIZE)
        .setIndexFile(null)
        .setMaxResults(DEFAULT_MAX_RESULTS);
  }

  /** Builder for {@link SearcherOptions}. */
  @AutoValue.Builder
  public abstract static class Builder {
    /**
     * Sets whether to normalize the embedding feature vector with L2 norm. Defaults to false.
     *
     * <p>Use this option only if the model does not already contain a native L2_NORMALIZATION
     * TFLite Op. In most cases, this is already the case and L2 norm is thus achieved through
     * TFLite inference.
     */
    public abstract Builder setL2Normalize(boolean l2Normalize);

    /**
     * Sets whether the embedding should be quantized to bytes via scalar quantization. Defaults to
     * false.
     *
     * <p>Embeddings are implicitly assumed to be unit-norm and therefore any dimension is
     * guaranteed to have a value in {@code [-1.0, 1.0]}. Use the l2_normalize option if this is not
     * the case.
     */
    public abstract Builder setQuantize(boolean quantize);

    /**
     * Sets the index file to search into.
     *
     * <p>Required if the model does not come with an index file inside. Otherwise, it can be ignore
     * by setting to {@code null}.
     */
    public abstract Builder setIndexFile(@Nullable File indexFile);

    /** Sets the maximum number of nearest neighbor results to return. Defaults to {@code 5} */
    public abstract Builder setMaxResults(int maxResults);

    public abstract SearcherOptions build();
  }
}
