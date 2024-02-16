// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.jni_zero.samples;

import android.app.Activity;
import android.os.Bundle;

public class SampleActivity extends Activity {
    static {
        System.loadLibrary("sample_lib");
    }

    public void onCreate(Bundle b) {
        super.onCreate(b);
        SampleForAnnotationProcessor.test();
    }
}
