// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This file contains a generic implementation of large (>32x32) 2d IDCTs.
// They are not implemented in the same way as smaller 2d IDCTs to reduce code size.

#![allow(clippy::excessive_precision)]

use std::f32::consts::SQRT_2;

use jxl_simd::F32SimdVec;
use jxl_simd::SimdDescriptor;

use crate::idct_32;

const WC_WEIGHTS_64: [f32; 32] = [
    0.500150636020651,
    0.5013584524464084,
    0.5037887256810443,
    0.5074711720725553,
    0.5124514794082247,
    0.5187927131053328,
    0.52657731515427,
    0.535909816907992,
    0.5469204379855088,
    0.5597698129470802,
    0.57465518403266,
    0.5918185358574165,
    0.6115573478825099,
    0.6342389366884031,
    0.6603198078137061,
    0.6903721282002123,
    0.7251205223771985,
    0.7654941649730891,
    0.8127020908144905,
    0.8683447152233481,
    0.9345835970364075,
    1.0144082649970547,
    1.1120716205797176,
    1.233832737976571,
    1.3892939586328277,
    1.5939722833856311,
    1.8746759800084078,
    2.282050068005162,
    2.924628428158216,
    4.084611078129248,
    6.796750711673633,
    20.373878167231453,
];

const WC_WEIGHTS_128: [f32; 64] = [
    0.5000376519155477,
    0.5003390374428216,
    0.5009427176380873,
    0.5018505174842379,
    0.5030651913013697,
    0.5045904432216454,
    0.5064309549285542,
    0.5085924210498143,
    0.5110815927066812,
    0.5139063298475396,
    0.5170756631334912,
    0.5205998663018917,
    0.524490540114724,
    0.5287607092074876,
    0.5334249333971333,
    0.538499435291984,
    0.5440022463817783,
    0.549953374183236,
    0.5563749934898856,
    0.5632916653417023,
    0.5707305880121454,
    0.5787218851348208,
    0.5872989370937893,
    0.5964987630244563,
    0.606362462272146,
    0.6169357260050706,
    0.6282694319707711,
    0.6404203382416639,
    0.6534518953751283,
    0.6674352009263413,
    0.6824501259764195,
    0.6985866506472291,
    0.7159464549705746,
    0.7346448236478627,
    0.7548129391165311,
    0.776600658233963,
    0.8001798956216941,
    0.8257487738627852,
    0.8535367510066064,
    0.8838110045596234,
    0.9168844461846523,
    0.9531258743921193,
    0.9929729612675466,
    1.036949040910389,
    1.0856850642580145,
    1.1399486751015042,
    1.2006832557294167,
    1.2690611716991191,
    1.346557628206286,
    1.4350550884414341,
    1.5369941008524954,
    1.6555965242641195,
    1.7952052190778898,
    1.961817848571166,
    2.163957818751979,
    2.4141600002500763,
    2.7316450287739396,
    3.147462191781909,
    3.7152427383269746,
    4.5362909369693565,
    5.827688377844654,
    8.153848602466814,
    13.58429025728446,
    40.744688103351834,
];

