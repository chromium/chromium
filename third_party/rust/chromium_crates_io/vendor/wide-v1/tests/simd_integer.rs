#![allow(clippy::unnecessary_cast)]

use wide::{
  f32x4, f32x8, f32x16, i8x16, i8x32, i16x8, i16x16, i32x4, i32x8, i32x16,
  u8x16, u16x8,
};

use crate::utils::{for_simd_types, random_iter, simd_chunks};

#[test]
fn test_unbounded_shl() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!(
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
    .map(|[left, right]| (left, right.map(T::cast_unsigned)))
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        // Cannot use `as u32` because that truncates, not saturates.
        left[i].unbounded_shl((right[i] as u128).min(u32::MAX as u128) as u32)
      }));
      let actual = Simd::new(left).unbounded_shl(SimdUnsigned::new(right));

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
      let expected = Simd::new(std::array::from_fn(|i| {
        // Cannot use `as u32` because that truncates, not saturates.
        left[i].unbounded_shl((right[i] as u128).min(u32::MAX as u128) as u32)
      }));
      let actual = Simd::new(left).unbounded_shl(Simd::new(right));

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_unbounded_shl_scalar() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!([
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
    ],)
    .flat_map(|left| {
      [1, 0, 3, 2, 6, 100, 0, 4, 1, 3, 123].map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].unbounded_shl(right)));
      let actual = Simd::new(left).unbounded_shl_scalar(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for (left, right) in simd_chunks!([
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
    ],)
    .flat_map(|left| {
      [1, 0, 3, 2, 6, 100, 0, 4, 1, 3, 123].map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].unbounded_shl(right)));
      let actual = Simd::new(left).unbounded_shl_scalar(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_unbounded_shr() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!(
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
    .map(|[left, right]| (left, right.map(T::cast_unsigned)))
    {
      let expected = Simd::new(std::array::from_fn(|i| {
        // Cannot use `as u32` because that truncates, not saturates.
        left[i].unbounded_shr((right[i] as u128).min(u32::MAX as u128) as u32)
      }));
      let actual = Simd::new(left).unbounded_shr(SimdUnsigned::new(right));

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
      let expected = Simd::new(std::array::from_fn(|i| {
        // Cannot use `as u32` because that truncates, not saturates.
        left[i].unbounded_shr((right[i] as u128).min(u32::MAX as u128) as u32)
      }));
      let actual = Simd::new(left).unbounded_shr(Simd::new(right));

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_unbounded_shr_scalar() {
  for_simd_types!(|T: Signed, N| {
    for (left, right) in simd_chunks!([
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
    ],)
    .flat_map(|left| {
      [1, 0, 3, 2, 6, 100, 0, 4, 1, 3, 123].map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].unbounded_shr(right)));
      let actual = Simd::new(left).unbounded_shr_scalar(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
  for_simd_types!(|T: Unsigned, N| {
    for (left, right) in simd_chunks!([
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
    ],)
    .flat_map(|left| {
      [1, 0, 3, 2, 6, 100, 0, 4, 1, 3, 123].map(|right| (left, right))
    })
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].unbounded_shr(right)));
      let actual = Simd::new(left).unbounded_shr_scalar(right);

      assert!(
        actual == expected,
        "\nexpected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_saturating_add() {
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MAX - 1, 15, 20, 100, T::MAX - 1, T::MAX / 2],
      [17, 18, 1, 2, 20, 5, T::MAX - 5, 50, 100],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].saturating_add(right[i])));
      let actual = Simd::new(left).saturating_add(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MIN + 1, T::MIN + 2, T::MIN / 2, 9],
      [-17, 18, T::MAX, -2, -20, -100, 10],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].saturating_add(right[i])));
      let actual = Simd::new(left).saturating_add(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
}

#[test]
fn test_saturating_sub() {
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MIN + 1, T::MIN + 1, 15, 20, 100, T::MIN + 1, T::MIN / 2],
      [17, 18, 1, 2, 20, 5, T::MAX - 5, 50, 100],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].saturating_sub(right[i])));
      let actual = Simd::new(left).saturating_sub(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MIN + 1, T::MIN + 2, T::MAX / 2],
      [17, -18, T::MIN, 2, 20, -100],
    )
    .chain(random_iter())
    {
      let expected =
        Simd::new(std::array::from_fn(|i| left[i].saturating_sub(right[i])));
      let actual = Simd::new(left).saturating_sub(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
}

#[test]
fn test_saturating_mul() {
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
        Simd::new(std::array::from_fn(|i| left[i].saturating_mul(right[i])));
      let actual = Simd::new(left).saturating_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
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
        Simd::new(std::array::from_fn(|i| left[i].saturating_mul(right[i])));
      let actual = Simd::new(left).saturating_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_saturating_div() {
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
        Simd::new(std::array::from_fn(|i| left[i].saturating_div(right[i])));
      let actual = Simd::new(left).saturating_div(Simd::new(right));

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
        Simd::new(std::array::from_fn(|i| left[i].saturating_div(right[i])));
      let actual = Simd::new(left).saturating_div(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}",
      );
    }
  });
}

