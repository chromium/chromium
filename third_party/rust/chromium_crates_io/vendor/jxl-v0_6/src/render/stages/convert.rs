// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::sync::{Arc, RwLock};

use crate::{
    frame::quantizer::LfQuantFactors,
    headers::bit_depth::BitDepth,
    render::{Channels, ChannelsMut, ErasedLocalState, RenderPipelineInOutStage, StageSpecialCase},
};
use jxl_simd::{F32SimdVec, I32SimdVec, SimdMask, simd_function};

// 32x32 blue noise dithering pattern from
// https://momentsingraphics.de/BlueNoise.html#Downloads scaled to have
// an average of 0 and be fully contained in (0.49219 to -0.49219).
// Rows are padded to 48 (32 + 16) to allow SIMD to wrap around horizontally
const DITHER_TABLE: [[f32; 48]; 32] = [
    [
        -0.26057, 0.32619, 0.21039, -0.03281, -0.10616, 0.16792, 0.43042, -0.48061, -0.00965,
        -0.31075, 0.24899, -0.35322, -0.02509, -0.25285, 0.02895, 0.10230, -0.28373, -0.00193,
        0.23355, 0.43428, -0.23741, 0.18336, -0.31847, -0.11002, -0.36094, 0.26057, -0.19108,
        -0.29531, 0.40726, -0.09458, 0.11002, -0.48833, -0.26057, 0.32619, 0.21039, -0.03281,
        -0.10616, 0.16792, 0.43042, -0.48061, -0.00965, -0.31075, 0.24899, -0.35322, -0.02509,
        -0.25285, 0.02895, 0.10230,
    ],
    [
        0.16020, -0.35708, -0.18336, 0.36094, -0.28373, -0.34550, -0.20267, 0.07914, 0.35708,
        -0.41498, 0.47675, -0.21811, -0.12546, 0.44200, -0.41884, -0.17178, 0.39954, 0.33778,
        -0.33778, 0.04053, -0.46517, 0.27215, -0.16792, 0.39182, 0.20653, -0.43814, -0.02895,
        0.17950, -0.41498, 0.01737, 0.24899, 0.49219, 0.16020, -0.35708, -0.18336, 0.36094,
        -0.28373, -0.34550, -0.20267, 0.07914, 0.35708, -0.41498, 0.47675, -0.21811, -0.12546,
        0.44200, -0.41884, -0.17178,
    ],
    [
        -0.00965, 0.08300, 0.41112, -0.46903, 0.04053, 0.47289, 0.26057, -0.05983, -0.13704,
        0.14862, 0.03281, 0.29531, -0.45744, 0.22583, 0.14862, -0.09072, -0.37638, 0.19881,
        -0.14476, 0.14476, -0.09072, 0.48447, -0.39954, 0.06369, -0.05983, -0.26829, 0.43428,
        -0.12546, 0.28759, -0.22969, -0.32619, -0.15248, -0.00965, 0.08300, 0.41112, -0.46903,
        0.04053, 0.47289, 0.26057, -0.05983, -0.13704, 0.14862, 0.03281, 0.29531, -0.45744,
        0.22583, 0.14862, -0.09072,
    ],
    [
        -0.42270, 0.23741, -0.23355, -0.11774, 0.18722, 0.11388, -0.43814, -0.24899, 0.41884,
        0.21039, -0.28373, -0.06756, 0.07914, 0.36480, -0.31075, 0.30303, -0.03281, 0.07142,
        -0.42656, 0.38024, -0.27987, 0.00579, 0.12546, -0.22197, 0.29917, 0.36866, 0.13704,
        -0.47289, 0.09072, 0.35708, -0.04825, 0.38796, -0.42270, 0.23741, -0.23355, -0.11774,
        0.18722, 0.11388, -0.43814, -0.24899, 0.41884, 0.21039, -0.28373, -0.06756, 0.07914,
        0.36480, -0.31075, 0.30303,
    ],
    [
        -0.28759, -0.07142, 0.44200, 0.27601, -0.38024, -0.16020, -0.01737, 0.30303, -0.33006,
        -0.40340, -0.16792, 0.40726, -0.36480, -0.00579, -0.19108, 0.41498, -0.26443, 0.46903,
        -0.21811, 0.28759, -0.04053, 0.22197, 0.34550, -0.44972, -0.14476, -0.34164, 0.04053,
        -0.19494, 0.45358, -0.37252, 0.21425, 0.05597, -0.28759, -0.07142, 0.44200, 0.27601,
        -0.38024, -0.16020, -0.01737, 0.30303, -0.33006, -0.40340, -0.16792, 0.40726, -0.36480,
        -0.00579, -0.19108, 0.41498,
    ],
    [
        0.31075, 0.14090, -0.33778, 0.00579, 0.34550, -0.29917, 0.38796, 0.13704, 0.05983,
        -0.10230, 0.34164, 0.10616, -0.23741, 0.19494, -0.47675, 0.04439, -0.39568, 0.24127,
        0.10616, -0.49219, -0.17950, -0.36094, -0.30303, 0.45744, -0.01351, 0.24513, -0.39182,
        -0.07528, 0.18722, -0.26057, -0.11002, -0.45358, 0.31075, 0.14090, -0.33778, 0.00579,
        0.34550, -0.29917, 0.38796, 0.13704, 0.05983, -0.10230, 0.34164, 0.10616, -0.23741,
        0.19494, -0.47675, 0.04439,
    ],
    [
        0.46903, -0.17178, -0.41112, 0.07528, -0.09458, 0.21811, -0.20267, -0.48833, 0.44972,
        0.00965, 0.24127, -0.42656, 0.48447, -0.11774, 0.26443, 0.14090, -0.15634, -0.07142,
        -0.32233, 0.36094, 0.42270, 0.19108, 0.07142, -0.11002, 0.15634, 0.38024, -0.28759,
        0.27987, -0.00193, 0.33006, 0.11388, -0.21039, 0.46903, -0.17178, -0.41112, 0.07528,
        -0.09458, 0.21811, -0.20267, -0.48833, 0.44972, 0.00965, 0.24127, -0.42656, 0.48447,
        -0.11774, 0.26443, 0.14090,
    ],
    [
        0.02123, 0.17950, 0.38024, -0.24127, -0.44586, 0.48833, -0.03667, 0.26829, -0.36866,
        -0.22583, 0.17178, -0.30689, 0.29145, -0.04825, -0.35322, 0.43042, 0.34936, 0.00193,
        0.16792, -0.12932, 0.03667, -0.06756, 0.31847, -0.40726, -0.24513, 0.09458, -0.17564,
        0.47675, -0.43042, -0.32233, 0.40340, 0.26057, 0.02123, 0.17950, 0.38024, -0.24127,
        -0.44586, 0.48833, -0.03667, 0.26829, -0.36866, -0.22583, 0.17178, -0.30689, 0.29145,
        -0.04825, -0.35322, 0.43042,
    ],
    [
        -0.47675, -0.12160, -0.04825, 0.28759, 0.10230, 0.15634, -0.14862, -0.27601, 0.36094,
        -0.12932, -0.05983, -0.45358, -0.17950, 0.01737, 0.09458, -0.29145, -0.22969, -0.43428,
        0.45744, -0.38796, -0.27601, -0.21039, -0.46131, 0.22969, 0.41112, -0.05211, -0.48061,
        0.16406, 0.05211, -0.14862, -0.03281, -0.36866, -0.47675, -0.12160, -0.04825, 0.28759,
        0.10230, 0.15634, -0.14862, -0.27601, 0.36094, -0.12932, -0.05983, -0.45358, -0.17950,
        0.01737, 0.09458, -0.29145,
    ],
    [
        -0.27215, 0.34164, -0.31075, 0.42656, -0.38410, -0.32619, 0.02895, 0.19881, 0.08300,
        0.42270, 0.31461, 0.13318, 0.45744, 0.37638, -0.40726, 0.31847, -0.08686, 0.21425, 0.29917,
        0.07914, 0.26829, 0.13704, 0.48447, -0.15248, 0.02509, -0.34936, 0.34936, -0.10230,
        0.42656, -0.23741, 0.22583, 0.09072, -0.27215, 0.34164, -0.31075, 0.42656, -0.38410,
        -0.32619, 0.02895, 0.19881, 0.08300, 0.42270, 0.31461, 0.13318, 0.45744, 0.37638, -0.40726,
        0.31847,
    ],
    [
        0.44972, 0.20267, 0.04825, -0.21425, 0.24513, -0.07142, 0.39954, -0.46131, -0.39568,
        -0.01351, -0.33392, 0.05597, -0.26443, 0.22197, -0.20653, 0.15248, 0.04439, -0.46517,
        -0.16406, -0.04439, -0.34936, 0.37252, -0.01351, -0.30689, 0.29917, 0.20653, -0.26829,
        0.26443, 0.13318, -0.39954, 0.30303, -0.08686, 0.44972, 0.20267, 0.04825, -0.21425,
        0.24513, -0.07142, 0.39954, -0.46131, -0.39568, -0.01351, -0.33392, 0.05597, -0.26443,
        0.22197, -0.20653, 0.15248,
    ],
    [
        -0.42656, 0.12932, -0.14476, -0.46903, -0.00579, 0.34936, -0.18722, 0.28373, -0.23741,
        0.22969, -0.16020, -0.38024, -0.08300, -0.48447, -0.02123, -0.14862, 0.48061, -0.31847,
        0.39568, -0.24899, 0.18722, -0.41884, 0.10230, -0.08300, -0.38796, 0.06369, -0.19881,
        -0.44972, 0.00579, -0.33392, 0.37252, -0.19108, -0.42656, 0.12932, -0.14476, -0.46903,
        -0.00579, 0.34936, -0.18722, 0.28373, -0.23741, 0.22969, -0.16020, -0.38024, -0.08300,
        -0.48447, -0.02123, -0.14862,
    ],
    [
        -0.02509, -0.35708, 0.32619, 0.46517, 0.17178, -0.28373, 0.10616, 0.47675, -0.09458,
        0.15248, 0.43428, 0.35322, 0.17564, 0.27215, 0.41112, -0.36480, 0.24899, 0.11774, 0.01351,
        0.33006, -0.11388, -0.18336, 0.41884, -0.23355, 0.16406, 0.46131, 0.38410, -0.04825,
        -0.15634, 0.49219, 0.17564, 0.03667, -0.02509, -0.35708, 0.32619, 0.46517, 0.17178,
        -0.28373, 0.10616, 0.47675, -0.09458, 0.15248, 0.43428, 0.35322, 0.17564, 0.27215, 0.41112,
        -0.36480,
    ],
    [
        0.40726, 0.23355, -0.25285, -0.08300, -0.41112, -0.12160, -0.35708, 0.05211, -0.41884,
        -0.29531, 0.02123, -0.21425, 0.09844, -0.30689, -0.11388, 0.34550, -0.26443, -0.07142,
        -0.39954, 0.44586, 0.05983, -0.48833, 0.24127, 0.34936, -0.44200, -0.12546, 0.12160,
        -0.30303, 0.27215, 0.07528, -0.48447, -0.29145, 0.40726, 0.23355, -0.25285, -0.08300,
        -0.41112, -0.12160, -0.35708, 0.05211, -0.41884, -0.29531, 0.02123, -0.21425, 0.09844,
        -0.30689, -0.11388, 0.34550,
    ],
    [
        0.28373, -0.17564, 0.09458, 0.02123, 0.30689, 0.41884, 0.20653, -0.03667, 0.32233, 0.25671,
        -0.45744, -0.05597, 0.46517, -0.41498, 0.00965, 0.07142, -0.44586, 0.16406, -0.20653,
        0.21811, -0.29917, 0.28759, -0.05597, 0.03281, -0.32619, -0.00965, 0.31847, -0.37252,
        0.18722, -0.11002, -0.22969, -0.06369, 0.28373, -0.17564, 0.09458, 0.02123, 0.30689,
        0.41884, 0.20653, -0.03667, 0.32233, 0.25671, -0.45744, -0.05597, 0.46517, -0.41498,
        0.00965, 0.07142,
    ],
    [
        -0.39568, 0.36866, -0.45744, -0.31847, 0.14476, -0.22583, -0.49219, 0.37638, -0.19494,
        -0.13318, 0.39182, -0.35322, 0.29531, -0.24127, 0.21039, -0.18722, 0.45358, 0.31461,
        -0.13318, -0.01737, -0.36094, 0.12932, -0.25671, 0.43814, -0.16792, 0.23355, -0.22197,
        0.44972, -0.42270, 0.33392, 0.42656, 0.11774, -0.39568, 0.36866, -0.45744, -0.31847,
        0.14476, -0.22583, -0.49219, 0.37638, -0.19494, -0.13318, 0.39182, -0.35322, 0.29531,
        -0.24127, 0.21039, -0.18722,
    ],
    [
        -0.13318, 0.19494, -0.03667, 0.44972, 0.24513, -0.15248, 0.08300, -0.33006, 0.00579,
        0.12546, 0.19494, 0.05983, -0.15634, 0.14476, 0.36480, -0.04053, -0.33006, 0.25671,
        -0.46903, 0.37252, 0.48833, -0.09458, -0.41112, 0.19108, 0.08686, -0.46903, -0.07528,
        0.04053, -0.26829, -0.02895, 0.22197, -0.34164, -0.13318, 0.19494, -0.03667, 0.44972,
        0.24513, -0.15248, 0.08300, -0.33006, 0.00579, 0.12546, 0.19494, 0.05983, -0.15634,
        0.14476, 0.36480, -0.04053,
    ],
    [
        0.47289, -0.21811, 0.06756, -0.38410, -0.27987, -0.06369, 0.27987, 0.43814, -0.25671,
        -0.39182, 0.49219, -0.27601, -0.07914, -0.48061, 0.42656, -0.38410, 0.11002, 0.03667,
        -0.27215, 0.15634, 0.07528, -0.22197, 0.33006, 0.38410, -0.34936, 0.27987, 0.15248,
        0.40340, 0.09844, -0.16406, -0.46131, 0.03281, 0.47289, -0.21811, 0.06756, -0.38410,
        -0.27987, -0.06369, 0.27987, 0.43814, -0.25671, -0.39182, 0.49219, -0.27601, -0.07914,
        -0.48061, 0.42656, -0.38410,
    ],
    [
        -0.29531, 0.31461, -0.10616, 0.39954, 0.01351, 0.33778, -0.43814, 0.17178, -0.08686,
        0.23741, -0.44586, 0.33778, -0.00193, -0.31461, 0.23741, -0.12932, -0.22583, -0.06756,
        0.40340, -0.16792, -0.43428, 0.01351, -0.14476, -0.04053, -0.29145, 0.46517, -0.13704,
        -0.39182, -0.32233, 0.29531, 0.38410, 0.16020, -0.29531, 0.31461, -0.10616, 0.39954,
        0.01351, 0.33778, -0.43814, 0.17178, -0.08686, 0.23741, -0.44586, 0.33778, -0.00193,
        -0.31461, 0.23741, -0.12932,
    ],
    [
        -0.44200, 0.26443, 0.12546, -0.42270, 0.21425, -0.19881, -0.35708, 0.04825, 0.36480,
        -0.02895, -0.21425, 0.09072, 0.41498, 0.18336, 0.04439, 0.29917, 0.47675, -0.40340,
        0.27601, -0.31461, 0.31075, 0.17564, 0.24899, -0.45744, 0.05597, -0.19494, 0.00193,
        0.36094, 0.24127, -0.09844, -0.24513, -0.00965, -0.44200, 0.26443, 0.12546, -0.42270,
        0.21425, -0.19881, -0.35708, 0.04825, 0.36480, -0.02895, -0.21425, 0.09072, 0.41498,
        0.18336, 0.04439, 0.29917,
    ],
    [
        -0.17564, -0.05597, -0.34550, -0.24899, 0.48061, 0.15248, -0.11388, 0.45358, -0.16406,
        -0.32233, 0.31461, -0.11774, -0.36866, -0.18722, -0.25671, -0.44200, 0.13318, -0.02123,
        0.19881, -0.10616, 0.43042, -0.36866, -0.24899, 0.41112, 0.11002, 0.21425, -0.25671,
        -0.47675, -0.04439, 0.13704, -0.37252, 0.43814, -0.17564, -0.05597, -0.34550, -0.24899,
        0.48061, 0.15248, -0.11388, 0.45358, -0.16406, -0.32233, 0.31461, -0.11774, -0.36866,
        -0.18722, -0.25671, -0.44200,
    ],
    [
        0.19108, 0.03667, 0.35708, -0.14090, 0.08300, -0.02123, -0.30303, -0.48061, 0.11774,
        0.20267, -0.43042, 0.25285, 0.14090, -0.04439, 0.38796, 0.34550, -0.34164, -0.19494,
        0.05983, -0.48447, 0.09844, -0.00579, -0.07914, 0.33778, -0.41498, -0.10230, 0.30689,
        0.17178, 0.48833, -0.20267, 0.07914, 0.33392, 0.19108, 0.03667, 0.35708, -0.14090, 0.08300,
        -0.02123, -0.30303, -0.48061, 0.11774, 0.20267, -0.43042, 0.25285, 0.14090, -0.04439,
        0.38796, 0.34550,
    ],
    [
        -0.48833, -0.30689, 0.41498, 0.22969, -0.44586, 0.32233, 0.25285, 0.39182, -0.23355,
        0.01737, 0.42270, -0.27987, 0.46903, -0.47289, 0.02123, -0.09072, 0.21811, 0.44586,
        -0.25285, 0.36480, -0.29145, 0.47289, -0.18722, 0.14476, -0.31461, 0.43814, -0.36094,
        0.04439, -0.29917, -0.41884, 0.25285, -0.11774, -0.48833, -0.30689, 0.41498, 0.22969,
        -0.44586, 0.32233, 0.25285, 0.39182, -0.23355, 0.01737, 0.42270, -0.27987, 0.46903,
        -0.47289, 0.02123, -0.09072,
    ],
    [
        0.46131, 0.11388, -0.21039, -0.07528, -0.38024, -0.26057, 0.06369, -0.05983, 0.29145,
        -0.40340, -0.09072, 0.06756, -0.16020, 0.27601, -0.31075, 0.10616, -0.14090, -0.43042,
        0.25671, -0.05211, -0.13318, 0.23355, -0.44972, 0.02895, 0.26829, -0.02895, -0.17950,
        0.37252, -0.13704, 0.40726, 0.01351, -0.26443, 0.46131, 0.11388, -0.21039, -0.07528,
        -0.38024, -0.26057, 0.06369, -0.05983, 0.29145, -0.40340, -0.09072, 0.06756, -0.16020,
        0.27601, -0.31075, 0.10616,
    ],
    [
        -0.03281, -0.40340, 0.27987, 0.17564, 0.02509, 0.44200, -0.15248, -0.34550, 0.14862,
        -0.19881, -0.01351, 0.36866, -0.38796, 0.19494, -0.22197, 0.32619, -0.37638, 0.00193,
        0.30689, 0.12160, -0.39182, 0.16792, -0.34550, 0.39954, -0.23355, 0.09072, -0.43428,
        0.22969, -0.06369, 0.12546, -0.35322, 0.30689, -0.03281, -0.40340, 0.27987, 0.17564,
        0.02509, 0.44200, -0.15248, -0.34550, 0.14862, -0.19881, -0.01351, 0.36866, -0.38796,
        0.19494, -0.22197, 0.32619,
    ],
    [
        -0.09844, 0.06756, 0.38410, -0.33392, -0.18336, 0.35322, 0.21039, -0.42270, 0.48833,
        0.33006, 0.21811, -0.33392, 0.12932, -0.05211, 0.39568, 0.04825, 0.48061, 0.17950,
        -0.31847, -0.21811, 0.38024, 0.05211, 0.32233, -0.06756, -0.12546, 0.46131, 0.16020,
        -0.25285, 0.29531, -0.44972, 0.17950, -0.16406, -0.09844, 0.06756, 0.38410, -0.33392,
        -0.18336, 0.35322, 0.21039, -0.42270, 0.48833, 0.33006, 0.21811, -0.33392, 0.12932,
        -0.05211, 0.39568, 0.04825,
    ],
    [
        0.22583, -0.46131, -0.27601, -0.00579, 0.12932, -0.47289, -0.09844, 0.10230, -0.28759,
        -0.12160, -0.49219, -0.24127, 0.44586, -0.11388, -0.45358, -0.27215, -0.17178, -0.07528,
        -0.47675, 0.43042, -0.02509, -0.27215, -0.19108, 0.19881, -0.49219, -0.37252, 0.33392,
        -0.00193, -0.33006, -0.20267, 0.48061, 0.34164, 0.22583, -0.46131, -0.27601, -0.00579,
        0.12932, -0.47289, -0.09844, 0.10230, -0.28759, -0.12160, -0.49219, -0.24127, 0.44586,
        -0.11388, -0.45358, -0.27215,
    ],
    [
        -0.22969, 0.42270, -0.12160, 0.31075, 0.46903, -0.22583, 0.27215, -0.02509, 0.03281,
        0.40340, 0.25671, 0.08686, 0.00965, 0.29145, -0.41112, 0.14090, 0.24513, 0.34164, 0.08686,
        -0.14862, 0.27601, -0.42656, 0.48447, 0.09844, 0.26443, -0.27987, 0.05597, -0.10230,
        0.43428, 0.08686, 0.02895, -0.38024, -0.22969, 0.42270, -0.12160, 0.31075, 0.46903,
        -0.22583, 0.27215, -0.02509, 0.03281, 0.40340, 0.25671, 0.08686, 0.00965, 0.29145,
        -0.41112, 0.14090,
    ],
    [
        0.15634, 0.09458, -0.36480, 0.18336, -0.05211, -0.40726, 0.36866, -0.33778, -0.19881,
        0.16020, -0.37638, -0.16020, -0.29917, 0.20267, 0.41884, -0.01737, -0.34936, -0.24127,
        0.02509, 0.20653, -0.36480, -0.08686, 0.01737, -0.33778, 0.41498, -0.03667, 0.37638,
        -0.17178, -0.47289, 0.26829, -0.28759, -0.05597, 0.15634, 0.09458, -0.36480, 0.18336,
        -0.05211, -0.40726, 0.36866, -0.33778, -0.19881, 0.16020, -0.37638, -0.16020, -0.29917,
        0.20267, 0.41884, -0.01737,
    ],
    [
        0.35708, 0.00193, 0.25285, -0.15634, -0.30303, 0.06369, 0.22197, 0.45358, -0.43814,
        0.30303, -0.04053, 0.46517, 0.35322, -0.21039, 0.06756, -0.14090, 0.37638, -0.43042,
        0.45744, -0.29531, 0.39568, 0.14862, 0.23741, -0.13704, -0.21425, 0.16406, -0.40726,
        0.22583, 0.13318, 0.38796, -0.12932, -0.43428, 0.35708, 0.00193, 0.25285, -0.15634,
        -0.30303, 0.06369, 0.22197, 0.45358, -0.43814, 0.30303, -0.04053, 0.46517, 0.35322,
        -0.21039, 0.06756, -0.14090,
    ],
    [
        -0.31461, -0.20653, 0.46131, -0.45358, 0.39568, -0.24513, -0.14090, 0.11002, -0.08300,
        -0.26829, 0.05211, -0.46517, -0.09844, -0.39568, -0.32619, -0.06369, 0.16792, 0.28373,
        0.11388, -0.04439, -0.18336, -0.44200, 0.35322, -0.26057, -0.46517, 0.31075, -0.07914,
        -0.34164, -0.24513, -0.02123, 0.19108, 0.44200, -0.31461, -0.20653, 0.46131, -0.45358,
        0.39568, -0.24513, -0.14090, 0.11002, -0.08300, -0.26829, 0.05211, -0.46517, -0.09844,
        -0.39568, -0.32619, -0.06369,
    ],
    [
        0.04825, -0.07914, -0.39954, 0.12160, 0.29145, 0.00965, -0.37638, 0.32233, 0.20267,
        -0.17564, 0.39182, 0.12160, 0.18336, 0.32619, 0.26057, 0.49219, -0.48447, -0.20653,
        -0.10616, -0.38796, 0.31847, 0.07528, -0.01737, 0.44586, 0.11774, 0.02509, 0.47289,
        0.07142, 0.33392, -0.38410, -0.17950, 0.28373, 0.04825, -0.07914, -0.39954, 0.12160,
        0.29145, 0.00965, -0.37638, 0.32233, 0.20267, -0.17564, 0.39182, 0.12160, 0.18336, 0.32619,
        0.26057, 0.49219,
    ],
];

