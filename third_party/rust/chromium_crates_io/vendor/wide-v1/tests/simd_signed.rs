use wide::{i16x8, i16x16, i16x32, i32x4, i32x8, i32x16};

use crate::utils::{for_simd_types, random_iter, simd_chunks};

#[test]
fn test_is_positive() {
  for_simd_types!(|T: Signed, N| {
    for value in
      simd_chunks!([1, -1, 2, 3, -2, -5, 0, 6, 9, -4, 0, T::MIN, T::MAX])
    {
      let expected =
        Simd::new(value.map(|x| if x.is_positive() { !0 } else { 0 }));
      let actual = Simd::new(value).is_positive();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_is_negative() {
  for_simd_types!(|T: Signed, N| {
    for value in
      simd_chunks!([1, -1, 2, 3, -2, -5, 0, 6, 9, -4, 0, T::MIN, T::MAX])
    {
      let expected =
        Simd::new(value.map(|x| if x.is_negative() { !0 } else { 0 }));
      let actual = Simd::new(value).is_negative();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_abs() {
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([1, -15, -2, -5, 0, 9, -4, 0, T::MIN, T::MAX])
      .chain(random_iter())
    {
      let expected = Simd::new(value.map(T::wrapping_abs));
      let actual = Simd::new(value).abs();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_unsigned_abs() {
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([1, -15, -2, -5, 0, 9, -4, 0, T::MIN, T::MAX])
      .chain(random_iter())
    {
      let expected = SimdUnsigned::new(value.map(T::unsigned_abs));
      let actual = Simd::new(value).unsigned_abs();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_signum() {
  for_simd_types!(|T: Signed, N| {
    for value in simd_chunks!([1, -15, -2, -5, 0, 9, -4, 0, T::MIN, T::MAX])
      .chain(random_iter())
    {
      let expected = Simd::new(value.map(T::signum));
      let actual = Simd::new(value).signum();

      assert_eq!(actual, expected);
    }
  });
}

#[test]
fn test_mul_scale_round() {
  // `mul_scale_round` is inconsistently missing from types.

  let a = i16x8::new([100, 200, 300, 400, 500, -600, 700, -800]);
  let b = i16x8::new([900, 1000, 1100, 1200, 1300, -1400, -1500, 1600]);
  let actual = a.mul_scale_round(b);
  let expected = i16x8::new([3, 6, 10, 15, 20, 26, -32, -39]);
  assert_eq!(actual, expected);

  let a = i16x16::from([
    0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300,
    1400, 1500,
  ]);
  let b = i16x16::from([
    0, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000,
    2100, 2200, 2300,
  ]);
  let expected = i16x16::from([
    0, 3, 6, 10, 15, 20, 26, 32, 39, 47, 55, 64, 73, 83, 94, 105,
  ]);
  let actual = a.mul_scale_round(b);
  assert_eq!(actual, expected);
}

#[test]
fn test_mul_scale_round_n() {
  // `mul_scale_round_n` is inconsistently missing from types.

  let a = i16x8::new([100, 200, 300, 400, 500, -600, 700, -800]);
  let actual = a.mul_scale_round_n(0x4000);
  let expected = i16x8::new([50, 100, 150, 200, 250, -300, 350, -400]);
  assert_eq!(actual, expected);

  let a = i16x16::from([
    0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300,
    1400, 1500,
  ]);
  let expected = i16x16::from([
    0, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 551, 601, 651, 701, 751,
  ]);
  // slightly higher than 0.5 to test rounding
  let actual = a.mul_scale_round_n(16400);
  assert_eq!(expected, actual);
}

#[test]
fn test_dot() {
  // `dot` is inconsistently missing from types.

  let value = i16x8::new([1, 2, 3, 4, 5, 6, i16::MIN + 1, i16::MIN]);
  let other = i16x8::new([17, -18, 190, -20, 21, -22, 3, 2]);
  let expected = i32x4::new([-19, 490, -27, -163837]);
  let actual = value.dot(other);
  assert_eq!(actual, expected);

  let value = i16x16::new([
    1,
    2,
    3,
    4,
    5,
    6,
    i16::MIN + 1,
    i16::MIN,
    10,
    20,
    30,
    40,
    50,
    60,
    i16::MAX - 1,
    i16::MAX,
  ]);
  let other = i16x16::new([
    17, -18, 190, -20, 21, -22, 3, 2, 170, -180, 1900, -200, 210, -220, 30, 20,
  ]);
  let expected =
    i32x8::new([-19, 490, -27, -163837, -1900, 49000, -2700, 1638320]);
  let actual = value.dot(other);
  assert_eq!(actual, expected);

  let value = i16x32::from([
    1,
    2,
    3,
    4,
    5,
    6,
    i16::MIN + 1,
    i16::MIN,
    10,
    20,
    30,
    40,
    50,
    60,
    i16::MAX - 1,
    i16::MAX,
    1,
    2,
    3,
    4,
    5,
    6,
    i16::MIN + 1,
    i16::MIN,
    10,
    20,
    30,
    40,
    50,
    60,
    i16::MAX - 1,
    i16::MAX,
  ]);
  let other = i16x32::from([
    17, -18, 190, -20, 21, -22, 3, 2, 170, -180, 1900, -200, 210, -220, 30, 20,
    17, -18, 190, -20, 21, -22, 3, 2, 170, -180, 1900, -200, 210, -220, 30, 20,
  ]);
  let expected = i32x16::from([
    -19, 490, -27, -163837, -1900, 49000, -2700, 1638320, -19, 490, -27,
    -163837, -1900, 49000, -2700, 1638320,
  ]);
  let actual = value.dot(other);
  assert_eq!(actual, expected);
}
