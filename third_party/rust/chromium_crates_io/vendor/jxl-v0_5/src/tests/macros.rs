// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

macro_rules! declare_test_file {
    ($ident:ident, $path:expr) => {
        declare_test_file!($ident, $path, checkpoints: &[]);
    };
    ($ident:ident, $path:expr, checkpoints: $checkpoints:expr) => {
        paste::paste! {
            #[test]
            fn [<test_decode_test_file_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                let file = std::fs::read(&path).unwrap();
                crate::tests::decode::decode(&file).unwrap();
            }

            #[test]
            fn [<test_decode_test_file_chunks_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                let file = std::fs::read(&path).unwrap();
                crate::tests::decode::decode_internal(&file, 1, false, false, None, None).unwrap();
            }

            #[test]
            fn [<test_scan_test_file_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                let file = std::fs::read(&path).unwrap();
                crate::tests::decode::scan_frames_with_decoder(&file, usize::MAX);
            }

            #[test]
            fn [<test_scan_test_file_chunks_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                let file = std::fs::read(&path).unwrap();
                crate::tests::decode::scan_frames_with_decoder(&file, 1);
            }

            #[test]
            fn [<test_compare_pipelines_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                let file = std::fs::read(&path).unwrap();
                let simple_frames = crate::tests::decode::decode_internal(&file, usize::MAX, true, false, None, None).unwrap().1;
                let frames = crate::tests::decode::decode(&file).unwrap().1;
                assert_eq!(frames.len(), simple_frames.len());
                for (fc, (f, sf)) in frames.into_iter().zip(simple_frames).enumerate() {
                    crate::tests::decode::compare_frames(&path, fc, &f, &sf);
                }
            }

            #[test]
            fn [<test_compare_incremental_ $ident>]() {
                let path = std::path::Path::new("resources/test/").join($path);
                crate::tests::compare_incremental::run(&path, $checkpoints);
            }
        }
    };
}

