/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.support.metadata;

import static com.google.common.truth.Truth.assertThat;

import java.util.regex.Pattern;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests of {@link MetadataParser}. */
@RunWith(JUnit4.class)
public final class MetadataParserTest {

  @Test
  public void version_wellFormedAsSemanticVersion() throws Exception {
    // Validates that the version is well-formed (x.y.z).
    String pattern = "[0-9]+\\.[0-9]+\\.[0-9]+";
    Pattern r = Pattern.compile(pattern);
    assertThat(MetadataParser.VERSION).matches(r);
  }
}
