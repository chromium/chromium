// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import android.util.ArrayMap;

import java.util.Map;

/** Native helpers. */
public class JniUtil {
    @CalledByNative
    private static Object[] mapToArray(Map<Object, Object> map) {
        Object[] ret = new Object[map.size() * 2];
        int i = 0;
        for (var entry : map.entrySet()) {
            ret[i++] = entry.getKey();
            ret[i++] = entry.getValue();
        }
        return ret;
    }

    @CalledByNative
    private static Map<Object, Object> arrayToMap(Object[] array) {
        int len = array.length;
        Map<Object, Object> ret = new ArrayMap(len / 2);
        for (int i = 0; i < len; i += 2) {
            ret.put(array[i], array[i + 1]);
        }
        return ret;
    }
}
