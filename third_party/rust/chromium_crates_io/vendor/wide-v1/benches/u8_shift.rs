#![feature(stmt_expr_attributes)]
use std::{hint::black_box, time::Instant};
use wide::{u8x16, u8x32, u8x64};

const REPEATS: usize = 1_000_000;
const ITS: usize = 128;

#[inline(always)]
fn manual_bench(mut f: impl FnMut()) -> f64 {
  let start = Instant::now();
  for _ in 0..REPEATS {
    f();
    black_box(());
  }
  start.elapsed().as_secs_f64() * 1e9 / REPEATS as f64 / ITS as f64
}

#[inline(always)]
fn bench_shift<T: Copy + Default>(
  width: &str,
  prefix: &str,
  op: fn(T, u32) -> T,
) {
  let measure = #[inline(always)]
  |bounded| {
    // Small RNG for shifts.
    let mut state = 1u32;
    let mut next_shift = #[inline(always)]
    || -> u32 {
      state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
      state ^= state >> 15;
      if bounded {
        state % 8
      } else {
        state % 16
      }
    };

    // A single shift for sequential operations.
    let reused_sequential = manual_bench(
      #[inline(always)]
      || {
        let mut v = T::default();
        let r = next_shift();
        for _ in 0..ITS {
          v = black_box(op(v, r));
        }
        black_box(v);
      },
    );
    // A single shift for parallel operations.
    let reused_parallel = manual_bench(
      #[inline(always)]
      || {
        let r = next_shift();
        for _ in 0..ITS {
          black_box(op(black_box(T::default()), r));
        }
      },
    );
    // A random shift per sequential operation.
    let random_sequential = manual_bench(
      #[inline(always)]
      || {
        let mut v = T::default();
        for _ in 0..ITS {
          let r = next_shift();
          v = black_box(op(v, r));
        }
        black_box(v);
      },
    );
    // A random shift per parallel operation.
    let random_parallel = manual_bench(
      #[inline(always)]
      || {
        for _ in 0..ITS {
          let r = next_shift();
          black_box(op(black_box(T::default()), r));
        }
      },
    );
    [reused_sequential, reused_parallel, random_sequential, random_parallel]
  };
  let in_bounds = measure(true);
  let out_of_bounds = measure(false);
  println!(
    "{:<8} {:<18} {:>10.1} {:>10.1} {:>10.1} {:>10.1} {:>10.1} {:>10.1} {:>10.1} {:>10.1}",
    width,
    prefix,
    in_bounds[0], in_bounds[1], in_bounds[2], in_bounds[3],
    out_of_bounds[0], out_of_bounds[1], out_of_bounds[2], out_of_bounds[3],
  );
}

macro_rules! bench_width {
  ($ty:ty, $name:literal) => {{
    bench_shift::<$ty>(
      $name,
      "shl",
      #[inline(always)]
      |v, r| v << r,
    );
    bench_shift::<$ty>(
      $name,
      "shr",
      #[inline(always)]
      |v, r| v >> r,
    );
    bench_shift::<$ty>(
      $name,
      "unbounded_shl",
      #[inline(always)]
      |v, r| v.unbounded_shl_scalar(r),
    );
    bench_shift::<$ty>(
      $name,
      "unbounded_shr",
      #[inline(always)]
      |v, r| v.unbounded_shr_scalar(r),
    );
  }};
}

fn main() {
  println!("{:<8} {:<18} {:<43} {:<43}", "", "", "in-bounds", "out-of-bounds");
  println!(
    "{:<8} {:<18} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}",
    "width",
    "shift",
    "reused_seq",
    "reused_par",
    "random_seq",
    "random_par",
    "reused_seq",
    "reused_par",
    "random_seq",
    "random_par"
  );
  bench_width!(u8x16, "u8x16");
  println!();
  bench_width!(u8x32, "u8x32");
  println!();
  bench_width!(u8x64, "u8x64");
}
