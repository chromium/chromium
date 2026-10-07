use std::fmt::Debug;

/// Splits arrays of arbitrary length into arrays with the length of `Simd`.
///
/// Without this, its quite hard to write tests that work for any lane count.
///
/// This iterates over the same elements multiple times, so that more cases are
/// tested.
///
/// ```ignore
/// for_simd_types(|T: Float, N| {
///   for value in simd_chunks!([1.0, 2.0, 3.0, 4.0]) {
///     // For `f32x8`:
///     // - [1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0]
///     // - [3.0, 4.0, 1.0, 2.0, 3.0, 4.0, 1.0, 2.0]
///
///     // For `f64x2`:
///     // - `[1.0, 2.0]`
///     // - `[3.0, 4.0]`
///   }
///
///   for [a, b] in simd_chunks!([1.0, 2.0, 3.0, 4.0], [5.0, 6.0, 7.0, 8.0]) {
///     // For `f64x2`:
///     // - `[[1.0, 2.0], [5.0, 6.0]]`
///     // - `[[3.0, 4.0], [7.0, 8.0]]`
///   }
/// });
/// ```
macro_rules! simd_chunks {
  ($expr:expr $(,)?) => {
    $crate::utils::simd_chunks_helper::<T, _, N, _>([$expr]).map(|[result]| result)
  };
  ($($expr:expr),* $(,)?) => {
    $crate::utils::simd_chunks_helper::<T, _, N, _>([$($expr),*])
  };
}
pub(crate) use simd_chunks;

/// Does the logic for `simd_chunks` because doing logic inside a macro hurts
/// compile times.
#[doc(hidden)]
pub fn simd_chunks_helper<T, const I: usize, const O: usize, const N: usize>(
  arrays: [[T; I]; N],
) -> impl Iterator<Item = [[T; O]; N]>
where
  T: Debug + Copy + 'static,
{
  (0..I).step_by(2).map(move |offset| {
    std::array::from_fn(|i| {
      let mut iter = arrays[i].into_iter().cycle().skip(offset);
      std::array::from_fn(|_| iter.next().unwrap())
    })
  })
}

#[test]
fn test_simd_chunks() {
  type T = u32;
  const N: usize = 8;

  assert_eq!(
    simd_chunks!(
      [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
      [11, 12, 13, 14, 15, 16, 17, 18, 19, 20],
    )
    .collect::<Vec<_>>(),
    vec![
      [[1, 2, 3, 4, 5, 6, 7, 8], [11, 12, 13, 14, 15, 16, 17, 18]],
      [[3, 4, 5, 6, 7, 8, 9, 10], [13, 14, 15, 16, 17, 18, 19, 20]],
      [[5, 6, 7, 8, 9, 10, 1, 2], [15, 16, 17, 18, 19, 20, 11, 12]],
      [[7, 8, 9, 10, 1, 2, 3, 4], [17, 18, 19, 20, 11, 12, 13, 14]],
      [[9, 10, 1, 2, 3, 4, 5, 6], [19, 20, 11, 12, 13, 14, 15, 16]],
    ]
  );
  assert_eq!(
    simd_chunks!([1, 2, 3], [4, 5, 6]).collect::<Vec<_>>(),
    vec![
      [[1, 2, 3, 1, 2, 3, 1, 2], [4, 5, 6, 4, 5, 6, 4, 5]],
      [[3, 1, 2, 3, 1, 2, 3, 1], [6, 4, 5, 6, 4, 5, 6, 4]],
    ]
  );
}
