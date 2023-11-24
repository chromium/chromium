// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Test class for SuperSize. */
public class TestClass {
    static final int REPEAT = 3;

    long gcd(long a, long b) {
        while (b != 0) {
            long c = a % b;
            a = b;
            b = c;
        }
        return a;
    }

    long lcm(long a, long b) {
        long g = gcd(a, b);
        return g == 0 ? 0 : a / g * b;
    }

    double quadHi(double a, double b, double c) throws IllegalArgumentException {
        if (a == 0) {
            throw new IllegalArgumentException("a cannot be 0.");
        }
        double bb = b * b;
        double ac4 = a * c * 4;
        if (bb < ac4) {
            throw new IllegalArgumentException("Complex roots not supported.");
        }
        return (Math.sqrt(bb - ac4) - b) / (a * 2);
    }

    String repeat(String a) {
        AuxClass aux = new AuxClass(REPEAT);
        return aux.repeatString(a);
    }
}