pub struct ConvertModularXYBToF32Stage {
    first_channel: usize,
    lf_quant: Arc<RwLock<LfQuantFactors>>,
}

impl ConvertModularXYBToF32Stage {
    pub fn new(
        first_channel: usize,
        lf_quant: Arc<RwLock<LfQuantFactors>>,
    ) -> ConvertModularXYBToF32Stage {
        ConvertModularXYBToF32Stage {
            first_channel,
            lf_quant,
        }
    }
}

impl std::fmt::Display for ConvertModularXYBToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular xyb data to F32 in channels {}..{}",
            self.first_channel,
            self.first_channel + 2,
        )
    }
}

impl RenderPipelineInOutStage for ConvertModularXYBToF32Stage {
    type InputT = i32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        (self.first_channel..self.first_channel + 3).contains(&c)
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let lf_quant = self.lf_quant.try_read().unwrap();
        let [scale_x, scale_y, scale_b] = lf_quant.quant_factors;
        assert_eq!(
            input_rows.len(),
            3,
            "incorrect number of channels; expected 3, found {}",
            input_rows.len()
        );
        // Input channels: [Y, X, B] (modular XYB order)
        // Output channels: [X, Y, B] (standard XYB order)
        let (input_y, input_x, input_b) = (&input_rows[0], &input_rows[1], &input_rows[2]);
        let (output_x, output_y, output_b) = output_rows.split_first_3_mut();
        // TODO(veluca): SIMD this
        for i in 0..xsize {
            output_x[0][i] = input_x[0][i] as f32 * scale_x;
            output_y[0][i] = input_y[0][i] as f32 * scale_y;
            output_b[0][i] = (input_b[0][i] as f32 + input_y[0][i] as f32) * scale_b;
        }
    }
}

