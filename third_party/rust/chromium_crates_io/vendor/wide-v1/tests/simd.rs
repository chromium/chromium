use core::ops::{BitOr, Neg, Not};
use std::{convert::identity, iter::once};

use bytemuck::cast;
use wide::{
  AlignTo, ShuffleExt, f32x4, f32x8, f32x16, f64x2, f64x4, f64x8, i8x16, i8x32,
  i16x8, i16x16, i16x32, i32x4, i32x8, i32x16, i64x2, i64x4, i64x8, u8x16,
  u8x32, u16x8, u16x16, u16x32, u32x4, u32x8, u32x16, u64x2, u64x4, u64x8,
};

use crate::utils::{for_simd_types, random_iter, simd_chunks};

#[test]
fn test_size() {
  assert_eq!(size_of::<f32x4>(), 16);
  assert_eq!(size_of::<f32x8>(), 32);
  assert_eq!(size_of::<f32x16>(), 64);
  assert_eq!(size_of::<f64x2>(), 16);
  assert_eq!(size_of::<f64x4>(), 32);
  assert_eq!(size_of::<f64x8>(), 64);
  assert_eq!(size_of::<i8x16>(), 16);
  assert_eq!(size_of::<i8x32>(), 32);
  assert_eq!(size_of::<i16x8>(), 16);
  assert_eq!(size_of::<i16x16>(), 32);
  assert_eq!(size_of::<i16x32>(), 64);
  assert_eq!(size_of::<i32x4>(), 16);
  assert_eq!(size_of::<i32x8>(), 32);
  assert_eq!(size_of::<i32x16>(), 64);
  assert_eq!(size_of::<i64x2>(), 16);
  assert_eq!(size_of::<i64x4>(), 32);
  assert_eq!(size_of::<i64x8>(), 64);
  assert_eq!(size_of::<u8x16>(), 16);
  assert_eq!(size_of::<u8x32>(), 32);
  assert_eq!(size_of::<u16x8>(), 16);
  assert_eq!(size_of::<u16x16>(), 32);
  assert_eq!(size_of::<u16x32>(), 64);
  assert_eq!(size_of::<u32x4>(), 16);
  assert_eq!(size_of::<u32x8>(), 32);
  assert_eq!(size_of::<u32x16>(), 64);
  assert_eq!(size_of::<u64x2>(), 16);
  assert_eq!(size_of::<u64x4>(), 32);
  assert_eq!(size_of::<u64x8>(), 64);
}

#[test]
fn test_alignment() {
  for_simd_types!(|T, N| {
    assert_eq!(align_of::<Simd>(), size_of::<Simd>());
  });
}

#[test]
fn test_debug() {
  for_simd_types!(|T, N| {
    let value = Simd::new(std::array::from_fn(|i| i as T));

    assert_eq!(
      format!("{value:?}"),
      format!("{:?}", value.to_array()).replace("[", "(").replace("]", ")")
    );
  });
  for_simd_types!(|T: Float, N| {
    let value = Simd::new(std::array::from_fn(|i| i as T));

    assert_eq!(
      format!("{value:.3?}"),
      format!("{:.3?}", value.to_array()).replace("[", "(").replace("]", ")")
    );
  });
}