const WC_WEIGHTS_256: [f32; 128] = [
    0.5000094125358878,
    0.500084723455784,
    0.5002354020255269,
    0.5004615618093246,
    0.5007633734146156,
    0.5011410648064231,
    0.5015949217281668,
    0.502125288230386,
    0.5027325673091954,
    0.5034172216566842,
    0.5041797745258774,
    0.5050208107132756,
    0.5059409776624396,
    0.5069409866925212,
    0.5080216143561264,
    0.509183703931388,
    0.5104281670536573,
    0.5117559854927805,
    0.5131682130825206,
    0.5146659778093218,
    0.516250484068288,
    0.5179230150949777,
    0.5196849355823947,
    0.5215376944933958,
    0.5234828280796439,
    0.52552196311921,
    0.5276568203859896,
    0.5298892183652453,
    0.5322210772308335,
    0.5346544231010253,
    0.537191392591309,
    0.5398342376841637,
    0.5425853309375497,
    0.545447171055775,
    0.5484223888484947,
    0.551513753605893,
    0.554724179920619,
    0.5580567349898085,
    0.5615146464335654,
    0.5651013106696203,
    0.5688203018875696,
    0.5726753816701664,
    0.5766705093136241,
    0.5808098529038624,
    0.5850978012111273,
    0.58953897647151,
    0.5941382481306648,
    0.5989007476325463,
    0.6038318843443582,
    0.6089373627182432,
    0.614223200800649,
    0.6196957502119484,
    0.6253617177319102,
    0.6312281886412079,
    0.6373026519855411,
    0.6435930279473415,
    0.6501076975307724,
    0.6568555347890955,
    0.6638459418498757,
    0.6710888870233562,
    0.6785949463131795,
    0.6863753486870501,
    0.6944420255086364,
    0.7028076645818034,
    0.7114857693151208,
    0.7204907235796304,
    0.7298378629074134,
    0.7395435527641373,
    0.749625274727372,
    0.7601017215162176,
    0.7709929019493761,
    0.7823202570613161,
    0.7941067887834509,
    0.8063772028037925,
    0.8191580674598145,
    0.83247799080191,
    0.8463678182968619,
    0.860860854031955,
    0.8759931087426972,
    0.8918035785352535,
    0.9083345588266809,
    0.9256319988042384,
    0.9437459026371479,
    0.962730784794803,
    0.9826461881778968,
    1.0035572754078206,
    1.0255355056139732,
    1.048659411496106,
    1.0730154944316674,
    1.0986992590905857,
    1.1258164135986009,
    1.1544842669978943,
    1.184833362908442,
    1.217009397314603,
    1.2511754798461228,
    1.287514812536712,
    1.326233878832723,
    1.3675662599582539,
    1.411777227500661,
    1.459169302866857,
    1.5100890297227016,
    1.5649352798258847,
    1.6241695131835794,
    1.6883285509131505,
    1.7580406092704062,
    1.8340456094306077,
    1.9172211551275689,
    2.0086161135167564,
    2.1094945286246385,
    2.22139377701127,
    2.346202662531156,
    2.486267909203593,
    2.644541877144861,
    2.824791402350551,
    3.0318994541759925,
    3.2723115884254845,
    3.5547153325075804,
    3.891107790700307,
    4.298537526449054,
    4.802076008665048,
    5.440166215091329,
    6.274908408039339,
    7.413566756422303,
    9.058751453879703,
    11.644627325175037,
    16.300023088031555,
    27.163977662448232,
    81.48784219222516,
];

#[inline(always)]
fn idct_impl_inner<D: SimdDescriptor>(d: D, data: &mut [D::F32Vec], scratch: &mut [D::F32Vec]) {
    let n = data.len();
    assert!(scratch.len() >= n);

    if n == 32 {
        d.call(
            #[inline(always)]
            |_| {
                (
                    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15],
                    data[16], data[17], data[18], data[19], data[20], data[21], data[22], data[23],
                    data[24], data[25], data[26], data[27], data[28], data[29], data[30], data[31],
                ) = idct_32(
                    d, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15],
                    data[16], data[17], data[18], data[19], data[20], data[21], data[22], data[23],
                    data[24], data[25], data[26], data[27], data[28], data[29], data[30], data[31],
                )
            },
        );
        return;
    }

    let wc_weights = match n {
        64 => &WC_WEIGHTS_64[..],
        128 => &WC_WEIGHTS_128[..],
        256 => &WC_WEIGHTS_256[..],
        _ => unreachable!("invalid large-dct size: {n}"),
    };

    assert_eq!(wc_weights.len(), n / 2);

    let (first_half, second_half) = scratch[..n].split_at_mut(n / 2);
    for i in 0..n / 2 {
        first_half[i] = data[i * 2];
        second_half[i] = data[2 * i + 1];
    }

    d.call(
        #[inline(always)]
        |_| idct_impl_inner(d, first_half, data),
    );

    for i in (1..n / 2).rev() {
        second_half[i] += second_half[i - 1];
    }
    second_half[0] *= D::F32Vec::splat(d, SQRT_2);

    d.call(
        #[inline(always)]
        |_| idct_impl_inner(d, second_half, data),
    );

    for i in 0..n / 2 {
        let mul = D::F32Vec::splat(d, wc_weights[i]);
        data[i] = second_half[i].mul_add(mul, first_half[i]);
        data[n - i - 1] = second_half[i].neg_mul_add(mul, first_half[i]);
    }
}