/// Asserts that two values (scalars or slices) are close to each other.
///
/// # Usage
/// - `assert_close!(left, right, max_abs_error)` (for scalars with absolute error threshold)
/// - `assert_close!(left, right, max_abs_error, rel: max_rel_error)` (for scalars with absolute and relative error thresholds)
/// - `assert_close!(all, left, right, max_abs_error)` (for slices/arrays/iterables with element-wise absolute error threshold)
/// - `assert_close!(all, left, right, max_abs_error, rel: max_rel_error)` (for slices/arrays/iterables with element-wise absolute and relative error thresholds)
/// - Optional custom formatting messages can be appended to any form.
macro_rules! assert_close {
    // 1. Vector with rel
    (all, $left:expr, $right:expr, $max_abs:expr, rel: $max_rel:expr $(, $fmt:expr $(, $arg:expr)* )? $(,)? ) => {{
        let left = $left;
        let right = $right;
        let max_abs = $max_abs;
        let max_rel = $max_rel;
        let left_ref = left;
        let right_ref = right;
        if left_ref.len() != right_ref.len() {
            #[allow(unused_mut)]
            let mut msg = format!(
                "assertion failed: `({} ≈ {})`\n left.len(): `{}`,\n right.len(): `{}`",
                stringify!($left),
                stringify!($right),
                left_ref.len(),
                right_ref.len()
            );
            $(
                msg.push('\n');
                msg.push_str(&format!($fmt $(, $arg)*));
            )?
            panic!("{}", msg);
        }

        let mut mismatch_count = 0;
        let mut first_mismatch = None;
        let mut worst_abs_mismatch = None;
        let mut worst_rel_mismatch = None;

        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        for (idx, (left_val, right_val)) in left_ref
            .iter()
            .copied()
            .zip(right_ref.iter().copied())
            .enumerate()
        {
            let left_f64 = left_val as f64;
            let right_f64 = right_val as f64;
            let max_abs_f64 = max_abs as f64;
            let max_rel_f64 = max_rel as f64;

            let abs_err = (left_f64 - right_f64).abs();
            let is_abs_err = !(abs_err <= max_abs_f64);
            let rel_err = 2.0 * abs_err / (left_f64.abs() + right_f64.abs() + 1e-16);
            let is_rel_err = !(rel_err <= max_rel_f64);

            if is_abs_err || is_rel_err {
                mismatch_count += 1;

                if first_mismatch.is_none() {
                    first_mismatch = Some((idx, left_val, right_val, abs_err, rel_err));
                }

                let update_worst_abs = match worst_abs_mismatch {
                    None => true,
                    Some((_, _, _, max_err)) => !(abs_err < max_err),
                };
                if update_worst_abs {
                    worst_abs_mismatch = Some((idx, left_val, right_val, abs_err));
                }

                let update_worst_rel = match worst_rel_mismatch {
                    None => true,
                    Some((_, _, _, max_err)) => !(rel_err < max_err),
                };
                if update_worst_rel {
                    worst_rel_mismatch = Some((idx, left_val, right_val, rel_err));
                }
            }
        }

        if mismatch_count > 0 {
            let first = first_mismatch.unwrap();
            let worst_abs = worst_abs_mismatch.unwrap();
            let worst_rel = worst_rel_mismatch.unwrap();

            #[allow(unused_mut)]
            let mut msg = format!(
                "assertion failed: `({} ≈ {})`\n\
                 mismatch count: {} / {}\n\
                 first mismatch at index {}:\n  \
                 left: `{:?}`\n  \
                 right: `{:?}`\n  \
                 abs_error: `{}` (threshold: `{:?}`)\n  \
                 rel_error: `{}` (threshold: `{:?}`)\n\
                 worst absolute error mismatch at index {}:\n  \
                 left: `{:?}`\n  \
                 right: `{:?}`\n  \
                 abs_error: `{}`\n\
                 worst relative error mismatch at index {}:\n  \
                 left: `{:?}`\n  \
                 right: `{:?}`\n  \
                 rel_error: `{}`",
                stringify!($left),
                stringify!($right),
                mismatch_count,
                left_ref.len(),
                first.0, first.1, first.2, first.3, max_abs, first.4, max_rel,
                worst_abs.0, worst_abs.1, worst_abs.2, worst_abs.3,
                worst_rel.0, worst_rel.1, worst_rel.2, worst_rel.3
            );
            $(
                msg.push('\n');
                msg.push_str(&format!($fmt $(, $arg)*));
            )?
            panic!("{}", msg);
        }
    }};

    // 2. Vector without rel
    (all, $left:expr, $right:expr, $max_abs:expr $(,)? ) => {
        assert_close!(all, $left, $right, $max_abs, rel: f64::INFINITY)
    };
    (all, $left:expr, $right:expr, $max_abs:expr, $fmt:expr $(, $arg:expr)* $(,)? ) => {
        assert_close!(all, $left, $right, $max_abs, rel: f64::INFINITY, $fmt $(, $arg)* )
    };

    // 3. Scalar with rel:
    ($left:expr, $right:expr, $max_abs:expr, rel: $max_rel:expr $(, $fmt:expr $(, $arg:expr)* )? $(,)? ) => {{
        let left = $left;
        let right = $right;
        let max_abs = $max_abs;
        let max_rel = $max_rel;

        let left_f64 = left as f64;
        let right_f64 = right as f64;
        let max_abs_f64 = max_abs as f64;
        let max_rel_f64 = max_rel as f64;

        let abs_err = (left_f64 - right_f64).abs();
        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        let is_abs_err = !(abs_err <= max_abs_f64);
        let rel_err = 2.0 * abs_err / (left_f64.abs() + right_f64.abs() + 1e-16);
        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        let is_rel_err = !(rel_err <= max_rel_f64);

        if is_abs_err || is_rel_err {
            #[allow(unused_mut)]
            let mut msg = format!(
                "assertion failed: `({} ≈ {})`\n  left: `{:?}`,\n right: `{:?}`,\n max_abs_error: `{:?}`,\n max_rel_error: `{:?}`",
                stringify!($left),
                stringify!($right),
                left, right, max_abs, max_rel
            );
            $(
                msg.push('\n');
                msg.push_str(&format!($fmt $(, $arg)*));
            )?
            panic!("{}", msg);
        }
    }};

    // 4. Scalar without rel:
    ($left:expr, $right:expr, $max_abs:expr $(,)? ) => {
        assert_close!($left, $right, $max_abs, rel: f64::INFINITY)
    };
    ($left:expr, $right:expr, $max_abs:expr, $fmt:expr $(, $arg:expr)* $(,)? ) => {
        assert_close!($left, $right, $max_abs, rel: f64::INFINITY, $fmt $(, $arg)* )
    };
}