#[test]
fn test_neg() {
  for_simd_types!(|T: Float, N| {
    for value in simd_chunks!([0.0, -0.0, 1.0, -2.0, -3.1, 5.3, T::INFINITY])
      .chain(random_iter())
    {
      let expected = Simd::new(value.map(T::neg));
      let actual = -Simd::new(value);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n   value: {value:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in
      simd_chunks!([0, 1, -2, -30, 50, T::MAX, T::MIN]).chain(random_iter())
    {
      let expected = Simd::new(value.map(T::wrapping_neg));
      let actual = -Simd::new(value);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_not() {
  for_simd_types!(|T: Float, N| {
    for value in simd_chunks!([0.0, -0.0, 1.0, -2.0, -3.1, 5.3, T::INFINITY])
      .chain(random_iter())
    {
      let expected = Simd::new(value.map(|x| T::from_bits(!x.to_bits())));
      let actual = !Simd::new(value);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for value in
      simd_chunks!([0, 1, 50, 113, T::MAX, T::MIN]).chain(random_iter())
    {
      let expected = Simd::new(value.map(T::not));
      let actual = !Simd::new(value);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_add() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] + right[i]));
      let actual = Simd::new(left) + Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [0, 1, 2, 5, 100, T::MAX, T::MAX - 1],
      [5, 0, 5, 11, 3, 2, 2],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_add(right[i])));
      let actual = Simd::new(left) + Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [-3, 5, 100, -20, T::MIN, T::MIN + 1, T::MAX, T::MIN],
      [3, -6, -30, 11, -2, -2, -2, 2],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_add(right[i])));
      let actual = Simd::new(left) + Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_add_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let right = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left[i] + right));
      let actual = Simd::new(left) + right;

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([0, 1, 2, 5, 100, T::MAX, T::MAX - 1]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_add(right)));
      let actual = Simd::new(left) + right;

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_scalar_add() {
  for_simd_types!(|T: Float, N| {
    for right in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let left = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left + right[i]));
      let actual = left + Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for right in simd_chunks!([0, 1, 2, 5, 100, T::MAX, T::MAX - 1]) {
      let left = 5 as T;
      let expected =
        Simd::new(std::array::from_fn(|i| left.wrapping_add(right[i])));
      let actual = left + Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_sub() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] - right[i]));
      let actual = Simd::new(left) - Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [0, 1, 2, 5, 100, T::MAX, T::MAX - 1],
      [5, 0, 5, 11, 3, 2, 2],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_sub(right[i])));
      let actual = Simd::new(left) - Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [-3, 5, 100, -20, T::MIN, T::MIN + 1, T::MAX, T::MIN],
      [-3, 6, 30, -11, 2, 2, 2, -2],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_sub(right[i])));
      let actual = Simd::new(left) - Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_sub_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let right = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left[i] - right));
      let actual = Simd::new(left) - right;

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([0, 1, 2, 5, 100, T::MAX, T::MAX - 1]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_sub(right)));
      let actual = Simd::new(left) - right;

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_scalar_sub() {
  for_simd_types!(|T: Float, N| {
    for right in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let left = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left - right[i]));
      let actual = left - Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for right in simd_chunks!([0, 1, 2, 5, 100, T::MAX, T::MAX - 1]) {
      let left = 5 as T;
      let expected =
        Simd::new(std::array::from_fn(|i| left.wrapping_sub(right[i])));
      let actual = left - Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_mul() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] * right[i]));
      let actual = Simd::new(left) * Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        T::MIN + 1,
        T::MIN,
        2,
        3,
        4,
        5,
        T::MAX - 1,
        T::MAX,
        T::MAX,
        T::MAX / 2,
        T::MIN / 2,
      ],
      [17, -18, 1, 1, -1, -2, -6, 3, 3, 2, 1, 3, 3],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_mul(right[i])));
      let actual = Simd::new(left) * Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        3,
        4,
        5,
        T::MAX / 4,
        T::MAX / 3,
        T::MAX / 2,
        T::MAX - 1,
        T::MAX,
        T::MAX,
        3,
        4,
        3,
        2,
        2,
        1,
      ],
      [
        17,
        18,
        9,
        1,
        0,
        3,
        4,
        3,
        2,
        2,
        1,
        T::MAX / 4,
        T::MAX / 3,
        T::MAX / 2,
        T::MAX - 1,
        T::MAX,
        T::MAX,
      ],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_mul(right[i])));
      let actual = Simd::new(left) * Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_mul_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let right = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left[i] * right));
      let actual = Simd::new(left) * right;

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([
      1,
      2,
      T::MIN + 1,
      T::MIN,
      2,
      3,
      4,
      5,
      T::MAX - 1,
      T::MAX,
      T::MAX,
      T::MAX / 2,
      T::MIN / 2,
    ]) {
      let right = 3;
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_mul(right)));
      let actual = Simd::new(left) * right;

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_scalar_mul() {
  for_simd_types!(|T: Float, N| {
    for right in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let left = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left * right[i]));
      let actual = left * Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for right in simd_chunks!([
      1,
      2,
      T::MIN + 1,
      T::MIN,
      2,
      3,
      4,
      5,
      T::MAX - 1,
      T::MAX,
      T::MAX,
      T::MAX / 2,
      T::MIN / 2,
    ]) {
      let left = 3 as T;
      let expected =
        Simd::new(std::array::from_fn(|i| left.wrapping_mul(right[i])));
      let actual = left * Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_div() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] / right[i]));
      let actual = Simd::new(left) / Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, mut right] in simd_chunks!(
      [11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1],
      [2, 5, 5, 8, 10, 5, 2, 10],
    )
    .chain(random_iter())
    {
      for right in &mut right {
        if *right == 0 {
          *right = 3;
        }
      }

      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_div(right[i])));
      let actual = Simd::new(left) / Simd::new(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, mut right] in simd_chunks!(
      [11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1],
      [-2, -5, 3, -6, -2, -1, -1, -1],
    )
    .chain(random_iter())
    {
      for right in &mut right {
        if *right == 0 {
          *right = 3;
        }
      }

      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_div(right[i])));
      let actual = Simd::new(left) / Simd::new(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_div_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let right = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left[i] / right));
      let actual = Simd::new(left) / right;

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1])
      .chain(random_iter())
    {
      for right in [2, 5, 5, 8, 10, 5, 2, 10] {
        let expected =
          Simd::new(std::array::from_fn(|i| left[i].wrapping_div(right)));
        let actual = Simd::new(left) / right;

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in simd_chunks!([11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1])
      .chain(random_iter())
    {
      for right in [-2, -5, 3, -6, -2, -1, -1, -1] {
        let expected =
          Simd::new(std::array::from_fn(|i| left[i].wrapping_div(right)));
        let actual = Simd::new(left) / right;

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

#[test]
fn test_scalar_div() {
  for_simd_types!(|T: Float, N| {
    for right in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let left = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left / right[i]));
      let actual = left / Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in [2, 5, 5, 8, 10, 5, 2, 10] {
      let _: T = left;
      for mut right in
        simd_chunks!([11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1])
          .chain(random_iter())
      {
        for right in &mut right {
          if *right == 0 {
            *right = 3;
          }
        }

        let expected =
          Simd::new(std::array::from_fn(|i| left.wrapping_div(right[i])));
        let actual = left / Simd::new(right);

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in [-2, -5, 3, -6, -2, -1, -1, -1] {
      let _: T = left;
      for mut right in
        simd_chunks!([11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1])
          .chain(random_iter())
      {
        for right in &mut right {
          if *right == 0 {
            *right = 3;
          }
        }

        let expected =
          Simd::new(std::array::from_fn(|i| left.wrapping_div(right[i])));
        let actual = left / Simd::new(right);

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

#[test]
fn test_rem() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] % right[i]));
      let actual = Simd::new(left) % Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, mut right] in simd_chunks!(
      [11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1],
      [2, 5, 5, 8, 10, 5, 2, 10],
    )
    .chain(random_iter())
    {
      for right in &mut right {
        if *right == 0 {
          *right = 3;
        }
      }

      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_rem(right[i])));
      let actual = Simd::new(left) % Simd::new(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, mut right] in simd_chunks!(
      [11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1],
      [-2, -5, 3, -6, -2, -1, -1, -1],
    )
    .chain(random_iter())
    {
      for right in &mut right {
        if *right == 0 {
          *right = 3;
        }
      }

      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_rem(right[i])));
      let actual = Simd::new(left) % Simd::new(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_rem_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let right = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left[i] % right));
      let actual = Simd::new(left) % right;

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1])
      .chain(random_iter())
    {
      for right in [2, 5, 5, 8, 10, 5, 2, 10] {
        let expected =
          Simd::new(std::array::from_fn(|i| left[i].wrapping_rem(right)));
        let actual = Simd::new(left) % right;

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in simd_chunks!([11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1])
      .chain(random_iter())
    {
      for right in [-2, -5, 3, -6, -2, -1, -1, -1] {
        let expected =
          Simd::new(std::array::from_fn(|i| left[i].wrapping_rem(right)));
        let actual = Simd::new(left) % right;

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

#[test]
fn test_scalar_rem() {
  for_simd_types!(|T: Float, N| {
    for right in simd_chunks!([0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1]) {
      let left = 5.3;
      let expected = Simd::new(std::array::from_fn(|i| left % right[i]));
      let actual = left % Simd::new(right);

      assert_eq!(
        actual ^ expected,
        Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}"
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in [2, 5, 5, 8, 10, 5, 2, 10] {
      let _: T = left;
      for mut right in
        simd_chunks!([11, 15, 2, 3, T::MAX, 0, T::MAX, T::MAX - 1])
          .chain(random_iter())
      {
        for right in &mut right {
          if *right == 0 {
            *right = 3;
          }
        }

        let expected =
          Simd::new(std::array::from_fn(|i| left.wrapping_rem(right[i])));
        let actual = left % Simd::new(right);

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in [-2, -5, 3, -6, -2, -1, -1, -1] {
      let _: T = left;
      for mut right in
        simd_chunks!([11, 15, -13, -16, T::MIN, 0, T::MIN, T::MIN + 1])
          .chain(random_iter())
      {
        for right in &mut right {
          if *right == 0 {
            *right = 3;
          }
        }

        let expected =
          Simd::new(std::array::from_fn(|i| left.wrapping_rem(right[i])));
        let actual = left % Simd::new(right);

        assert!(
          actual == expected,
          "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

#[test]
fn test_shl() {
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        T::MAX - 1,
        -123,
        -121,
        53,
        -60,
        -49,
        T::MAX / 2,
        T::MIN + 1,
        T::MIN / 2,
        -121,
        53,
      ],
      [1, 0, 3, 2, -1, 6, 100, 0, 4, 1, 3, -101, 123],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        left[i].wrapping_shl(right[i] as u32)
      }));
      let actual = Simd::new(left) << Simd::new(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        T::MAX - 1,
        T::MAX - 122,
        T::MAX - 120,
        53,
        T::MAX - 59,
        T::MAX - 48,
        T::MAX / 4,
        T::MAX / 2,
        T::MAX / 4,
        T::MAX - 120,
        53,
      ],
      [1, 0, 3, 2, T::MAX, 6, 100, 0, 4, 1, 3, T::MAX - 100, 123],
    )
    .chain(random_iter())
    {
      #[allow(clippy::unnecessary_cast)]
      let expected = Simd::new(std::array::from_fn(|i| {
        left[i].wrapping_shl(right[i] as u32)
      }));
      let actual = Simd::new(left) << Simd::new(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_shl_scalar() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!([
      1,
      2,
      T::MAX - 1,
      -123,
      -121,
      T::MAX / 2,
      T::MIN / 2,
      53,
      -60,
      -49,
      T::MAX / 2,
      T::MIN + 1,
      T::MIN / 2,
    ])
    .flat_map(|left| {
      [1, 0, 3, -2, -6, 100, 7, 8, 9, 15, 16, 31, 32, 33]
        .map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_shl(right as u32)));

      for actual in [
        Simd::new(left) << right as i8,
        Simd::new(left) << right as u8,
        Simd::new(left) << right as i16,
        Simd::new(left) << right as u16,
        Simd::new(left) << right,
        Simd::new(left) << right as u32,
        Simd::new(left) << right as i64,
        Simd::new(left) << right as u64,
        Simd::new(left) << right as i128,
        Simd::new(left) << right as u128,
        // `simd << isize` and `simd << usize` are missing.
      ] {
        assert!(
          actual == expected,
          "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for (left, right) in simd_chunks!([
      1,
      2,
      T::MAX - 1,
      T::MAX - 122,
      T::MAX - 120,
      T::MAX / 2,
      T::MAX / 4,
      53,
      T::MAX - 59,
      T::MAX - 48,
      T::MAX / 2,
      T::MIN + 1,
      T::MIN / 2,
    ])
    .flat_map(|left| {
      [1, 0, 3, -2, -6, 100, 7, 8, 9, 15, 16, 31, 32, 33]
        .map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_shl(right as u32)));

      for actual in [
        Simd::new(left) << right as i8,
        Simd::new(left) << right as u8,
        Simd::new(left) << right as i16,
        Simd::new(left) << right as u16,
        Simd::new(left) << right,
        Simd::new(left) << right as u32,
        Simd::new(left) << right as i64,
        Simd::new(left) << right as u64,
        Simd::new(left) << right as i128,
        Simd::new(left) << right as u128,
        // `simd << isize` and `simd << usize` are missing.
      ] {
        assert!(
          actual == expected,
          "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

// `scalar << simd` is missing.

#[test]
fn test_shr() {
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        T::MAX - 1,
        -123,
        -121,
        53,
        -60,
        -49,
        T::MAX / 2,
        T::MIN + 1,
        T::MIN / 2,
        -121,
        53,
      ],
      [1, 0, 3, 2, -1, 6, 100, 0, 4, 1, 3, -101, 123],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        left[i].wrapping_shr(right[i] as u32)
      }));
      let actual = Simd::new(left) >> Simd::new(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for [left, right] in simd_chunks!(
      [
        1,
        2,
        T::MAX - 1,
        T::MAX - 122,
        T::MAX - 120,
        53,
        T::MAX - 59,
        T::MAX - 48,
        T::MAX / 4,
        T::MAX / 2,
        T::MAX / 4,
        T::MAX - 120,
        53,
      ],
      [1, 0, 3, 2, T::MAX, 6, 100, 0, 4, 1, 3, T::MAX - 100, 123],
    )
    .chain(random_iter())
    {
      #[allow(clippy::unnecessary_cast)]
      let expected = Simd::new(std::array::from_fn(|i| {
        left[i].wrapping_shr(right[i] as u32)
      }));
      let actual = Simd::new(left) >> Simd::new(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_shr_scalar() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!([
      1,
      2,
      T::MAX - 1,
      -123,
      -121,
      T::MAX / 2,
      T::MIN / 2,
      53,
      -60,
      -49,
      T::MAX / 2,
      T::MIN + 1,
      T::MIN / 2,
    ])
    .flat_map(|left| {
      [1, 0, 3, -2, -6, 100, 7, 8, 9, 15, 16, 31, 32, 33]
        .map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_shr(right as u32)));

      for actual in [
        Simd::new(left) >> right as i8,
        Simd::new(left) >> right as u8,
        Simd::new(left) >> right as i16,
        Simd::new(left) >> right as u16,
        Simd::new(left) >> right,
        Simd::new(left) >> right as u32,
        Simd::new(left) >> right as i64,
        Simd::new(left) >> right as u64,
        Simd::new(left) >> right as i128,
        Simd::new(left) >> right as u128,
        // `simd >> isize` and `simd >> usize` are missing.
      ] {
        assert!(
          actual == expected,
          "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for (left, right) in simd_chunks!([
      1,
      2,
      T::MAX - 1,
      T::MAX - 122,
      T::MAX - 120,
      T::MAX / 2,
      T::MAX / 4,
      53,
      T::MAX - 59,
      T::MAX - 48,
      T::MAX / 2,
      T::MIN + 1,
      T::MIN / 2,
    ])
    .flat_map(|left| {
      [1, 0, 3, -2, -6, 100, 7, 8, 9, 15, 16, 31, 32, 33]
        .map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].wrapping_shr(right as u32)));

      for actual in [
        Simd::new(left) >> right as i8,
        Simd::new(left) >> right as u8,
        Simd::new(left) >> right as i16,
        Simd::new(left) >> right as u16,
        Simd::new(left) >> right,
        Simd::new(left) >> right as u32,
        Simd::new(left) >> right as i64,
        Simd::new(left) >> right as u64,
        Simd::new(left) >> right as i128,
        Simd::new(left) >> right as u128,
        // `simd >> isize` and `simd >> usize` are missing.
      ] {
        assert!(
          actual == expected,
          "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
        );
      }
    }
  });
}

// `scalar >> simd` is missing.

#[test]
fn test_bitand() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        T::from_bits(left[i].to_bits() & right[i].to_bits())
      }));
      let actual = Simd::new(left) & Simd::new(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [0, 1, 2, 30, 5, T::MAX - 5, T::MIN + 3],
      [5, 78, 3, 26, 30, 5, 78],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] & right[i]));
      let actual = Simd::new(left) & Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

// `simd & scalar` is missing.

// `scalar & simd` is missing.

#[test]
fn test_bitor() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        T::from_bits(left[i].to_bits() | right[i].to_bits())
      }));
      let actual = Simd::new(left) | Simd::new(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [0, 1, 2, 30, 5, T::MAX - 5, T::MIN + 3],
      [5, 78, 3, 26, 30, 5, 78],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] | right[i]));
      let actual = Simd::new(left) | Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

