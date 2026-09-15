// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

@JniType("::my::cpp::Define")
public interface SampleTypeDefine extends JniTypeToken {
    @JniType("::my::cpp::Nested")
    public interface Nested extends JniTypeToken {}
}
