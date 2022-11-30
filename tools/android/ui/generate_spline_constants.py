#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Generates Java code to declare SPLINE_POSITION and SPLINE_TIME as precomputed
array.

Run this and paste the output into //chrome/android/java/src/org/chromium/
chrome/browser/compositor/layouts/phone/stack/StackScroller.java
"""

from __future__ import print_function

import math
import sys

def Main():
    # Keep these in sync with the values in //chrome/android/java/src/org/
    # chromium/chrome/browser/compositor/layouts/phone/stack/StackScroller.java
    NB_SAMPLES = 100
    INFLEXION = 0.35  # Tension lines cross at (INFLEXION, 1)

    START_TENSION = 0.5
    END_TENSION = 1.0
    P1 = START_TENSION * INFLEXION
    P2 = 1.0 - END_TENSION * (1.0 - INFLEXION)

    spline_time = []
    spline_position = []

    xMin = 0.0
    yMin = 0.0
    for i in range(NB_SAMPLES):
        alpha = float(i) / NB_SAMPLES

        xMax = 1.0
        while 1:
            x = xMin + (xMax - xMin) / 2.0
            coef = 3.0 * x * (1.0 - x)
            tx = coef * ((1.0 - x) * P1 + x * P2) + x * x * x
            if math.fabs(tx - alpha) < 1E-5:
                break
            if tx > alpha:
                xMax = x
            else:
                xMin = x

        spline_position.append(coef * ((1.0 - x) * START_TENSION + x)
                               + x * x * x)

        yMax = 1.0
        while 1:
            y = yMin + (yMax - yMin) / 2.0
            coef = 3.0 * y * (1.0 - y)
            dy = coef * ((1.0 - y) * START_TENSION + y) + y * y * y
            if math.fabs(dy - alpha) < 1E-5:
                break
            if dy > alpha:
                yMax = y
            else:
                yMin = y

        spline_time.append(coef * ((1.0 - y) * P1 + y * P2) + y * y * y)

    spline_position.append(1.0)
    spline_time.append(1.0)

    print(WriteJavaArrayDeclaration('SPLINE_POSITION', spline_position))
    print(WriteJavaArrayDeclaration('SPLINE_TIME', spline_time))

    return 0

def WriteJavaArrayDeclaration(name, float_list):
    MAX_CHARS_PER_LINE = 100
    INDENT_LEVELS = 2
    SPACES_PER_INDENT_LEVEL = 4
    DECLARATION_PREAMBLE = ' ' * SPACES_PER_INDENT_LEVEL * INDENT_LEVELS
    VALUES_PREAMBLE = ' ' * SPACES_PER_INDENT_LEVEL * (INDENT_LEVELS + 2)

    # Default precision is 6 decimal plates.
    FLOAT_LENGTH = len('0.123456, ')

    # +1 accounts for the trimmed space at the end.
    FLOATS_PER_LINE = ((MAX_CHARS_PER_LINE - len(VALUES_PREAMBLE) + 1)
                       / FLOAT_LENGTH)

    chunks = []
    for i in xrange(0, len(float_list), FLOATS_PER_LINE):
        float_chunk = float_list[i : i + FLOATS_PER_LINE]
        values = ', '.join(['%ff' % f for f in float_chunk])
        chunks.append(VALUES_PREAMBLE + values)

    s = DECLARATION_PREAMBLE + 'private static final float[] %s = {\n' % name
    s += ',\n'.join(chunks)
    s += '\n'
    s += DECLARATION_PREAMBLE + '};'
    return s

if __name__ == '__main__':
    sys.exit(Main())