// `simd | scalar` is missing.

// `scalar | simd` is missing.

#[test]
fn test_bitxor() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        T::from_bits(left[i].to_bits() ^ right[i].to_bits())
      }));
      let actual = Simd::new(left) ^ Simd::new(right);

      assert!(
        (0..N).all(|i| actual.to_array()[i].to_bits()
          == expected.to_array()[i].to_bits()),
        "expected: {expected:?}\n  actual: {actual:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [0, 1, 2, 30, 5, T::MAX - 5, T::MIN + 3],
      [5, 78, 3, 26, 30, 5, 78],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| left[i] ^ right[i]));
      let actual = Simd::new(left) ^ Simd::new(right);

      assert_eq!(actual, expected);
    }
  });
}

// `simd ^ scalar` is missing.

// `scalar ^ simd` is missing.

#[test]
fn test_simd_eq() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] == right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_eq(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] == right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_eq(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] == right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_eq(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_eq_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] == right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_eq(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] == right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_eq(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] == right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_eq(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_ne() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] != right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_ne(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] != right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_ne(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] != right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_ne(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_ne_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] != right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_ne(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] != right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_ne(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] != right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_ne(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_lt() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] < right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_lt(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] < right[i] { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_lt(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] < right[i] { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_lt(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_lt_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] < right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_lt(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] < right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_lt(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] < right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_lt(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_gt() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] > right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_gt(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] > right[i] { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_gt(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] > right[i] { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_gt(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_gt_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] > right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_gt(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] > right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_gt(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] > right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_gt(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_le() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] <= right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_le(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] <= right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_le(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] <= right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_le(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_le_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] <= right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_le(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] <= right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_le(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] <= right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_le(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_ge() {
  for_simd_types!(|T: Float, N| {
    for [left, right] in simd_chunks!(
      [5.0, -0.0, 1.0, 2.0, 6.3, -30.2, 1e3, 20.2],
      [5.0, 0.0, 5.0, -3.1, 6.3, -30.2, 1e4, 20.1],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] >= right[i] { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_ge(Simd::new(right));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5, 5],
      [5, 5, 2, -5, -1, -7, 20, -6, 0, T::MIN + 5, 5, T::MAX - 5],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] >= right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_ge(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [5, 1, 5, T::MAX - 5, 20, T::MAX],
      [5, 5, 2, 20, T::MAX - 5, 0],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] >= right[i] { !0 } else { 0 }
      }));
      let actual = Simd::new(left).simd_ge(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_simd_ge_scalar() {
  for_simd_types!(|T: Float, N| {
    for left in simd_chunks!([5.0, -0.0, 1.0, 5.0, 6.3, -30.2, 1e3, 20.2]) {
      let right = 5.0;
      let expected = Simd::new(std::array::from_fn(|i| {
        if left[i] >= right { T::from_bits(!0) } else { 0.0 }
      }));
      let actual = Simd::new(left).simd_ge(right);

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for left in
      simd_chunks!([5, 1, 5, -5, -5, -1, -3, 20, T::MIN + 5, 0, T::MAX - 5])
    {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] >= right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_ge(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for left in simd_chunks!([5, 1, 5, T::MAX - 5, 20, T::MAX]) {
      let right = 5;
      let expected =
        Simd::new(std::array::from_fn(
          |i| {
            if left[i] >= right { !0 } else { 0 }
          },
        ));
      let actual = Simd::new(left).simd_ge(right);

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_bitselect() {
  for_simd_types!(|T: Float, N| {
    for [value, if_true, if_false] in simd_chunks!(
      [1.45, 1.1, -4.0, 11.0, -41.0, -17.0, 61.0, -1.5],
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        T::from_bits(
          if_true[i].to_bits() & value[i].to_bits()
            | if_false[i].to_bits() & !value[i].to_bits(),
        )
      }));
      let actual =
        Simd::new(value).bitselect(Simd::new(if_true), Simd::new(if_false));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n   value: {value:?}\n if_true: {if_true:?}\nif_false: {if_false:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [value, if_true, if_false] in simd_chunks!(
      [0, 0, !0, 0, !0, !0, 0, !0],
      [4, 6, 3, 20, T::MAX, 5, 123, 111],
      [5, 1, 4, 50, 1, T::MIN, 120, 112],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if_true[i] & value[i] | if_false[i] & !value[i]
      }));
      let actual =
        Simd::new(value).bitselect(Simd::new(if_true), Simd::new(if_false));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    mask: {value:?}\n if_true: {if_true:?}\nif_false: {if_false:?}",
      );
    }
  });
}

#[test]
fn test_select() {
  for_simd_types!(|T: Float, N| {
    for [mask, if_true, if_false] in simd_chunks!(
      [1.0, 1.0, -1.0, 1.0, -1.0, -1.0, 1.0, -1.0],
      [0.0, -0.0, 1.0, 2.0, -3.1, 5.3, 1e3, -20.1],
      [5.0, 0.0, 5.0, 3.1, 6.3, -30.2, 1e4, 53.2],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if mask[i].is_sign_negative() { if_true[i] } else { if_false[i] }
      }));
      let actual = Simd::new(mask)
        .is_sign_negative()
        .select(Simd::new(if_true), Simd::new(if_false));

      assert!(
        actual ^ expected == Simd::ZERO,
        "expected: {expected:?}\n  actual: {actual:?}\n    mask: {mask:?}\n if_true: {if_true:?}\nif_false: {if_false:?}",
      );
    }
  });
  for_simd_types!(|T: Integer, N| {
    for [mask, if_true, if_false] in simd_chunks!(
      [0, 0, !0, 0, !0, !0, 0, !0],
      [4, 6, 3, 20, T::MAX, 5, 123, 111],
      [5, 1, 4, 50, 1, T::MIN, 120, 112],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        if mask[i] > 0 { if_true[i] } else { if_false[i] }
      }));
      let actual = Simd::new(mask)
        .simd_gt(Simd::ZERO)
        .select(Simd::new(if_true), Simd::new(if_false));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    mask: {mask:?}\n if_true: {if_true:?}\nif_false: {if_false:?}",
      );
    }
  });
}