#[test]
fn test_overflowing_add() {
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MAX - 1, 15, 20, 100, T::MAX - 1, T::MAX / 2],
      [17, 18, 1, 2, 20, 5, T::MAX - 5, 50, 100],
    )
    .chain(random_iter())
    {
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_add(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_add(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_add(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MIN + 1, T::MIN + 2, T::MIN / 2, 9],
      [-17, 18, T::MAX, -2, -20, -100, 10],
    )
    .chain(random_iter())
    {
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_add(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_add(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_add(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
}

#[test]
fn test_overflowing_sub() {
  for_simd_types!(|T: Integer, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MIN + 1, T::MIN + 1, 15, 20, 100, T::MIN + 1, T::MIN / 2],
      [17, 18, 1, 2, 20, 5, T::MAX - 5, 50, 100],
    )
    .chain(random_iter())
    {
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_sub(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_sub(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_sub(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
  for_simd_types!(|T: Signed, N| {
    for [left, right] in simd_chunks!(
      [1, 2, T::MAX - 1, T::MIN + 1, T::MIN + 2, T::MAX / 2],
      [17, -18, T::MIN, 2, 20, -100],
    )
    .chain(random_iter())
    {
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_sub(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_sub(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_sub(Simd::new(right));

      assert_eq!(expected, actual);
    }
  });
}

#[test]
fn test_overflowing_mul() {
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
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_mul(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_mul(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
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
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_mul(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_mul(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_overflowing_div() {
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

      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_div(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_div(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_div(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
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

      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_div(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_div(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_div(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_overflowing_rem() {
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

      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_rem(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_rem(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_rem(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
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

      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].overflowing_rem(right[i]).0)),
        Simd::new(std::array::from_fn(|i| {
          if left[i].overflowing_rem(right[i]).1 { !0 } else { 0 }
        })),
      );
      let actual = Simd::new(left).overflowing_rem(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_widening_mul() {
  for_simd_types!(|T: Signed, N, DoubleSizedSimd| {
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
      let expected = DoubleSizedSimd::new(std::array::from_fn(|i| {
        (left[i] as DoubleSizedT) * (right[i] as DoubleSizedT)
      }));
      let actual = Simd::new(left).widening_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
  for_simd_types!(|T: Unsigned, N, DoubleSizedSimd| {
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
      let expected = DoubleSizedSimd::new(std::array::from_fn(|i| {
        (left[i] as DoubleSizedT) * (right[i] as DoubleSizedT)
      }));
      let actual = Simd::new(left).widening_mul(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_mul_keep_low_high() {
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
      let expected = (
        SimdUnsigned::new(std::array::from_fn(|i| {
          left[i].wrapping_mul(right[i]).cast_unsigned()
        })),
        Simd::new(std::array::from_fn(|i| {
          ((left[i] as DoubleSizedT).wrapping_mul(right[i] as DoubleSizedT)
            >> T::BITS) as T
        })),
      );
      let actual = Simd::new(left).mul_keep_low_high(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
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
      let expected = (
        Simd::new(std::array::from_fn(|i| left[i].wrapping_mul(right[i]))),
        Simd::new(std::array::from_fn(|i| {
          ((left[i] as DoubleSizedT).wrapping_mul(right[i] as DoubleSizedT)
            >> T::BITS) as T
        })),
      );
      let actual = Simd::new(left).mul_keep_low_high(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_mul_keep_high() {
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
      let expected = Simd::new(std::array::from_fn(|i| {
        ((left[i] as DoubleSizedT).wrapping_mul(right[i] as DoubleSizedT)
          >> T::BITS) as T
      }));
      let actual = Simd::new(left).mul_keep_high(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
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
      let expected = Simd::new(std::array::from_fn(|i| {
        ((left[i] as DoubleSizedT).wrapping_mul(right[i] as DoubleSizedT)
          >> T::BITS) as T
      }));
      let actual = Simd::new(left).mul_keep_high(Simd::new(right));

      assert!(
        actual == expected,
        "expected: {expected:?}\n  actual: {actual:?}\n    left: {left:?}\n   right: {right:?}"
      );
    }
  });
}

#[test]
fn test_from_big_truncate() {
  // `from_{big}_truncate` is inconsistently missing from types.

  let value = i16x16::new([
    10000, 1001, 2, 3, 4, 5, 6, 32767, 10000, 1001, 2, 128, -129, -128, 127,
    255,
  ]);
  let expected = i8x16::new([
    16, -23, 2, 3, 4, 5, 6, -1, 16, -23, 2, -128, 127, -128, 127, -1,
  ]);
  let actual = i8x16::from_i16x16_truncate(value);
  assert_eq!(actual, expected);

  let value = i32x8::new([10000, 1001, 2, 3, 4, 5, -65536, 65536]);
  let expected = i16x8::new([10000, 1001, 2, 3, 4, 5, 0, 0]);
  let actual = i16x8::from_i32x8_truncate(value);
  assert_eq!(actual, expected);
}

#[test]
fn test_from_big_saturate() {
  // `from_{big}_saturate` is inconsistently missing from types.

  let value = i16x16::new([
    10000, 1001, 2, 3, 4, 5, 6, 32767, 10000, 1001, 2, 128, -129, -128, 127,
    255,
  ]);
  let expected = i8x16::new([
    127, 127, 2, 3, 4, 5, 6, 127, 127, 127, 2, 127, -128, -128, 127, 127,
  ]);
  let actual = i8x16::from_i16x16_saturate(value);
  assert_eq!(actual, expected);

  let value = i32x8::new([10000, 1001, 2, 3, 4, 5, -65535, 65536]);
  let expected = i16x8::new([10000, 1001, 2, 3, 4, 5, -32768, 32767]);
  let actual = i16x8::from_i32x8_saturate(value);
  assert_eq!(actual, expected);
}

#[test]
fn test_round_float() {
  // `round_float` only exists for select types.

  let value = i32x4::new([-1, 30, i32::MIN, i32::MAX]);
  let expected = f32x4::new([-1.0, 30.0, i32::MIN as f32, i32::MAX as f32]);
  let actual = value.round_float();
  assert_eq!(actual, expected);

  let value =
    i32x16::new([0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1]);
  let expected = f32x16::new([
    0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, -8.0, -7.0, -6.0, -5.0, -4.0, -3.0,
    -2.0, -1.0,
  ]);
  let actual = value.round_float();
  assert_eq!(actual, expected);

  let value = i32x8::new([-1, 30, i32::MIN, i32::MAX, 29, 35, -8, 0]);
  let expected = f32x8::new([
    -1.0,
    30.0,
    i32::MIN as f32,
    i32::MAX as f32,
    29.0,
    35.0,
    -8.0,
    0.0,
  ]);
  let actual = value.round_float();
  assert_eq!(actual, expected);
}

#[test]
fn test_swizzle_half() {
  // `swizzle_half` is inconsistently missing from types.

  let value = i8x32::new([
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
  ]);
  let indices = i8x32::new([
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11,
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  ]);
  let expected = i8x32::new([
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 32, 31, 30, 29, 28,
    27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
  ]);
  let actual = value.swizzle_half(indices);
  assert_eq!(actual, expected);
}

#[test]
fn test_swizzle_half_out_of_range_zeroes() {
  // `swizzle_half` is strict: any index outside `[0, 15]` (per half) must zero
  // that output lane. This previously failed on the AVX2 path, which used a
  // signed `saturating_add(0x60)` that cannot set the high bit that makes
  // `pshufb` zero, so out-of-range indices leaked `self[..][0]`.
  let value = i8x32::new([
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
  ]);
  for &bad in &[16i8, 17, 31, 100, -1, -128] {
    let indices = i8x32::new([bad; 32]);
    assert_eq!(
      value.swizzle_half(indices),
      i8x32::new([0i8; 32]),
      "index {bad} is out of range and must zero every lane",
    );
  }
}

#[test]
fn test_from_u8x16_low() {
  // This function only exists for select types.

  let value =
    u8x16::new([1, 2, 3, 4, 5, 6, 7, u8::MAX, 9, 10, 11, 12, 13, 14, 15, 16]);
  let expected = i16x8::new([1, 2, 3, 4, 5, 6, 7, u8::MAX as i16]);
  let actual = i16x8::from_u8x16_low(value);
  assert_eq!(actual, expected);

  let value =
    u8x16::from([255, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255, 128]);
  let expected = u16x8::from([255, 2, 3, 4, 5, 6, 7, 8]);
  let actual = u16x8::from_u8x16_low(value);
  assert_eq!(actual, expected);
}

#[test]
fn test_from_u8x16_high() {
  // This function only exists for select types.

  let value =
    u8x16::new([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255, 128]);
  let expected = i16x8::new([9, 10, 11, 12, 13, 14, 255, 128]);
  let actual = i16x8::from_u8x16_high(value);
  assert_eq!(actual, expected);

  let value =
    u8x16::from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255, 128]);
  let expected = u16x8::from([9, 10, 11, 12, 13, 14, 255, 128]);
  let actual = u16x8::from_u8x16_high(value);
  assert_eq!(actual, expected);
}

#[test]
fn test_narrow_i16x8() {
  // This function only exists for select types.

  let a = i16x8::new([-1, 2, -3, 4, -5, 6, -7, 8]);
  let b = i16x8::new([9, 10, 11, 12, 13, -14, 15, -16]);
  let expected =
    u8x16::new([0, 2, 0, 4, 0, 6, 0, 8, 9, 10, 11, 12, 13, 0, 15, 0]);
  let actual = u8x16::narrow_i16x8(a, b);
  assert_eq!(actual, expected);
}
