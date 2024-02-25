// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Test class for SuperSize. */
public class TestClass {
    static final int REPEAT = 3;

    static int[] reverse(int[] ar) {
        int n = ar.length;
        int[] ret = new int[n];
        for (int i = 0; i < n; ++i) {
            ret[i] = ar[n - 1 - i];
        }
        return ret;
    }

    long gcd(long a, long b) {
        while (b != 0) {
            long c = a % b;
            a = b;
            b = c;
        }
        return a;
    }

    double quadHi(double a, double b, double c) throws IllegalArgumentException {
        if (a == 0) {
            throw new IllegalArgumentException("a cannot be 0.");
        }
        double desc = b * b - 4 * a * c;
        if (desc < 0) {
            throw new IllegalArgumentException("Complex roots not supported.");
        }
        return (-b + Math.sqrt(desc)) / (2 * a);
    }

    String repeat(String a) {
        String ret = "";
        for (int i = 0; i < REPEAT; ++i) {
            ret = ret + 1;
        }
        return ret;
    }
}