#[test]
fn test_to_bitmask() {
  for_simd_types!(|T: Float, N| {
    for value in
      simd_chunks!([0.0, -0.0, 1.0, -1.0, T::INFINITY, -5.3, T::NAN, 2.4])
        .chain(random_iter())
    {
      let expected = (0..N)
        .map(|i| if value[i].is_sign_negative() { 1 << i } else { 0 })
        .fold(0, u64::bitor);
      let actual = Simd::new(value).to_bitmask() as u64;

      assert!(
        actual == expected,
        "expected: {expected:0>32b}\n  actual: {actual:0>32b}\n   value: {value:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in
      simd_chunks!([0, T::MIN, T::MIN + 10, -5, 7, T::MAX, T::MIN, 9, -10])
        .chain(random_iter())
    {
      let expected = (0..N)
        .map(|i| if value[i].is_negative() { 1 << i } else { 0 })
        .fold(0, u64::bitor);
      #[allow(clippy::unnecessary_cast)]
      let actual = Simd::new(value).to_bitmask() as u64;

      assert!(
        actual == expected,
        "expected: {expected:0>32b}\n  actual: {actual:0>32b}\n   value: {value:?}",
      );
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in
      simd_chunks!([0, 10, 5, T::MAX, T::MAX / 2 + 5, 2, T::MAX / 2 + 10])
        .chain(random_iter())
    {
      let expected = (0..N)
        .map(|i| if value[i] > T::MAX >> 1 { 1 << i } else { 0 })
        .fold(0, u64::bitor);
      #[allow(clippy::unnecessary_cast)]
      let actual = Simd::new(value).to_bitmask() as u64;

      assert!(
        actual == expected,
        "expected: {expected:0>32b}\n  actual: {actual:0>32b}\n   value: {value:?}",
      );
    }
  });
}

#[test]
fn test_any() {
  for_simd_types!(|T: Float, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -0.0 } else { 0.0 }))
      .chain([[0.0 as T; N], [-0.0 as T; N]])
    {
      let expected = value.into_iter().any(|x| x.is_sign_negative());
      let actual = Simd::new(value).any();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -5 } else { 1 }))
      .chain([[2 as T; N], [-4 as T; N]])
    {
      let expected = value.into_iter().any(|x| x.is_negative());
      let actual = Simd::new(value).any();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { !0 - 5 } else { 1 }))
      .chain([[2 as T; N], [!0 - 6 as T; N]])
    {
      let expected = value.into_iter().any(|x| x.cast_signed().is_negative());
      let actual = Simd::new(value).any();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
}

#[test]
fn test_all() {
  for_simd_types!(|T: Float, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -0.0 } else { 0.0 }))
      .chain([[0.0 as T; N], [-0.0 as T; N]])
    {
      let expected = value.into_iter().all(|x| x.is_sign_negative());
      let actual = Simd::new(value).all();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -5 } else { 1 }))
      .chain([[2 as T; N], [-4 as T; N]])
    {
      let expected = value.into_iter().all(|x| x.is_negative());
      let actual = Simd::new(value).all();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { !0 - 5 } else { 1 }))
      .chain([[2 as T; N], [!0 - 6 as T; N]])
    {
      let expected = value.into_iter().all(|x| x.cast_signed().is_negative());
      let actual = Simd::new(value).all();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
}

#[test]
fn test_none() {
  for_simd_types!(|T: Float, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -0.0 } else { 0.0 }))
      .chain([[0.0 as T; N], [-0.0 as T; N]])
    {
      let expected = !value.into_iter().any(|x| x.is_sign_negative());
      let actual = Simd::new(value).none();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { -5 } else { 1 }))
      .chain([[2 as T; N], [-4 as T; N]])
    {
      let expected = !value.into_iter().any(|x| x.is_negative());
      let actual = Simd::new(value).none();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in (0..N)
      .map(|i| std::array::from_fn(|j| if i == j { !0 - 5 } else { 1 }))
      .chain([[2 as T; N], [!0 - 6 as T; N]])
    {
      let expected = !value.into_iter().any(|x| x.cast_signed().is_negative());
      let actual = Simd::new(value).none();

      assert_eq!(actual, expected, "\n   value: {value:?}");
    }
  });
}