macro_rules! assert_image_eq {
    ($left:expr, $right:expr $(, $fmt:expr $(, $arg:expr)* )? $(,)? ) => {{
        let left = $left;
        let right = $right;

        let left_size = left.size();
        let right_size = right.size();
        if left_size != right_size {
            #[allow(unused_mut)]
            let mut msg = format!(
                "assertion failed: `({} == {})`\n left.size(): `{:?}`,\n right.size(): `{:?}`",
                stringify!($left),
                stringify!($right),
                left_size,
                right_size
            );
            $(
                msg.push('\n');
                msg.push_str(&format!($fmt $(, $arg)*));
            )?
            panic!("{}", msg);
        }

        let mut mismatch_count = 0;
        let mut first_mismatch = None;

        for y in 0..left_size.1 {
            let row_l = left.row(y);
            let row_r = right.row(y);

            for (x, (left_val, right_val)) in row_l
                .iter()
                .copied()
                .zip(row_r.iter().copied())
                .enumerate()
            {
                if !(left_val == right_val) {
                    mismatch_count += 1;

                    if first_mismatch.is_none() {
                        first_mismatch = Some((x, y, left_val, right_val));
                    }
                }
            }
        }

        if mismatch_count > 0 {
            let first = first_mismatch.unwrap();

            #[allow(unused_mut)]
            let mut msg = format!(
                "assertion failed: `({} == {})`\n\
                 mismatch count: {} / {}\n\
                 first mismatch at ({}, {}):\n  \
                 left: `{:?}`\n  \
                 right: `{:?}`",
                stringify!($left),
                stringify!($right),
                mismatch_count,
                left_size.0 * left_size.1,
                first.0, first.1, first.2, first.3
            );
            $(
                msg.push('\n');
                msg.push_str(&format!($fmt $(, $arg)*));
            )?
            panic!("{}", msg);
        }
    }};
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_with_floats() {
        assert_close!(1.0000001f64, 1.0000002, 0.000001);
        assert_close!(1.0, 1.1, 0.2);
    }

    #[test]
    fn test_with_integers() {
        assert_close!(100, 101, 2);
        assert_close!(777u32, 770, 7);
        assert_close!(500i64, 498, 3);
    }

    #[test]
    #[should_panic]
    fn test_panic_float() {
        assert_close!(1.0, 1.2, 0.1);
    }

    #[test]
    #[should_panic]
    fn test_panic_integer() {
        assert_close!(100, 105, 2);
    }

    #[test]
    #[should_panic]
    fn test_nan_comparison() {
        assert_close!(f64::NAN, f64::NAN, 0.1);
    }

    #[test]
    #[should_panic]
    fn test_nan_tolerance() {
        assert_close!(1.0, 1.0, f64::NAN);
    }

    #[test]
    fn test_infinity_tolerance() {
        assert_close!(1.0, 1.0, f64::INFINITY);
    }

    #[test]
    #[should_panic]
    fn test_nan_comparison_with_infinity_tolerance() {
        assert_close!(f32::NAN, f32::NAN, f32::INFINITY);
    }

    #[test]
    #[should_panic]
    fn test_infinity_comparison_with_infinity_tolerance() {
        assert_close!(f32::INFINITY, f32::INFINITY, f32::INFINITY);
    }

    #[test]
    fn test_vectors_abs() {
        assert_close!(all, &[1.0, 2.0], &[1.0001, 1.9999], 0.001);
    }

    #[test]
    fn test_vectors_rel() {
        assert_close!(all, &[100.0, 200.0], &[100.1, 199.9], 1.0, rel: 0.01);
    }

    #[test]
    fn test_images_eq() {
        use crate::image::Image;
        let mut img1 = Image::new((2, 2)).unwrap();
        let mut img2 = Image::new((2, 2)).unwrap();
        img1.row_mut(0).copy_from_slice(&[1.0, 2.0]);
        img1.row_mut(1).copy_from_slice(&[3.0, 4.0]);
        img2.row_mut(0).copy_from_slice(&[1.0, 2.0]);
        img2.row_mut(1).copy_from_slice(&[3.0, 4.0]);
        assert_image_eq!(img1, img2);
    }
}