#[inline(always)]
fn do_idct<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    let n = storage.len();
    assert!((n - 1) * stride < data.len());
    for i in 0..n {
        storage[i] = D::F32Vec::load_array(d, &data[i * stride]);
    }
    d.call(
        #[inline(always)]
        |d| idct_impl_inner(d, storage, scratch),
    );
    for i in 0..n {
        storage[i].store_array(&mut data[i * stride]);
    }
}

#[inline(always)]
fn do_idct_rowblock<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    let n = storage.len();
    assert!(n.is_multiple_of(D::F32Vec::LEN));
    assert!(data.len() >= n);

    let row_stride = n / D::F32Vec::LEN;
    for i in 0..n {
        storage[i] = D::F32Vec::load_array(
            d,
            &data[row_stride * (i % D::F32Vec::LEN) + (i / D::F32Vec::LEN)],
        );
    }
    d.call(
        #[inline(always)]
        |d| idct_impl_inner(d, storage, scratch),
    );
    for i in 0..n {
        storage[i].store_array(&mut data[row_stride * (i % D::F32Vec::LEN) + (i / D::F32Vec::LEN)]);
    }
}

#[inline(always)]
fn do_idct_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    let n = storage.len();
    assert!(n.is_multiple_of(D::F32Vec::LEN));
    assert!(data.len() >= n);

    let row_stride = n / (2 * D::F32Vec::LEN);
    for i in 0..n / 2 {
        storage[i] = D::F32Vec::load_array(d, &data[row_stride * 2 * i]);
        storage[i + n / 2] = D::F32Vec::load_array(d, &data[row_stride * (2 * i + 1)]);
    }
    d.call(
        #[inline(always)]
        |d| idct_impl_inner(d, storage, scratch),
    );
    for i in 0..n {
        storage[i].store_array(&mut data[row_stride * i]);
    }
}

#[inline(always)]
fn idct2d_square<D: SimdDescriptor>(
    d: D,
    data: &mut [f32],
    n: usize,
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    let data = D::F32Vec::make_array_slice_mut(data);
    let chunks = n / D::F32Vec::LEN;
    // Step 1: do column-DCTs on the first K columns.
    for i in 0..chunks {
        d.call(
            #[inline(always)]
            |_| do_idct(d, &mut data[i..], chunks, &mut storage[..n], scratch),
        );
    }
    // Step 2: do column-DCTs on groups of K columns, transposing KxK blocks and
    // swapping them in their final place as we do so.
    for i in 0..chunks {
        D::F32Vec::transpose_square(d, &mut data[i * n + i..], chunks);
        for j in i + 1..chunks {
            D::F32Vec::transpose_square(d, &mut data[j * n + i..], chunks);
            D::F32Vec::transpose_square(d, &mut data[i * n + j..], chunks);
            for k in 0..D::F32Vec::LEN {
                data.swap(i * n + j + k * chunks, j * n + i + k * chunks);
            }
        }
        d.call(
            #[inline(always)]
            |_| do_idct(d, &mut data[i..], chunks, &mut storage[..n], scratch),
        );
    }
}

#[inline(always)]
fn idct2d_wide<D: SimdDescriptor>(
    d: D,
    data: &mut [f32],
    c: usize,
    r: usize,
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    assert!(r < c);
    let data = D::F32Vec::make_array_slice_mut(data);
    let column_chunks = c / D::F32Vec::LEN;
    let row_chunks = r / D::F32Vec::LEN;
    // Step 1: do rowblock-DCTs on the first K rows, transposing KxK blocks first.
    for i in 0..row_chunks {
        for j in 0..column_chunks {
            D::F32Vec::transpose_square(d, &mut data[i * c + j..], column_chunks);
        }
        d.call(
            #[inline(always)]
            |_| do_idct_rowblock(d, &mut data[i * c..], &mut storage[..c], scratch),
        );
    }
    // Step 2: do column-DCTs on groups of K columns, transposing KxK blocks back.
    for i in 0..column_chunks {
        for j in 0..row_chunks {
            D::F32Vec::transpose_square(d, &mut data[j * c + i..], column_chunks);
        }
        d.call(
            #[inline(always)]
            |_| do_idct(d, &mut data[i..], column_chunks, &mut storage[..r], scratch),
        );
    }
}

