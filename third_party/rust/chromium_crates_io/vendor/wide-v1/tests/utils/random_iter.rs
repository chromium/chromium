use wide::{
  f32x4, f32x8, f32x16, f64x2, f64x4, f64x8, i8x16, i8x32, i16x8, i16x16,
  i16x32, i32x4, i32x8, i32x16, i64x2, i64x4, i64x8, u8x16, u8x32, u16x8,
  u16x16, u16x32, u32x4, u32x8, u32x16, u64x2, u64x4, u64x8,
};

/// Returns an iterator over 100 random values of type `T`.
///
/// This is used for fuzz-testing.
#[expect(private_bounds)]
pub fn random_iter<T>() -> impl Iterator<Item = T>
where
  T: Random,
{
  const SEED: u64 = 0x123456789abcdef0;
  const ITERATIONS: usize = 1000;

  let mut state = SEED;
  (0..ITERATIONS).map(move |_| T::random(&mut state))
}

/// Generates the next pseudo-random number.
/// Definitely non-cryptographic, just used for generating random test values.
fn update_state(state: &mut u64) {
  // Constants for the LCG
  const A: u64 = 6364136223846793005;
  const C: u64 = 1442695040888963407;

  // Update the state and calculate the next number (rotate to avoid lack of
  // randomness in low bits)
  *state = state.wrapping_mul(A).wrapping_add(C).rotate_left(31);
}

fn next_state(state: &mut u64) -> u64 {
  update_state(state);

  // generate special values more often than random chance to test edge cases
  match *state & 0xf {
    0 => 0,
    1 => 1,
    2 => u64::MAX,
    _ => {
      update_state(state);
      *state
    }
  }
}

trait Random {
  fn random(state: &mut u64) -> Self;
}

macro_rules! impl_random_for_float {
  ($T:ident) => {
    impl Random for $T {
      fn random(state: &mut u64) -> Self {
        let state = next_state(state);

        // generate special float values more often than random
        // chance to test edge cases
        let m = (state >> 8) & 15;

        match m {
          1 => Self::NAN,
          2 => Self::INFINITY,
          3 => Self::NEG_INFINITY,
          _ => ((state as i64) as Self) / 7.0,
        }
      }
    }
  };
}
impl_random_for_float!(f32);
impl_random_for_float!(f64);

macro_rules! impl_random_for_integer {
  ($T:ident) => {
    impl Random for $T {
      fn random(state: &mut u64) -> Self {
        next_state(state) as Self
      }
    }
  };
}
impl_random_for_integer!(i8);
impl_random_for_integer!(i16);
impl_random_for_integer!(i32);
impl_random_for_integer!(i64);
impl_random_for_integer!(u8);
impl_random_for_integer!(u16);
impl_random_for_integer!(u32);
impl_random_for_integer!(u64);
impl_random_for_integer!(usize);

macro_rules! impl_random_for_simd {
  ($T:ident, $Simd:ident) => {
    impl Random for $Simd {
      fn random(state: &mut u64) -> Self {
        Self::from(std::array::from_fn(|_| $T::random(state)))
      }
    }
  };
}
impl_random_for_simd!(f32, f32x4);
impl_random_for_simd!(f32, f32x8);
impl_random_for_simd!(f32, f32x16);
impl_random_for_simd!(f64, f64x2);
impl_random_for_simd!(f64, f64x4);
impl_random_for_simd!(f64, f64x8);
impl_random_for_simd!(i8, i8x16);
impl_random_for_simd!(i8, i8x32);
impl_random_for_simd!(i16, i16x8);
impl_random_for_simd!(i16, i16x16);
impl_random_for_simd!(i16, i16x32);
impl_random_for_simd!(i32, i32x4);
impl_random_for_simd!(i32, i32x8);
impl_random_for_simd!(i32, i32x16);
impl_random_for_simd!(i64, i64x2);
impl_random_for_simd!(i64, i64x4);
impl_random_for_simd!(i64, i64x8);
impl_random_for_simd!(u8, u8x16);
impl_random_for_simd!(u8, u8x32);
impl_random_for_simd!(u16, u16x8);
impl_random_for_simd!(u16, u16x16);
impl_random_for_simd!(u16, u16x32);
impl_random_for_simd!(u32, u32x4);
impl_random_for_simd!(u32, u32x8);
impl_random_for_simd!(u32, u32x16);
impl_random_for_simd!(u64, u64x2);
impl_random_for_simd!(u64, u64x4);
impl_random_for_simd!(u64, u64x8);

impl<T, const N: usize> Random for [T; N]
where
  T: Random,
{
  fn random(state: &mut u64) -> Self {
    std::array::from_fn(|_| T::random(state))
  }
}

impl<T0, T1> Random for (T0, T1)
where
  T0: Random,
  T1: Random,
{
  fn random(state: &mut u64) -> Self {
    (T0::random(state), T1::random(state))
  }
}
