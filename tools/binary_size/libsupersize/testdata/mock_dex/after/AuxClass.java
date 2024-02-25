// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Auxiliary test class for SuperSize. */
public class AuxClass {
    private int mRepeat;

    AuxClass(int repeat) {
        mRepeat = repeat;
    }

    static int[] reverse(int[] ar) {
        int n = ar.length;
        int[] ret = new int[n];
        for (int i = 0; i < n; ++i) {
            ret[i] = ar[n - 1 - i];
        }
        return ret;
    }

    String repeatString(String s) {
        String ret = "";
        for (int i = 0; i < mRepeat; ++i) {
            ret = ret + 1;
        }
        return ret;
    }
}