#[test]
fn test_reduce_add() {
  for_simd_types!(|T: Float, N| {
    for value in simd_chunks!([
      0.2, 0.3, 0.5, 0.7, 1.1, 1.3, 1.7, 1.9, 2.3, 2.9, 3.1, 3.7, 4.1, 4.3,
      4.7, 5.3,
    ]) {
      let expected = value.into_iter().sum::<T>();
      let actual = Simd::new(value).reduce_add();

      assert!(
        (actual - expected).abs() <= expected.abs() * 1e-5,
        "expected: {expected:?}\n  actual: {actual:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([1, 2, 3, -4, 5, -6, 7, -9, -10, 20, -30, T::MAX])
    {
      let expected = value.into_iter().fold(0, T::wrapping_add);
      let actual = Simd::new(value).reduce_add();

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for value in simd_chunks!([1, 2, 3, 4, 5, 6, 7, 9, 10, 20, 30, 40, T::MAX])
    {
      let expected = value.into_iter().fold(0, T::wrapping_add);
      let actual = Simd::new(value).reduce_add();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_reduce_mul() {
  for_simd_types!(|T: Float, N| {
    for value in simd_chunks!([
      0.2, 0.3, 0.5, 0.7, 1.1, 1.3, 1.7, 1.9, 2.3, 2.9, 3.1, 3.7, 4.1, 4.3,
      4.7, 5.3,
    ]) {
      let expected = value.into_iter().product::<T>();
      let actual = Simd::new(value).reduce_mul();

      assert!(
        (actual - expected).abs() <= expected.abs() * 1e-5,
        "expected: {expected:?}\n  actual: {actual:?}",
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    // Have many `1` values so that not all cases overflow.
    for value in simd_chunks!([
      1,
      2,
      3,
      1,
      -1,
      -4,
      5,
      -1,
      1,
      -6,
      7,
      -9,
      -10,
      20,
      -30,
      T::MAX
    ]) {
      let expected = value.into_iter().fold(1, T::wrapping_mul);
      let actual = Simd::new(value).reduce_mul();

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    // Have many `1` values so that not all cases overflow.
    for value in simd_chunks!([
      1,
      2,
      1,
      3,
      1,
      1,
      4,
      1,
      5,
      1,
      6,
      1,
      1,
      7,
      9,
      1,
      10,
      20,
      30,
      40,
      T::MAX
    ])
    .chain(random_iter())
    {
      let expected = value.into_iter().fold(1, T::wrapping_mul);
      let actual = Simd::new(value).reduce_mul();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_shuffle() {
  for_simd_types!(|T, N| {
    // The values themselves do not matter here, as long as each lane is
    // different
    let simd = Simd::new(std::array::from_fn(|i| (i * 3 + 4) as T));

    for indices in
      shuffle_indices(1).map(|indices| indices.map(|index| index as Unsigned))
    {
      let indices = SimdUnsigned::new(indices);
      let zeroing = simd.shuffle_zeroing(indices);
      let wrapping = simd.shuffle_wrapping(indices);
      let actual = simd.shuffle(indices);

      let is_8bit = size_of::<T>() == 1;
      let is_8bitx16 = is_8bit && N == 16;

      if is_8bitx16 {
        // `i/u8x16` make the most guarantees

        let high_bit_set =
          cast::<SimdSigned, Simd>(indices.cast_signed().is_negative());

        assert!(
          high_bit_set
            .select(
              actual.simd_eq(zeroing),
              actual.simd_eq(zeroing) | actual.simd_eq(wrapping),
            )
            .all()
        );
      } else if is_8bit {
        // All other 8-bit types also make additional guarantees

        assert!((actual.simd_eq(zeroing) | actual.simd_eq(wrapping)).all());
      } else {
        let overflow =
          cast::<SimdUnsigned, Simd>(indices.simd_ge(N as Unsigned));

        assert!((actual.simd_eq(zeroing) | overflow).all());
      }
    }
  });
}

#[test]
fn test_shuffle_zeroing() {
  for_simd_types!(|T, N| {
    // The values themselves do not matter here, as long as each lane is
    // different
    let simd = Simd::new(std::array::from_fn(|i| (i * 3 + 4) as T));

    for indices in
      shuffle_indices(1).map(|indices| indices.map(|index| index as Unsigned))
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        simd.as_array().get(indices[i] as usize).copied().unwrap_or_default()
      }));
      let actual = simd.shuffle_zeroing(SimdUnsigned::new(indices));

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_shuffle_wrapping() {
  for_simd_types!(|T, N| {
    // The values themselves do not matter here, as long as each lane is
    // different
    let simd = Simd::new(std::array::from_fn(|i| (i * 3 + 4) as T));

    for indices in
      shuffle_indices(1).map(|indices| indices.map(|index| index as Unsigned))
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        simd.as_array()[indices[i] as usize % N]
      }));
      let actual = simd.shuffle_wrapping(SimdUnsigned::new(indices));

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_array_shuffle() {
  macro_rules! test_inputs {
    ($INPUTS:literal) => {
      for_simd_types!(|T, N| {
        // The values themselves do not matter here, as long as each lane is
        // different
        let simd: [Simd; $INPUTS] = std::array::from_fn(|input| {
          Simd::new(std::array::from_fn(|lane| {
            ((input * N + lane) * 3 + 4) as T
          }))
        });

        for indices in shuffle_indices($INPUTS)
          .map(|indices| indices.map(|index| index as Unsigned))
        {
          let indices = SimdUnsigned::new(indices);
          let zeroing = simd.shuffle_zeroing(indices);
          let wrapping = simd.shuffle_wrapping(indices);
          let actual = simd.shuffle(indices);

          let is_8bit = size_of::<T>() == 1;

          if is_8bit {
            // An additional guarantee is made for all 8-bit types

            assert!((actual.simd_eq(zeroing) | actual.simd_eq(wrapping)).all());
          } else {
            let overflow = cast::<SimdUnsigned, Simd>(
              indices.simd_ge((N * $INPUTS) as Unsigned),
            );

            assert!((actual.simd_eq(zeroing) | overflow).all());
          }
        }
      });
    };
  }
  test_inputs!(2);
  test_inputs!(3);
  test_inputs!(4);
}

#[test]
fn test_array_shuffle_zeroing() {
  macro_rules! test_inputs {
    ($INPUTS:literal) => {
      for_simd_types!(|T, N| {
        // The values themselves do not matter here, as long as each lane is
        // different
        let simd: [Simd; $INPUTS] = std::array::from_fn(|input| {
          Simd::new(std::array::from_fn(|lane| {
            ((input * N + lane) * 3 + 4) as T
          }))
        });

        for indices in shuffle_indices($INPUTS)
          .map(|indices| indices.map(|index| index as Unsigned))
        {
          let expected = Simd::new(std::array::from_fn(|i| {
            simd
              .map(Simd::to_array)
              .into_iter()
              .flatten()
              .nth(indices[i] as usize)
              .unwrap_or_default()
          }));
          let actual = simd.shuffle_zeroing(SimdUnsigned::new(indices));

          assert_eq!(actual, expected);
        }
      });
    };
  }
  test_inputs!(2);
  test_inputs!(3);
  test_inputs!(4);
}

#[test]
fn test_array_shuffle_wrapping() {
  macro_rules! test_inputs {
    ($INPUTS:literal) => {
      for_simd_types!(|T, N| {
        // The values themselves do not matter here, as long as each lane is
        // different
        let simd: [Simd; $INPUTS] = std::array::from_fn(|input| {
          Simd::new(std::array::from_fn(|lane| {
            ((input * N + lane) * 3 + 4) as T
          }))
        });

        for indices in shuffle_indices($INPUTS)
          .map(|indices| indices.map(|index| index as Unsigned))
        {
          let expected = Simd::new(std::array::from_fn(|i| {
            simd
              .map(Simd::to_array)
              .into_iter()
              .flatten()
              .nth(indices[i] as usize % (N * $INPUTS))
              .unwrap()
          }));
          let actual = simd.shuffle_wrapping(SimdUnsigned::new(indices));

          assert_eq!(actual, expected);
        }
      });
    };
  }
  test_inputs!(2);
  test_inputs!(3);
  test_inputs!(4);
}

/// A helper function for shuffle tests that generates an iterator over indices
/// to test.
///
/// This can wrap indices intentionally, `inputs` (the number of input vectors)
/// is used to compute the total in-bounds index count.
///
/// Its important that this function goes over enough values to catch bugs and
/// edge cases.
fn shuffle_indices<const N: usize>(
  inputs: usize,
) -> impl Iterator<Item = [usize; N]> {
  (0..N)
    .map(|offset| std::array::from_fn(|i| i * 3 + offset))
    .chain(random_iter())
    .map(move |indices| {
      indices.map(|index| {
        // We need both in bounds and out of bounds indices, so wrap indices
        // half of the time
        if index & 0b1000 != 0 { index % (N * inputs) } else { index }
      })
    })
}

#[test]
fn test_transpose() {
  for_simd_types!(|T, N| {
    let array = std::array::from_fn(|i| {
      Simd::new(std::array::from_fn(|j| (i * 100 + j) as T))
    });
    let expected = std::array::from_fn(|i| {
      Simd::new(std::array::from_fn(|j| (j * 100 + i) as T))
    });
    let actual = Simd::transpose(array);

    assert_eq!(expected, actual);
  });
}

#[test]
fn test_unpack_lo() {
  // `unpack_lo` is inconsistently missing from types.

  let a = f32x4::new([1.0, 2.0, 3.0, 4.0]);
  let b = f32x4::new([5.0, 6.0, 7.0, 8.0]);
  let expected = f32x4::new([1.0, 5.0, 2.0, 6.0]);
  let actual = a.unpack_lo(b);
  assert_eq!(expected, actual);

  // The low half of both vectors is interleaved one element at a time, across
  // the whole vector rather than within each 128-bit lane.
  let a = u32x4::new([1, 2, 3, 4]);
  let b = u32x4::new([5, 6, 7, 8]);
  assert_eq!(u32x4::new([1, 5, 2, 6]), a.unpack_lo(b));

  let a = u32x8::new([1, 2, 3, 4, 5, 6, 7, 8]);
  let b = u32x8::new([11, 12, 13, 14, 15, 16, 17, 18]);
  assert_eq!(u32x8::new([1, 11, 2, 12, 3, 13, 4, 14]), a.unpack_lo(b));

  let a = u32x16::new([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
  let b = u32x16::new([
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
  ]);
  let expected =
    u32x16::new([1, 21, 2, 22, 3, 23, 4, 24, 5, 25, 6, 26, 7, 27, 8, 28]);
  assert_eq!(expected, a.unpack_lo(b));

  let a = u64x2::new([1, 2]);
  let b = u64x2::new([5, 6]);
  assert_eq!(u64x2::new([1, 5]), a.unpack_lo(b));

  let a = u64x4::new([1, 2, 3, 4]);
  let b = u64x4::new([5, 6, 7, 8]);
  assert_eq!(u64x4::new([1, 5, 2, 6]), a.unpack_lo(b));

  let a = u64x8::new([1, 2, 3, 4, 5, 6, 7, 8]);
  let b = u64x8::new([11, 12, 13, 14, 15, 16, 17, 18]);
  assert_eq!(u64x8::new([1, 11, 2, 12, 3, 13, 4, 14]), a.unpack_lo(b));
}

#[test]
fn test_unpack_hi() {
  // `unpack_hi` is inconsistently missing from types.

  let a = f32x4::new([1.0, 2.0, 3.0, 4.0]);
  let b = f32x4::new([5.0, 6.0, 7.0, 8.0]);
  let expected = f32x4::new([3.0, 7.0, 4.0, 8.0]);
  let actual = a.unpack_hi(b);
  assert_eq!(expected, actual);

  // As in `test_unpack_lo`, across the whole vector rather than within each
  // 128-bit lane, taking the high half of both vectors.
  let a = u32x4::new([1, 2, 3, 4]);
  let b = u32x4::new([5, 6, 7, 8]);
  assert_eq!(u32x4::new([3, 7, 4, 8]), a.unpack_hi(b));

  let a = u32x8::new([1, 2, 3, 4, 5, 6, 7, 8]);
  let b = u32x8::new([11, 12, 13, 14, 15, 16, 17, 18]);
  assert_eq!(u32x8::new([5, 15, 6, 16, 7, 17, 8, 18]), a.unpack_hi(b));

  let a = u32x16::new([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
  let b = u32x16::new([
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
  ]);
  let expected = u32x16::new([
    9, 29, 10, 30, 11, 31, 12, 32, 13, 33, 14, 34, 15, 35, 16, 36,
  ]);
  assert_eq!(expected, a.unpack_hi(b));

  let a = u64x2::new([1, 2]);
  let b = u64x2::new([5, 6]);
  assert_eq!(u64x2::new([2, 6]), a.unpack_hi(b));

  let a = u64x4::new([1, 2, 3, 4]);
  let b = u64x4::new([5, 6, 7, 8]);
  assert_eq!(u64x4::new([3, 7, 4, 8]), a.unpack_hi(b));

  let a = u64x8::new([1, 2, 3, 4, 5, 6, 7, 8]);
  let b = u64x8::new([11, 12, 13, 14, 15, 16, 17, 18]);
  assert_eq!(u64x8::new([5, 15, 6, 16, 7, 17, 8, 18]), a.unpack_hi(b));
}

/// Scalar `(add_mul_lo, add_mul_hi)` for one `u32` lane, from the definition.
fn add_mul_reference_u32<const W: u32>(acc: u32, a: u32, b: u32) -> (u32, u32) {
  let mask = u32::MAX >> (32 - W);
  let product = (a & mask) as u64 * (b & mask) as u64;
  (
    acc.wrapping_add((product as u32) & mask),
    acc.wrapping_add((product >> W) as u32),
  )
}

/// Scalar `(add_mul_lo, add_mul_hi)` for one `u64` lane, from the definition.
fn add_mul_reference_u64<const W: u32>(acc: u64, a: u64, b: u64) -> (u64, u64) {
  let mask = u64::MAX >> (64 - W);
  let product = (a & mask) as u128 * (b & mask) as u128;
  (
    acc.wrapping_add((product as u64) & mask),
    acc.wrapping_add((product >> W) as u64),
  )
}

/// Fuzz `add_mul_lo` / `add_mul_hi` for one type against the scalar reference,
/// at each of the given widths.
macro_rules! check_add_mul {
  ($Simd:ident, $reference:ident, [$($W:literal),* $(,)?]) => {
    for [acc, a, b] in random_iter::<[$Simd; 3]>() {
      let (acc_arr, a_arr, b_arr) = (acc.to_array(), a.to_array(), b.to_array());
      $({
        let expected_lo = $Simd::new(std::array::from_fn(|i| {
          $reference::<$W>(acc_arr[i], a_arr[i], b_arr[i]).0
        }));
        let expected_hi = $Simd::new(std::array::from_fn(|i| {
          $reference::<$W>(acc_arr[i], a_arr[i], b_arr[i]).1
        }));
        let ty = stringify!($Simd);
        assert_eq!(expected_lo, acc.add_mul_lo::<$W>(a, b), "lo at W={} for {ty}", $W);
        assert_eq!(expected_hi, acc.add_mul_hi::<$W>(a, b), "hi at W={} for {ty}", $W);
      })*
    }
  };
}

#[test]
fn test_add_mul_lo() {
  // `add_mul_lo` is inconsistently missing from types.

  // 3*5 = 15; 2^16 mod 2^16 = 0; 3*(2^15+1) mod 2^16 = 2^15+3; MAX+1 wraps.
  let acc = u32x4::new([7, 1, 0, u32::MAX]);
  let a = u32x4::new([3, 1 << 8, (1 << 15) + 1, 1]);
  let b = u32x4::new([5, 1 << 8, 3, 1]);
  assert_eq!(u32x4::new([22, 1, (1 << 15) + 3, 0]), acc.add_mul_lo::<16>(a, b));

  // The same shape one width family up, where 52 is the IFMA width.
  let acc = u64x4::new([7, 1, 0, u64::MAX]);
  let a = u64x4::new([3, 1 << 30, (1 << 51) + 1, 1]);
  let b = u64x4::new([5, 1 << 30, 3, 1]);
  assert_eq!(u64x4::new([22, 1, (1 << 51) + 3, 0]), acc.add_mul_lo::<52>(a, b));
}

#[test]
fn test_add_mul_hi() {
  // `add_mul_hi` is inconsistently missing from types.

  // 15 >> 16 = 0; 2^16 >> 16 = 1; (2^16+2^15+3) >> 16 = 1; 1 >> 16 = 0.
  let acc = u32x4::new([7, 1, 0, 9]);
  let a = u32x4::new([3, 1 << 8, (1 << 15) + 1, 1]);
  let b = u32x4::new([5, 1 << 8, 3, 1]);
  assert_eq!(u32x4::new([7, 2, 1, 9]), acc.add_mul_hi::<16>(a, b));

  // 15 >> 52 = 0; 2^60 >> 52 = 256; (2^52+2^51+3) >> 52 = 1; 1 >> 52 = 0.
  let acc = u64x4::new([7, 1, 0, 9]);
  let a = u64x4::new([3, 1 << 30, (1 << 51) + 1, 1]);
  let b = u64x4::new([5, 1 << 30, 3, 1]);
  assert_eq!(u64x4::new([7, 257, 1, 9]), acc.add_mul_hi::<52>(a, b));
}

#[test]
fn test_add_mul_ignores_operand_bits_above_w() {
  // The IFMA instructions truncate their operands, so every backend must too.
  let acc = u32x4::splat(0);
  let a = u32x4::splat((1 << 16) | 7);
  let b = u32x4::splat((u32::MAX ^ ((1 << 16) - 1)) | 5);
  assert_eq!(u32x4::splat(35), acc.add_mul_lo::<16>(a, b));
  assert_eq!(acc, acc.add_mul_hi::<16>(a, b));

  let acc = u64x4::splat(0);
  let a = u64x4::splat((1 << 52) | 7);
  let b = u64x4::splat((u64::MAX ^ ((1 << 52) - 1)) | 5);
  assert_eq!(u64x4::splat(35), acc.add_mul_lo::<52>(a, b));
  assert_eq!(acc, acc.add_mul_hi::<52>(a, b));
}

#[test]
fn test_add_mul_matches_reference() {
  // The widths straddle each implementation's boundaries: the 32-bit types take
  // a native lane multiply up to 16 and go scalar above it, the 64-bit types
  // take a widening multiply up to 32, IFMA at 52, and scalar otherwise.
  check_add_mul!(u32x4, add_mul_reference_u32, [1, 15, 16, 17, 26, 32]);
  check_add_mul!(u32x8, add_mul_reference_u32, [1, 15, 16, 17, 26, 32]);
  check_add_mul!(u32x16, add_mul_reference_u32, [1, 15, 16, 17, 26, 32]);
  check_add_mul!(u64x2, add_mul_reference_u64, [1, 26, 32, 33, 51, 52, 64]);
  check_add_mul!(u64x4, add_mul_reference_u64, [1, 26, 32, 33, 51, 52, 64]);
  check_add_mul!(u64x8, add_mul_reference_u64, [1, 26, 32, 33, 51, 52, 64]);
}

#[test]
fn test_unpack_low() {
  // `unpack_low` is inconsistently missing from types.

  let a = u8x16::new([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);
  let b =
    u8x16::new([12, 11, 22, 13, 99, 15, 16, 17, 8, 19, 2, 21, 22, 3, 24, 127]);
  let expected =
    u8x16::new([0, 12, 1, 11, 2, 22, 3, 13, 4, 99, 5, 15, 6, 16, 7, 17]);
  let actual = u8x16::unpack_low(a, b);
  assert_eq!(actual, expected);
}

#[test]
fn test_unpack_high() {
  // `unpack_high` is inconsistently missing from types.

  let a = u8x16::new([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);
  let b =
    u8x16::new([12, 11, 22, 13, 99, 15, 16, 17, 8, 19, 2, 21, 22, 3, 24, 127]);
  let expected =
    u8x16::new([8, 8, 9, 19, 10, 2, 11, 21, 12, 22, 13, 3, 14, 24, 15, 127]);
  let actual = u8x16::unpack_high(a, b);
  assert_eq!(actual, expected);
}

#[test]
fn test_max() {
  for_simd_types!(|T: Signed, N| {
    for [value, other] in simd_chunks!(
      [1, 2, T::MIN + 1, T::MIN, 6, -8, 12, 9],
      [17, -18, 1, 1, 19, -5, -1, -9],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| value[i].max(other[i])));
      let actual = Simd::new(value).max(Simd::new(other));

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for [value, other] in simd_chunks!(
      [1, 2, 1, 10, 6, T::MAX - 1, 12, 9],
      [17, 18, 0, 9, 19, 5, 3, 9],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| value[i].max(other[i])));
      let actual = Simd::new(value).max(Simd::new(other));

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_min() {
  for_simd_types!(|T: Signed, N| {
    for [value, other] in simd_chunks!(
      [1, 2, T::MIN + 1, T::MIN, 6, -8, 12, 9],
      [17, -18, 1, 1, 19, -5, -1, -9],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| value[i].min(other[i])));
      let actual = Simd::new(value).min(Simd::new(other));

      assert_eq!(actual, expected);
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for [value, other] in simd_chunks!(
      [1, 2, 1, 10, 6, T::MAX - 1, 12, 9],
      [17, 18, 0, 9, 19, 5, 3, 9],
    )
    .chain(random_iter())
    {
      let expected = Simd::new(std::array::from_fn(|i| value[i].min(other[i])));
      let actual = Simd::new(value).min(Simd::new(other));

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_clamp() {
  for_simd_types!(|T: Integer, N| {
    for [value, mut min, mut max] in simd_chunks!(
      [5, 5, 10, 10, 10, 10],
      [2, 6, 5, 10, 9, 11],
      [10, 10, 7, 10, 9, 11],
    )
    .chain(random_iter())
    {
      for (min, max) in min.iter_mut().zip(&mut max) {
        if *min > *max {
          *min = 5;
          *max = 6;
        }
      }

      let expected =
        Simd::new(std::array::from_fn(|i| value[i].clamp(min[i], max[i])));
      let actual = Simd::new(value).clamp(Simd::new(min), Simd::new(max));

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_reduce_max() {
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([1, 2, T::MIN + 1, T::MIN, 6, -8, -1, 9]) {
      for i in 0..=N {
        let mut value = value;
        if i != N {
          value[i] = T::MAX;
        }

        let expected = value.into_iter().max().unwrap();
        let actual = Simd::new(value).reduce_max();

        assert_eq!(actual, expected);
      }
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in simd_chunks!([1, 2, 6, 8, 100, 9]) {
      for i in 0..=N {
        let mut value = value;
        if i != N {
          value[i] = T::MAX;
        }

        let expected = value.into_iter().max().unwrap();
        let actual = Simd::new(value).reduce_max();

        assert_eq!(actual, expected);
      }
    }
  });
}

#[test]
fn test_reduce_min() {
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([-1, -2, T::MAX - 1, T::MAX, -6, 8, 1, -9]) {
      for i in 0..=N {
        let mut value = value;
        if i != N {
          value[i] = T::MIN;
        }

        let expected = value.into_iter().min().unwrap();
        let actual = Simd::new(value).reduce_min();

        assert_eq!(actual, expected);
      }
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in simd_chunks!([5, 2, 6, 8, 100, 9]) {
      for i in 0..=N {
        let mut value = value;
        if i != N {
          value[i] = 1;
        }

        let expected = value.into_iter().min().unwrap();
        let actual = Simd::new(value).reduce_min();

        assert_eq!(actual, expected);
      }
    }
  });
}

#[test]
fn test_from_small() {
  // `from_{small}` is inconsistently missing from types.

  for value in
    once([10, 2, -3, 4, 5, -6, 7, 8, 9, 7, i8::MAX, 12, 13, 6, 55, i8::MIN])
      .chain(random_iter())
  {
    let expected = i16x16::new(value.map(|x| x as i16));
    let actual = i16x16::from_i8x16(i8x16::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([
    10,
    2,
    -3,
    4,
    5,
    -6,
    7,
    8,
    9,
    7,
    i8::MAX,
    12,
    13,
    6,
    55,
    i8::MIN,
    10,
    2,
    -3,
    4,
    5,
    -6,
    7,
    8,
    9,
    7,
    i8::MAX,
    12,
    13,
    6,
    55,
    i8::MIN,
  ])
  .chain(random_iter())
  {
    let expected = i16x32::new(value.map(|x| x as i16));
    let actual = i16x32::from_i8x32(i8x32::new(value));

    assert_eq!(expected, actual);
  }

  for value in
    once([1, 2, 3, 4, 5, 6, i16::MIN + 1, i16::MIN]).chain(random_iter())
  {
    let expected = i32x8::new(value.map(|x| x as i32));
    let actual = i32x8::from_i16x8(i16x8::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([
    1,
    2,
    3,
    4,
    5,
    6,
    i16::MIN + 1,
    i16::MIN,
    1,
    2,
    3,
    4,
    5,
    6,
    i16::MIN + 1,
    i16::MIN,
  ])
  .chain(random_iter())
  {
    let expected = i32x16::new(value.map(|x| x as i32));
    let actual = i32x16::from_i16x16(i16x16::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([1, 2, 3, 4, 5, i16::MAX as u16, u16::MAX - 1, u16::MAX])
    .chain(random_iter())
  {
    let expected = i32x8::new(value.map(|x| x as i32));
    let actual = i32x8::from_u16x8(u16x8::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([10, 2, 3, 4, 5, 6, 7, 8, 9, 7, 127, 12, 13, 6, 55, 255])
    .chain(random_iter())
  {
    let expected = u16x16::new(value.map(|x| x as u16));
    let actual = u16x16::from(u8x16::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([1, 2, 3, 4, 5, i16::MAX as u16, u16::MAX - 1, u16::MAX])
    .chain(random_iter())
  {
    let expected = u32x8::new(value.map(|x| x as u32));
    let actual = u32x8::from(u16x8::new(value));

    assert_eq!(expected, actual);
  }

  for value in once([
    1,
    2,
    3,
    4,
    5,
    i16::MAX as u16,
    u16::MAX - 1,
    u16::MAX,
    1,
    2,
    3,
    4,
    5,
    i16::MAX as u16,
    u16::MAX - 1,
    u16::MAX,
  ])
  .chain(random_iter())
  {
    let expected = u32x16::new(value.map(|x| x as u32));
    let actual = u32x16::from(u16x16::new(value));

    assert_eq!(expected, actual);
  }
}

#[test]
fn test_to_array() {
  for_simd_types!(|T, N| {
    let array = std::array::from_fn(|i| i as T);

    assert_eq!(Simd::new(array).to_array(), array);
  });
}

#[test]
fn test_ne_bytes() {
  // `to_ne_bytes` returns the raw memory representation of the vector,
  // matching the primitive scalar `to_ne_bytes` per element, and
  // `from_ne_bytes` is its inverse. Both hold on little-endian and
  // big-endian targets.
  for_simd_types!(|T: Float, N| {
    for value in simd_chunks!([
      1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0,
      14.0, 15.0, 16.0,
    ])
    .chain(random_iter())
    {
      let simd = Simd::new(value);
      let bytes = simd.to_ne_bytes();
      assert_eq!(Simd::from_ne_bytes(bytes).to_ne_bytes(), bytes);

      let expected: [u8; size_of::<Simd>()] = value
        .into_iter()
        .flat_map(T::to_ne_bytes)
        .collect::<Vec<_>>()
        .try_into()
        .unwrap();
      assert_eq!(bytes, expected);
    }
  });
  for_simd_types!(|T: Integer, N| {
    for value in
      simd_chunks!([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,])
        .chain(random_iter())
    {
      let simd = Simd::new(value);
      let bytes = simd.to_ne_bytes();
      assert_eq!(Simd::from_ne_bytes(bytes).to_ne_bytes(), bytes);

      let expected: [u8; size_of::<Simd>()] = value
        .into_iter()
        .flat_map(T::to_ne_bytes)
        .collect::<Vec<_>>()
        .try_into()
        .unwrap();
      assert_eq!(bytes, expected);
    }
  });
}

#[test]
fn test_as_array() {
  for_simd_types!(|T, N| {
    let array = std::array::from_fn(|i| i as T);

    assert_eq!(Simd::new(array).as_array(), &array);
  });
}

#[test]
fn test_as_mut_array() {
  for_simd_types!(|T, N| {
    let mut array = std::array::from_fn(|i| i as T);

    assert_eq!(Simd::new(array).as_mut_array(), &mut array);
  });
}

#[test]
fn test_from_slice_unaligned() {
  // `from_slice_unaligned` only exists for select types and is inconsistent
  // with `from(...slice...)`.

  let slice = [0, 1_i8, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
  let expected =
    i8x16::new([1_i8, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
  let actual = i8x16::from_slice_unaligned(&slice[1..17]);
  assert_eq!(actual, expected);

  let slice = [0, 1, 2, 3, 4, 5, 6, 7, 8];
  let expected = i16x8::new([1, 2, 3, 4, 5, 6, 7, 8]);
  let actual = i16x8::from_slice_unaligned(&slice[1..9]);
  assert_eq!(actual, expected);
}

#[test]
fn test_from_slice() {
  for_simd_types!(|T, N| {
    for len in 0..=N + 3 {
      let vec = Vec::from_iter((0..len).map(|i| i as T));
      let slice = vec.as_slice();

      if slice.len() > N {
        // This is what should happen here, but there is no way to test it.
        // assert_panic!(Simd::from(slice));
      } else {
        let result = Simd::from(slice);
        assert_eq!(&result.as_array()[..slice.len()], slice);
        assert!(result.as_array()[slice.len()..].iter().all(|x| *x == 0 as T));
      }
    }
  });
}

#[test]
fn test_simd_align_to() {
  for_simd_types!(|T, N| {
    let array = std::array::from_fn::<T, 100, _>(|i| i as T);
    for start in 0..100 {
      let slice = &array[start..];
      let (head, body, tail) = Simd::simd_align_to(slice);

      assert_eq!(head.len() + body.len() * N + tail.len(), slice.len());
      assert_eq!(head, &slice[..head.len()]);
      for (i, body_item) in body.iter().enumerate() {
        let offset = head.len() + i * N;
        assert_eq!(body_item.as_array(), &slice[offset..offset + N]);
      }
      assert_eq!(tail, &slice[head.len() + body.len() * N..]);
    }
  });
}

#[test]
fn test_simd_align_to_mut() {
  for_simd_types!(|T, N| {
    let mut array = std::array::from_fn::<T, 100, _>(|i| i as T);
    for start in 0..100 {
      let slice = &identity(array)[start..];
      let (head, body, tail) = Simd::simd_align_to_mut(&mut array[start..]);

      assert_eq!(head.len() + body.len() * N + tail.len(), slice.len());
      assert_eq!(head, &slice[..head.len()]);
      for (i, body_item) in body.iter().enumerate() {
        let offset = head.len() + i * N;
        assert_eq!(body_item.as_array(), &slice[offset..offset + N]);
      }
      assert_eq!(tail, &slice[head.len() + body.len() * N..]);
    }
  });
}

#[cfg(feature = "serde")]
#[test]
fn test_serde() {
  for_simd_types!(|T: Float, N| {
    for value in
      simd_chunks!([0.0, 5.4, T::NAN, T::INFINITY, 3.1, -5.3, T::NEG_INFINITY])
    {
      let value = Simd::new(value);

      let serialized =
        bincode::serialize(&value).expect("serialization panicked");
      let deserialized = bincode::deserialize::<Simd>(&serialized)
        .expect("deserializaion panicked");

      assert!(
        (deserialized.simd_eq(value) | deserialized.is_nan() & value.is_nan())
          .all(),
      );
    }
  });
  for_simd_types!(|T: Signed, N| {
    for value in
      simd_chunks!([0, 1, -2, 3, T::MAX, T::MIN, T::MAX / 2, T::MIN / 2])
    {
      let value = Simd::new(value);

      let serialized =
        bincode::serialize(&value).expect("serialization panicked");
      let deserialized = bincode::deserialize::<Simd>(&serialized)
        .expect("deserializaion panicked");

      assert_eq!(deserialized, value);
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for value in simd_chunks!([0, 1, 3, T::MAX, T::MAX / 2, 5]) {
      let value = Simd::new(value);

      let serialized =
        bincode::serialize(&value).expect("serialization panicked");
      let deserialized = bincode::deserialize::<Simd>(&serialized)
        .expect("deserializaion panicked");

      assert_eq!(deserialized, value);
    }
  });
}
