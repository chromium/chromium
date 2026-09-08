#!/usr/bin/env python3
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

import sys
import math

n = int(sys.argv[1])

assert n > 1
assert n <= 32
assert n & (n - 1) == 0

print(
    """\
// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
"""
)

print("#![allow(clippy::type_complexity)]")
print("#![allow(clippy::erasing_op)]")
print("#![allow(clippy::identity_op)]")

print("use jxl_simd::{F32SimdVec, SimdDescriptor};")

print()

variables = list(range(n))

next_var = n


def next():
    global next_var
    next_var += 1
    return next_var - 1


def wc_multiplier(i, N):
    return 1.0 / (2 * math.cos((i + 0.5) * math.pi / N))


def resampling_scale(i, N):
    return (
        math.cos(i / (16 * N) * math.pi)
        * math.cos(i / (8 * N) * math.pi)
        * math.cos(i / (4 * N) * math.pi)
        * N
    )


def inverse_even_odd(variables):
    n = len(variables)
    ret = [0] * len(variables)
    for i in range(n // 2):
        ret[2 * i] = variables[i]
    for i in range(n // 2):
        ret[2 * i + 1] = variables[n // 2 + i]
    return ret


def addsub_reverse(variables, op):
    n = len(variables)
    ret = [0] * (n // 2)
    for i in range(n // 2):
        ret[i] = next()
        print(
            "let v%d = v%d %s v%d;" % (ret[i], variables[i], op, variables[n - i - 1])
        )
    return ret


def b(variables):
    n = len(variables)
    ret = [0] * len(variables)
    ret[0] = next()
    print(
        "let v%d = v%d.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v%d);"
        % (ret[0], variables[0], variables[1])
    )
    for i in range(1, n - 1):
        ret[i] = next()
        print("let v%d = v%d + v%d;" % (ret[i], variables[i], variables[i + 1]))
    ret[n - 1] = variables[n - 1]
    return ret


def multiply(variables):
    n = len(variables)
    ret = [x for x in variables]
    for i in range(n):
        print("let mul = D::F32Vec::splat(d, %.16f);" % wc_multiplier(i, 2 * n))
        ret[i] = next()
        print("let v%d = v%d * mul;" % (ret[i], variables[i]))
    return ret


def call_reinterpreting_dct(variables):
    n = len(variables)
    print("(")
    for i in variables:
        print("v%d," % i)
    print(") = reinterpreting_dct_%d(d," % n)
    for i in variables:
        print("v%d," % i)
    print(");")


def dct(variables):
    n = len(variables)
    ret = [0] * len(variables)
    if n == 2:
        ret[0] = next()
        print("let v%d = v%d + v%d;" % (ret[0], variables[0], variables[1]))
        ret[1] = next()
        print("let v%d = v%d - v%d;" % (ret[1], variables[0], variables[1]))
    else:
        first_half = addsub_reverse(variables, "+")
        first_half = dct(first_half)
        second_half = addsub_reverse(variables, "-")
        second_half = multiply(second_half)
        second_half = dct(second_half)
        second_half = b(second_half)
        ret = first_half + second_half
        ret = inverse_even_odd(ret)

    return ret


print("#[allow(clippy::too_many_arguments)]")
print("#[allow(clippy::excessive_precision)]")
print("#[inline(always)]")
print("pub(super) fn reinterpreting_dct_%d<D: SimdDescriptor>(d: D," % n)
for v in variables:
    print("v%d: D::F32Vec," % v)
print(") -> (")
for _ in range(n):
    print("D::F32Vec,")
print(") {")

variables = dct(variables)

print("(")

for i, v in enumerate(variables):
    print("v%d * D::F32Vec::splat(d, %f)," % (v, 1.0 / resampling_scale(i, n)))

print(")")

print("}")


print()
print("#[inline(always)]")
print(
    "pub(super) fn do_reinterpreting_dct_%d<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], stride: usize) {"
    % n
)

print("assert!(data.len() > %d * stride);" % (n - 1))
for i in range(n):
    print("let mut v%d = D::F32Vec::load_array(d, &data[%d*stride]);" % (i, i))

call_reinterpreting_dct(list(range(n)))

for i in range(n):
    print("v%d.store_array(&mut data[%d*stride]);" % (i, i))

print("}")

if n > 2:
    # DCT on one row of KxK blocks, where K is the vector length.
    print()
    print("#[inline(always)]")
    print(
        "pub(super) fn do_reinterpreting_dct_%d_rowblock<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray]) {"
        % n
    )

    print("assert!(data.len() >= %d);" % n)
    print("const { assert!(%dusize.is_multiple_of(D::F32Vec::LEN)) };" % n)
    print("let row_stride = %d / D::F32Vec::LEN;" % n)

    for i in range(n):
        print(
            "let mut v%d = D::F32Vec::load_array(d, &data[row_stride*(%d %% D::F32Vec::LEN) + (%d / D::F32Vec::LEN)]);"
            % (i, i, i)
        )

    call_reinterpreting_dct(list(range(n)))

    for i in range(n):
        print(
            "v%d.store_array(&mut data[row_stride*(%d %% D::F32Vec::LEN) + (%d / D::F32Vec::LEN)]);"
            % (i, i, i)
        )

    print("}")

    # n-DCT + partial transpose of a n x (n/2) matrix
    print()
    print("#[inline(always)]")
    print(
        "pub(super) fn do_reinterpreting_dct_%d_trh<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray]) {"
        % n
    )

    print("let row_stride = %d / D::F32Vec::LEN;" % (n // 2))
    print("assert!(data.len() > %d * row_stride);" % (n - 1))
    print("const { assert!(%dusize.is_multiple_of(D::F32Vec::LEN)) };" % (n // 2))

    for i in range(n):
        print("let mut v%d = D::F32Vec::load_array(d, &data[row_stride*%d]);" % (i, i))

    call_reinterpreting_dct(list(range(n)))

    indices = inverse_even_odd(list(range(n)))

    for i in range(n):
        print("v%d.store_array(&mut data[row_stride*%d]);" % (indices[i], i))

    print("}")