#[inline(always)]
fn idct2d_thin<D: SimdDescriptor>(
    d: D,
    data: &mut [f32],
    c: usize,
    r: usize,
    storage: &mut [D::F32Vec],
    scratch: &mut [D::F32Vec],
) {
    assert!(r > c);
    let data = D::F32Vec::make_array_slice_mut(data);
    let column_chunks = c / D::F32Vec::LEN;
    let row_chunks = r / D::F32Vec::LEN;
    // Note: input is transposed, so in the beginning it has ROWS *columns* and COLS *rows*.
    // Step 1: do column-DCTs on columns.
    for i in 0..row_chunks {
        d.call(
            #[inline(always)]
            |_| do_idct(d, &mut data[i..], row_chunks, &mut storage[..c], scratch),
        );
    }
    // Step 2: Incrementally transpose each square sub-block of the matrix, then do a column-IDCT which also completes the transpose.
    for i in 0..column_chunks {
        let tr_block = |data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray], i, j, l| {
            D::F32Vec::transpose_square(d, &mut data[i * r + j + l * column_chunks..], row_chunks)
        };

        (0..2).for_each(|l| tr_block(data, i, i, l));
        for j in i + 1..column_chunks {
            (0..2).for_each(|l| tr_block(data, i, j, l));
            (0..2).for_each(|l| tr_block(data, j, i, l));
            for l in 0..2 {
                for k in 0..D::F32Vec::LEN {
                    data.swap(
                        i * r + j + k * row_chunks + l * column_chunks,
                        j * r + i + k * row_chunks + l * column_chunks,
                    );
                }
            }
        }
        d.call(
            #[inline(always)]
            |_| do_idct_trh(d, &mut data[i..], &mut storage[..r], scratch),
        );
    }
}

macro_rules! make_idct2d {
    ($name: ident, $h: literal, $w: literal) => {
        pub fn $name<D: SimdDescriptor>(d: D, data: &mut [f32]) {
            const L: usize = if $w < $h { $h } else { $w };
            let mut storage = [D::F32Vec::zero(d); L];
            let mut scratch = [D::F32Vec::zero(d); L];
            if $w == $h {
                return d.call(
                    #[inline(always)]
                    |_| idct2d_square(d, data, $w, &mut storage, &mut scratch),
                );
            }
            if $w > $h {
                return d.call(
                    #[inline(always)]
                    |_| idct2d_wide(d, data, $w, $h, &mut storage, &mut scratch),
                );
            }
            return d.call(
                #[inline(always)]
                |_| idct2d_thin(d, data, $w, $h, &mut storage, &mut scratch),
            );
        }
    };
}

make_idct2d!(idct2d_32_64, 32, 64);
make_idct2d!(idct2d_64_32, 64, 32);
make_idct2d!(idct2d_64_64, 64, 64);
make_idct2d!(idct2d_64_128, 64, 128);
make_idct2d!(idct2d_128_64, 128, 64);
make_idct2d!(idct2d_128_128, 128, 128);
make_idct2d!(idct2d_128_256, 128, 256);
make_idct2d!(idct2d_256_128, 256, 128);
make_idct2d!(idct2d_256_256, 256, 256);

#[cfg(test)]
#[inline(always)]
pub fn do_idct_64<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    let mut storage = [D::F32Vec::zero(d); 64];
    let mut scratch = [D::F32Vec::zero(d); 64];
    d.call(
        #[inline(always)]
        |_| {
            do_idct(d, data, stride, &mut storage, &mut scratch);
        },
    );
}

#[cfg(test)]
#[inline(always)]
pub fn do_idct_128<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    let mut storage = [D::F32Vec::zero(d); 128];
    let mut scratch = [D::F32Vec::zero(d); 128];
    d.call(
        #[inline(always)]
        |_| {
            do_idct(d, data, stride, &mut storage, &mut scratch);
        },
    );
}

#[cfg(test)]
#[inline(always)]
pub fn do_idct_256<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    let mut storage = [D::F32Vec::zero(d); 256];
    let mut scratch = [D::F32Vec::zero(d); 256];
    d.call(
        #[inline(always)]
        |_| {
            do_idct(d, data, stride, &mut storage, &mut scratch);
        },
    );
}
