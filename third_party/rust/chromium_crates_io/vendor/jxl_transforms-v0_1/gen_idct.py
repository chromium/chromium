#!/usr/bin/env python3
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

import sys
import math

n = int(sys.argv[1])

assert n > 1
assert n <= 256
assert n & (n - 1) == 0

print(
    """\
// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
"""
)

print("#![allow(unused)]")
print("#![allow(clippy::type_complexity)]")
print("#![allow(clippy::erasing_op)]")
print("#![allow(clippy::identity_op)]")

print("use jxl_simd::{F32SimdVec, SimdDescriptor};")
print("use crate::*;")

print()

variables = list(range(n))

next_var = n


def next():
    global next_var
    next_var += 1
    return next_var - 1


def wc_multiplier(i, N):
    return 1.0 / (2 * math.cos((i + 0.5) * math.pi / N))


def forward_even_odd(variables):
    n = len(variables)
    ret = [0] * len(variables)
    for i in range(n // 2):
        ret[i] = variables[2 * i]
    for i in range(n // 2, n):
        ret[i] = variables[2 * (i - n // 2) + 1]
    return ret


def b_transpose(variables):
    n = len(variables)
    ret = [0] * len(variables)
    for i in range(1, n):
        ret[i] = next()
        print("let mut v%d = v%d + v%d;" % (ret[i], variables[i - 1], variables[i]))
    ret[0] = next()
    print(
        "let mut v%d = v%d * D::F32Vec::splat(d, std::f32::consts::SQRT_2);"
        % (ret[0], variables[0])
    )
    return ret


def multiply_and_add(variables):
    n = len(variables)
    ret = [0] * len(variables)
    for i in range(n // 2):
        print("let mul = D::F32Vec::splat(d, %.16f);" % wc_multiplier(i, n))
        ret[i] = next()
        print(
            "let mut v%d = v%d.mul_add(mul, v%d);"
            % (ret[i], variables[n // 2 + i], variables[i])
        )
        ret[n - i - 1] = next()
        print(
            "let mut v%d = v%d.neg_mul_add(mul, v%d);"
            % (ret[n - i - 1], variables[n // 2 + i], variables[i])
        )
    return ret


def call_idct(variables):
    n = len(variables)
    print("(")
    for i in variables:
        print("v%d," % i)
    print(") = idct_%d(d," % n)
    for i in variables:
        print("v%d," % i)
    print(");")


def d_call_idct(variables):
    print("d.call(#[inline(always)] |_| {")
    call_idct(variables)
    print("});")


def idct(variables):
    n = len(variables)
    ret = [0] * len(variables)
    if n == 2:
        ret[0] = next()
        print("let mut v%d = v%d + v%d;" % (ret[0], variables[0], variables[1]))
        ret[1] = next()
        print("let mut v%d = v%d - v%d;" % (ret[1], variables[0], variables[1]))
    else:
        ret = forward_even_odd(variables)
        first_half = ret[: n // 2]
        second_half = ret[n // 2 :]
        first_half = idct(first_half)
        second_half = b_transpose(second_half)
        second_half = idct(second_half)
        ret = first_half + second_half
        ret = multiply_and_add(ret)

    return ret


print("#[allow(clippy::too_many_arguments)]")
print("#[allow(clippy::excessive_precision)]")
print("#[inline(always)]")
print("pub(super) fn idct_%d<D: SimdDescriptor>(d: D," % n)
for v in variables:
    print("mut v%d: D::F32Vec," % v)
print(") -> (")
for _ in range(n):
    print("D::F32Vec,")
print(") {")

variables = idct(variables)

print("(")

for v in variables:
    print("v%d," % v)

print(")")

print("}")


print()
print("#[inline(always)]")
print(
    "pub(super) fn do_idct_%d<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], stride: usize) {"
    % n
)

print("assert!(data.len() > %d * stride);" % (n - 1))
for i in range(n):
    print("let mut v%d = D::F32Vec::load_array(d, &data[%d*stride]);" % (i, i))

call_idct(list(range(n)))

for i in range(n):
    print("v%d.store_array(&mut data[%d*stride]);" % (i, i))

print("}")

# IDCT on one row of KxK blocks, where K is the vector length.
print()
print("#[inline(always)]")
print(
    "pub(super) fn do_idct_%d_rowblock<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray]) {"
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

call_idct(list(range(n)))

for i in range(n):
    print(
        "v%d.store_array(&mut data[row_stride*(%d %% D::F32Vec::LEN) + (%d / D::F32Vec::LEN)]);"
        % (i, i, i)
    )

print("}")


# n-IDCT + partial transpose of a n x (n/2) matrix
if n > 2:
    print()
    print("#[inline(always)]")
    print(
        "pub(super) fn do_idct_%d_trh<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray]) {"
        % n
    )

    print("let row_stride = %d / D::F32Vec::LEN;" % (n // 2))
    print("assert!(data.len() > %d * row_stride);" % (n - 1))
    print("const { assert!(%dusize.is_multiple_of(D::F32Vec::LEN)) };" % (n // 2))

    indices = forward_even_odd(list(range(n)))

    for i in range(n):
        print(
            "let mut v%d = D::F32Vec::load_array(d, &data[row_stride*%d]);"
            % (i, indices[i])
        )

    call_idct(list(range(n)))

    for i in range(n):
        print("v%d.store_array(&mut data[row_stride*%d]);" % (i, i))

    print("}")

# 32-IDCT + partial transpose of a 32x8 matrix
if n == 32:
    print()
    print("#[inline(always)]")
    print(
        "pub(super) fn do_idct_32_trq<D: SimdDescriptor>(d: D, data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray]) {"
    )

    print("let row_stride = 8 / D::F32Vec::LEN;")
    print("assert!(data.len() > 31 * row_stride);")
    print("const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };")

    indices = [
        0,
        4,
        8,
        12,
        16,
        20,
        24,
        28,
        1,
        5,
        9,
        13,
        17,
        21,
        25,
        29,
        2,
        6,
        10,
        14,
        18,
        22,
        26,
        30,
        3,
        7,
        11,
        15,
        19,
        23,
        27,
        31,
    ]

    for i in range(n):
        print(
            "let mut v%d = D::F32Vec::load_array(d, &data[row_stride*%d]);"
            % (i, indices[i])
        )

    call_idct(list(range(32)))

    for i in range(n):
        print("v%d.store_array(&mut data[row_stride*%d]);" % (i, i))

    print("}")