pub struct ConvertModularToF32Stage {
    channel: usize,
    bit_depth: BitDepth,
}

impl ConvertModularToF32Stage {
    pub fn new(channel: usize, bit_depth: BitDepth) -> ConvertModularToF32Stage {
        ConvertModularToF32Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertModularToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular data to F32 in channel {} with bit depth {:?}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD 32-bit float passthrough (bitcast i32 to f32)
simd_function!(
    int_to_float_32bit_simd_dispatch,
    d: D,
    fn int_to_float_32bit_simd(input: &[i32], output: &mut [f32], xsize: usize) {
        let simd_width = D::I32Vec::LEN;

        // Process complete SIMD vectors
        for (in_chunk, out_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::I32Vec::load(d, in_chunk);
            val.bitcast_to_f32().store(out_chunk);
        }
    }
);

// SIMD 16-bit float (half-precision) to 32-bit float conversion
// Uses hardware F16C/NEON instructions when available via F32Vec::load_f16_bits()
simd_function!(
    int_to_float_16bit_simd_dispatch,
    d: D,
    fn int_to_float_16bit_simd(input: &[i32], output: &mut [f32], xsize: usize) {
        let simd_width = D::F32Vec::LEN;

        // Temporary buffer for i32->u16 conversion via SIMD
        // Note: Using constant 16 (max AVX-512 width) because D::F32Vec::LEN
        // cannot be used as array size in Rust (const generics limitation)
        const { assert!(D::F32Vec::LEN <= 16) }
        let mut u16_buf = [0u16; 16];

        // Process complete SIMD vectors
        for (in_chunk, out_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            // Use SIMD to extract lower 16 bits from each i32 lane
            let i32_vec = D::I32Vec::load(d, in_chunk);
            i32_vec.store_u16(&mut u16_buf[..simd_width]);
            // Use hardware f16->f32 conversion
            let result = D::F32Vec::load_f16_bits(d, &u16_buf[..simd_width]);
            result.store(out_chunk);
        }
    }
);

// Converts custom [bits]-bit float (with [exp_bits] exponent bits) stored as
// int back to binary32 float.
fn int_to_float(input: &[i32], output: &mut [f32], bit_depth: &BitDepth, xsize: usize) {
    assert_eq!(input.len(), output.len());
    let bits = bit_depth.bits_per_sample();
    let exp_bits = bit_depth.exponent_bits_per_sample();

    // Use SIMD fast paths for common formats
    if bits == 32 && exp_bits == 8 {
        // 32-bit float passthrough
        int_to_float_32bit_simd_dispatch(input, output, xsize);
        return;
    }

    if bits == 16 && exp_bits == 5 {
        // IEEE 754 half-precision (f16) - common HDR format
        int_to_float_16bit_simd_dispatch(input, output, xsize);
        return;
    }

    // Generic scalar path for other custom float formats
    int_to_float_generic(input, output, bits, exp_bits);
}

// Generic scalar conversion for arbitrary bit-depth floats
// TODO: SIMD optimization for custom float formats
fn int_to_float_generic(input: &[i32], output: &mut [f32], bits: u32, exp_bits: u32) {
    let exp_bias = (1 << (exp_bits - 1)) - 1;
    let sign_shift = bits - 1;
    let mant_bits = bits - exp_bits - 1;
    let mant_shift = 23 - mant_bits;
    for (&in_val, out_val) in input.iter().zip(output) {
        let mut f = in_val as u32;
        let signbit = (f >> sign_shift) != 0;
        f &= (1 << sign_shift) - 1;
        if f == 0 {
            *out_val = if signbit { -0.0 } else { 0.0 };
            continue;
        }
        let mut exp = (f >> mant_bits) as i32;
        let mut mantissa = f & ((1 << mant_bits) - 1);
        if exp == (1 << exp_bits) - 1 {
            // NaN or infinity
            f = if signbit { 0x80000000 } else { 0 };
            f |= 0b11111111 << 23;
            f |= mantissa << mant_shift;
            *out_val = f32::from_bits(f);
            continue;
        }
        mantissa <<= mant_shift;
        // Try to normalize only if there is space for maneuver.
        if exp == 0 && exp_bits < 8 {
            // subnormal number
            while (mantissa & 0x800000) == 0 {
                mantissa <<= 1;
                exp -= 1;
            }
            exp += 1;
            // remove leading 1 because it is implicit now
            mantissa &= 0x7fffff;
        }
        exp -= exp_bias;
        // broke up the arbitrary float into its parts, now reassemble into
        // binary32
        exp += 127;
        assert!(exp >= 0);
        f = if signbit { 0x80000000 } else { 0 };
        f |= (exp as u32) << 23;
        f |= mantissa;
        *out_val = f32::from_bits(f);
    }
}

// SIMD modular to 32 bit float conversion
simd_function!(
    modular_to_float_32bit_simd_dispatch,
    d: D,
    fn modular_to_float_32bit_simd(input: &[i32], output: &mut [f32], scale: f32, xsize: usize) {
        let simd_width = D::I32Vec::LEN;

        let scale = D::F32Vec::splat(d, scale);

        // Process complete SIMD vectors
        for (in_chunk, out_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::I32Vec::load(d, in_chunk);
            (val.as_f32() * scale).store(out_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertModularToF32Stage {
    type InputT = i32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let input = &input_rows[0];
        if self.bit_depth.floating_point_sample() {
            int_to_float(input[0], output_rows[0][0], &self.bit_depth, xsize);
        } else {
            let scale = 1.0 / ((1u64 << self.bit_depth.bits_per_sample()) - 1) as f32;
            modular_to_float_32bit_simd_dispatch(input[0], output_rows[0][0], scale, xsize);
        }
    }

    fn is_special_case(&self) -> Option<StageSpecialCase> {
        if self.bit_depth.floating_point_sample() {
            None
        } else {
            Some(StageSpecialCase::ModularToF32 {
                channel: self.channel,
                bit_depth: self.bit_depth.bits_per_sample() as u8,
            })
        }
    }
}

/// Stage that converts f32 values in [0, 1] range to u8 values.
pub struct ConvertF32ToU8Stage {
    channel: usize,
    bit_depth: u8,
}

impl ConvertF32ToU8Stage {
    pub fn new(channel: usize, bit_depth: u8) -> ConvertF32ToU8Stage {
        ConvertF32ToU8Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertF32ToU8Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert F32 to U8 in channel {} with bit depth {}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD F32 to U8 conversion
simd_function!(
    f32_to_u8_simd_dispatch,
    d: D,
    fn f32_to_u8_simd(
        input: &[f32],
        output: &mut [u8],
        max: f32,
        position: (usize, usize),
        channel: usize,
        xsize: usize,
    ) {
        let (x0, y0) = position;
        let simd_width = D::F32Vec::LEN;
        let zero = D::F32Vec::splat(d, 0.0);
        let scale = D::F32Vec::splat(d, max);

        for (block, (input_chunk, output_chunk)) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
            .enumerate()
        {
            let x = block * simd_width;
            let val = D::F32Vec::load(d, input_chunk);
            let dither_x = (x0 + x + channel * 23) % 32;
            let dither_y = (y0 + channel * 13) % 32;
            let dither = D::F32Vec::load(
                d,
                &DITHER_TABLE[dither_y][dither_x..],
            );
            let scaled = val * scale;
            let dithered = scaled + dither;
            let clamped = dithered.max(zero).min(scale);
            clamped.round_store_u8(output_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertF32ToU8Stage {
    type InputT = f32;
    type OutputT = u8;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<u8>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let input = input_rows[0][0];
        let output = &mut output_rows[0][0];
        let max = ((1u32 << self.bit_depth) - 1) as f32;
        f32_to_u8_simd_dispatch(input, output, max, position, self.channel, xsize);
    }

    fn is_special_case(&self) -> Option<StageSpecialCase> {
        Some(StageSpecialCase::F32ToU8 {
            channel: self.channel,
            bit_depth: self.bit_depth,
        })
    }
}

/// Stage that converts i32 values to u8 values, applying a multiplier.
pub struct ConvertI32ToU8Stage {
    channel: usize,
    multiplier: i32,
    max: i32,
}

impl ConvertI32ToU8Stage {
    pub fn new(channel: usize, multiplier: i32, max: i32) -> ConvertI32ToU8Stage {
        ConvertI32ToU8Stage {
            channel,
            multiplier,
            max,
        }
    }
}

impl std::fmt::Display for ConvertI32ToU8Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert I32 to U8 in channel {} with multiplier {}",
            self.channel, self.multiplier
        )
    }
}

// SIMD I32 to U8 conversion
simd_function!(
    i32_to_u8_simd_dispatch,
    d: D,
    fn i32_to_u8_simd(input: &[i32], output: &mut [u8], scale: i32, max: i32, xsize: usize) {
        let simd_width = D::F32Vec::LEN;
        let scale = D::I32Vec::splat(d, scale);
        let max = D::I32Vec::splat(d, max);
        let zero = D::I32Vec::splat(d, 0);

        // Process SIMD vectors using div_ceil (buffers are padded)
        for (input_chunk, output_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::I32Vec::load(d, input_chunk);
            let scaled = val * scale;
            let zeroclip = scaled.lt_zero().if_then_else_i32(zero, scaled);
            let clip = scaled.gt(max).if_then_else_i32(max, zeroclip);
            clip.store_u8(output_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertI32ToU8Stage {
    type InputT = i32;
    type OutputT = u8;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<u8>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let input = input_rows[0][0];
        let output = &mut output_rows[0][0];
        i32_to_u8_simd_dispatch(input, output, self.multiplier, self.max, xsize);
    }
}

/// Stage that converts f32 values in [0, 1] range to u16 values.
pub struct ConvertF32ToU16Stage {
    channel: usize,
    bit_depth: u8,
}

impl ConvertF32ToU16Stage {
    pub fn new(channel: usize, bit_depth: u8) -> ConvertF32ToU16Stage {
        ConvertF32ToU16Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertF32ToU16Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert F32 to U16 in channel {} with bit depth {}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD F32 to U16 conversion
simd_function!(
    f32_to_u16_simd_dispatch,
    d: D,
    fn f32_to_u16_simd(input: &[f32], output: &mut [u16], max: f32, xsize: usize) {
        let simd_width = D::F32Vec::LEN;
        let zero = D::F32Vec::splat(d, 0.0);
        let one = D::F32Vec::splat(d, 1.0);
        let scale = D::F32Vec::splat(d, max);

        // Process SIMD vectors using div_ceil (buffers are padded)
        for (input_chunk, output_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::F32Vec::load(d, input_chunk);
            // Clamp to [0, 1] and scale
            let clamped = val.max(zero).min(one);
            let scaled = clamped * scale;
            scaled.round_store_u16(output_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertF32ToU16Stage {
    type InputT = f32;
    type OutputT = u16;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<u16>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let input = input_rows[0][0];
        let output = &mut output_rows[0][0];
        let max = ((1u32 << self.bit_depth) - 1) as f32;
        f32_to_u16_simd_dispatch(input, output, max, xsize);
    }
}

/// Stage that converts f32 values to f16 (half-precision float) values.
pub struct ConvertF32ToF16Stage {
    channel: usize,
    clamp_range: Option<(f32, f32)>,
}

impl ConvertF32ToF16Stage {
    pub fn new(channel: usize) -> ConvertF32ToF16Stage {
        ConvertF32ToF16Stage {
            channel,
            clamp_range: None,
        }
    }

    pub fn new_with_clamp_range(
        channel: usize,
        clamp_range: Option<(f32, f32)>,
    ) -> ConvertF32ToF16Stage {
        ConvertF32ToF16Stage {
            channel,
            clamp_range,
        }
    }

    pub fn new_with_unit_clamp(channel: usize, clamp_unit_range: bool) -> ConvertF32ToF16Stage {
        ConvertF32ToF16Stage {
            channel,
            clamp_range: if clamp_unit_range {
                Some((0.0, 1.0))
            } else {
                None
            },
        }
    }
}

impl std::fmt::Display for ConvertF32ToF16Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "convert F32 to F16 in channel {}", self.channel)
    }
}

impl RenderPipelineInOutStage for ConvertF32ToF16Stage {
    type InputT = f32;
    type OutputT = crate::util::f16;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<crate::util::f16>,
        _state: Option<&mut ErasedLocalState>,
    ) {
        let input = &input_rows[0];
        if let Some((min_value, max_value)) = self.clamp_range {
            for i in 0..xsize {
                output_rows[0][0][i] =
                    crate::util::f16::from_f32(input[0][i].clamp(min_value, max_value));
            }
        } else {
            for i in 0..xsize {
                output_rows[0][0][i] = crate::util::f16::from_f32(input[0][i]);
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::error::Result;
    use crate::headers::bit_depth::BitDepth;
    use test_log::test;

    #[test]
    fn f32_to_u8_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertF32ToU8Stage::new(0, 8),
            (500, 500),
            1,
        )
    }

    #[test]
    fn f32_to_u16_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertF32ToU16Stage::new(0, 16),
            (500, 500),
            1,
        )
    }

    #[test]
    fn f32_to_f16_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(|| ConvertF32ToF16Stage::new(0), (500, 500), 1)
    }

    #[test]
    fn f32_to_f16_consistency_with_unit_clamp() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertF32ToF16Stage::new_with_unit_clamp(0, true),
            (500, 500),
            1,
        )
    }

    /// Test ConvertModularToF32Stage consistency with different bit depths.
    #[test]
    fn modular_to_f32_8bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModularToF32Stage::new(0, BitDepth::integer_samples(8)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn modular_to_f32_16bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModularToF32Stage::new(0, BitDepth::integer_samples(16)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn test_int_to_float_32bit() {
        // Test 32-bit float passthrough
        let bit_depth = BitDepth::f32();
        let test_values: Vec<f32> = vec![
            0.0,
            1.0,
            -1.0,
            0.5,
            -0.5,
            f32::INFINITY,
            f32::NEG_INFINITY,
            1e-30,
            1e30,
        ];
        let input: Vec<i32> = test_values
            .iter()
            .map(|&f| f.to_bits() as i32)
            .chain(std::iter::repeat(0))
            .take(16)
            .collect();
        let mut output = vec![0.0f32; 16];

        int_to_float(&input, &mut output, &bit_depth, test_values.len());

        for (i, (&expected, &actual)) in test_values.iter().zip(output.iter()).enumerate() {
            if expected.is_nan() {
                assert!(actual.is_nan(), "index {}: expected NaN, got {}", i, actual);
            } else {
                assert_eq!(expected, actual, "index {}: mismatch", i);
            }
        }
    }

    #[test]
    fn test_int_to_float_16bit() {
        // Test 16-bit float (f16) conversion for normal values
        let bit_depth = BitDepth::f16();

        // f16 format: 1 sign, 5 exp, 10 mantissa
        // Test cases: (f16_bits, expected_f32)
        let test_cases: Vec<(u16, f32)> = vec![
            (0x0000, 0.0),               // +0
            (0x8000, -0.0),              // -0
            (0x3C00, 1.0),               // 1.0
            (0xBC00, -1.0),              // -1.0
            (0x3800, 0.5),               // 0.5
            (0x4000, 2.0),               // 2.0
            (0x4400, 4.0),               // 4.0
            (0x7BFF, 65504.0),           // max normal f16
            (0x7C00, f32::INFINITY),     // +inf
            (0xFC00, f32::NEG_INFINITY), // -inf
            (0x0001, 5.960_464_5e-8),    // smallest positive subnormal
            (0x03FF, 6.097_555e-5),      // largest positive subnormal
            (0x8001, -5.960_464_5e-8),   // smallest negative subnormal
        ];

        let input: Vec<i32> = test_cases
            .iter()
            .map(|(bits, _)| *bits as i32)
            .chain(std::iter::repeat(0))
            .take(16)
            .collect();
        let mut output = vec![0.0f32; 16];

        int_to_float(&input, &mut output, &bit_depth, test_cases.len());

        for (i, (&(_, expected), &actual)) in test_cases.iter().zip(output.iter()).enumerate() {
            assert!(
                (expected - actual).abs() < 1e-6
                    || expected == actual
                    || (expected.is_sign_negative() == actual.is_sign_negative()
                        && expected == 0.0
                        && actual == 0.0),
                "index {}: expected {}, got {}",
                i,
                expected,
                actual
            );
        }
    }
}
