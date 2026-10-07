use std::{
  hint::black_box,
  time::{Duration, Instant},
};
use wide::{f32x4, f32x8, f32x16, f64x2, f64x4, f64x8};

/// Adaptive warmup + multi-trial measurement returning seconds-per-call.
/// Warms up for ~200ms, measures 11 trials (~200ms each), takes minimum.
fn measure(mut f: impl FnMut()) -> f64 {
  // Warmup
  let warmup = Instant::now();
  let mut warmup_iters = 0u64;
  while warmup.elapsed() < Duration::from_millis(200) {
    let batch = 10000u64;
    for _ in 0..batch {
      f();
    }
    warmup_iters += batch;
  }

  // Batch size for ~200ms per trial
  let batch = (warmup_iters / 10).max(1000);
  let trials = 11;

  let mut best = f64::INFINITY;
  for _ in 0..trials {
    let start = Instant::now();
    for _ in 0..batch {
      f();
    }
    let elapsed = start.elapsed().as_secs_f64();
    let per_call = elapsed / batch as f64;
    best = best.min(per_call);
  }
  best
}

fn mixed_lanes_f64_8() -> [f64; 8] {
  [0.001, -0.5, 2.0, 709.5, 1.5, -1.0, 710.0, 0.1]
}
fn mixed_lanes_f64_4() -> [f64; 4] {
  [0.001, -0.5, 2.0, 709.5]
}
fn mixed_lanes_f64_2() -> [f64; 2] {
  [0.5, 709.5]
}

fn mixed_lanes_f32_16() -> [f32; 16] {
  [
    0.001, -0.5, 2.0, 88.3, 1.5, -1.0, 87.0, 0.1, 0.25, -88.4, 10.0, 0.0, -5.0,
    50.0, -0.1, 0.5,
  ]
}
fn mixed_lanes_f32_8() -> [f32; 8] {
  [0.001, -0.5, 2.0, 88.3, 1.5, -1.0, 87.0, 0.1]
}
fn mixed_lanes_f32_4() -> [f32; 4] {
  [0.001, -0.5, 2.0, 88.3]
}

fn sub_f64_8() -> [f64; 8] {
  [-100.0; 8]
}
fn sub_f64_4() -> [f64; 4] {
  [-100.0; 4]
}
fn sub_f64_2() -> [f64; 2] {
  [-100.0, -500.0]
}
fn sub_f32_4() -> [f32; 4] {
  [-100.0; 4]
}
fn sub_f32_8() -> [f32; 8] {
  [-100.0; 8]
}
fn sub_f32_16() -> [f32; 16] {
  [-100.0; 16]
}

fn small_f64_8() -> [f64; 8] {
  [0.3, -0.3, 0.1, 0.4, 0.2, -0.2, 0.01, -0.01]
}
fn small_f64_4() -> [f64; 4] {
  [0.3, -0.3, 0.1, 0.4]
}
fn small_f64_2() -> [f64; 2] {
  [0.3, -0.3]
}
fn small_f32_8() -> [f32; 8] {
  [0.3, -0.3, 0.1, 0.4, 0.2, -0.2, 0.01, -0.01]
}
fn small_f32_4() -> [f32; 4] {
  [0.3, -0.3, 0.1, 0.4]
}
fn small_f32_16() -> [f32; 16] {
  [
    0.3, -0.3, 0.1, 0.4, 0.2, -0.2, 0.01, -0.01, 0.05, -0.05, 0.15, -0.15,
    0.25, -0.25, 0.001, -0.001,
  ]
}

fn cubes_f64_8() -> [f64; 8] {
  [8.0, 27.0, 64.0, 125.0, 1.0, 0.125, 1000.0, 0.001]
}
fn cubes_f64_4() -> [f64; 4] {
  [8.0, 27.0, 64.0, 125.0]
}
fn cubes_f32_8() -> [f32; 8] {
  [8.0, 27.0, 64.0, 125.0, 1.0, 0.125, 1000.0, 0.001]
}
fn cubes_f32_4() -> [f32; 4] {
  [8.0, 27.0, 64.0, 125.0]
}
fn cubes_f32_16() -> [f32; 16] {
  [
    8.0, 27.0, 64.0, 125.0, 1.0, 0.125, 1000.0, 0.001, 216.0, 0.027, 512.0,
    3375.0, 0.008, 10.0, 2.0, 7.0,
  ]
}

fn overflow_f64_8() -> [f64; 8] {
  [709.5, 708.0, 0.5, 1.0, 2.0, 709.0, 10.0, 0.1]
}
fn overflow_f64_4() -> [f64; 4] {
  [709.5, 708.0, 0.5, 1.0]
}
fn overflow_f64_2() -> [f64; 2] {
  [709.5, 708.0]
}
fn overflow_f32_8() -> [f32; 8] {
  [88.3, 88.0, 0.5, 1.0, 2.0, 88.0, 10.0, 0.1]
}
fn overflow_f32_4() -> [f32; 4] {
  [88.3, 88.0, 0.5, 1.0]
}
fn overflow_f32_16() -> [f32; 16] {
  [
    88.3, 88.0, 0.5, 1.0, 2.0, 88.0, 10.0, 0.1, 0.5, 1.0, -50.0, 50.0, 0.0,
    -0.0, -103.0, 88.72,
  ]
}

macro_rules! bench_tput {
  ($name:literal, $simd_ty:ty, $scalar_ty:ty, $simd_fn:ident, $scalar_fn:ident, $mixed:expr, $lanes:literal) => {
    let t_simd = measure(|| {
      let v = <$simd_ty>::from(black_box($mixed));
      black_box(v.$simd_fn());
    });
    let t_scalar = measure(|| {
      let arr: [$scalar_ty; $lanes] = black_box($mixed);
      let r = arr.map(|x| <$scalar_ty>::$scalar_fn(black_box(x)));
      black_box(r);
    });
    println!(
      "  {:<18} {:<14} {:>7.1}ns  {:>7.1}ns  {:>6.2}x",
      concat!(stringify!($simd_fn), "_", stringify!($simd_ty)),
      $name,
      t_simd * 1e9,
      t_scalar * 1e9,
      t_scalar / t_simd
    );
  };
}

macro_rules! bench_lat {
  ($name:literal, $simd_ty:ty, $scalar_ty:ty, $simd_fn:ident, $scalar_fn:ident, $seed:expr) => {
    let t_simd = measure(|| {
      let v = <$simd_ty>::splat(black_box($seed));
      black_box(v.$simd_fn());
    });
    let t_scalar = measure(|| {
      let x = black_box($seed);
      black_box(<$scalar_ty>::$scalar_fn(x));
    });
    println!(
      "  {:<18} {:<14} {:>7.1}ns  {:>7.1}ns  {:>6.2}x",
      concat!(stringify!($simd_fn), "_lat"),
      $name,
      t_simd * 1e9,
      t_scalar * 1e9,
      t_scalar / t_simd
    );
  };
}

fn main() {
  println!("\n== THROUGHPUT (mixed lanes, independent calls) ==");
  println!(
    "  {:<18} {:<14} {:>9} {:>9} {:>6}",
    "function", "path", "simd", "scalar", "ratio"
  );

  macro_rules! bench_all {
    ($simd:ty, $scl:ty, $lanes:literal, $mixed:expr, $sub:expr, $small:expr, $cubes:expr) => {
      bench_tput!("mixed", $simd, $scl, exp, exp, $mixed, $lanes);
      bench_tput!("mixed", $simd, $scl, ln, ln, $mixed, $lanes);
      bench_tput!("subnormal", $simd, $scl, exp, exp, $sub, $lanes);
      bench_tput!("mixed", $simd, $scl, exp_m1, exp_m1, $mixed, $lanes);
      bench_tput!("mixed", $simd, $scl, ln_1p, ln_1p, $mixed, $lanes);
      bench_tput!("small-polys", $simd, $scl, sinh, sinh, $small, $lanes);
      bench_tput!("small-polys", $simd, $scl, cosh, cosh, $small, $lanes);
      bench_tput!("mixed", $simd, $scl, tanh, tanh, $mixed, $lanes);
      bench_tput!("cubes", $simd, $scl, cbrt, cbrt, $cubes, $lanes);
    };
  }

  bench_all!(
    f64x2,
    f64,
    2,
    mixed_lanes_f64_2(),
    sub_f64_2(),
    small_f64_2(),
    [8.0, 27.0]
  );
  bench_all!(
    f64x4,
    f64,
    4,
    mixed_lanes_f64_4(),
    sub_f64_4(),
    small_f64_4(),
    cubes_f64_4()
  );
  bench_all!(
    f64x8,
    f64,
    8,
    mixed_lanes_f64_8(),
    sub_f64_8(),
    small_f64_8(),
    cubes_f64_8()
  );
  bench_all!(
    f32x4,
    f32,
    4,
    mixed_lanes_f32_4(),
    sub_f32_4(),
    small_f32_4(),
    cubes_f32_4()
  );
  bench_all!(
    f32x8,
    f32,
    8,
    mixed_lanes_f32_8(),
    sub_f32_8(),
    small_f32_8(),
    cubes_f32_8()
  );
  bench_all!(
    f32x16,
    f32,
    16,
    mixed_lanes_f32_16(),
    sub_f32_16(),
    small_f32_16(),
    cubes_f32_16()
  );

  println!(
    "\n== HOMOGENEOUS (all lanes same normal value — SIMD ideal case) =="
  );
  println!(
    "  {:<18} {:<14} {:>9} {:>9} {:>6}",
    "function", "path", "simd", "scalar", "ratio"
  );

  macro_rules! bench_homo {
    ($simd:ty, $scl:ty, $lanes:literal, $val:expr) => {{
      let t_simd_exp = measure(|| {
        black_box(<$simd>::splat(black_box($val)).exp());
      });
      let t_scl_exp = measure(|| {
        let arr: [$scl; $lanes] = core::array::from_fn(|_| black_box($val));
        black_box(arr.map(|x| <$scl>::exp(x)));
      });
      println!(
        "  {:<18} {:<14} {:>7.1}ns  {:>7.1}ns  {:>6.2}x",
        concat!("exp_", stringify!($simd)),
        "normal",
        t_simd_exp * 1e9,
        t_scl_exp * 1e9,
        t_scl_exp / t_simd_exp
      );
    }};
  }

  bench_homo!(f64x2, f64, 2, 1.5_f64);
  bench_homo!(f64x4, f64, 4, 1.5_f64);
  bench_homo!(f64x8, f64, 8, 1.5_f64);
  bench_homo!(f32x4, f32, 4, 1.5_f32);
  bench_homo!(f32x8, f32, 8, 1.5_f32);
  bench_homo!(f32x16, f32, 16, 1.5_f32);

  println!("\n== BOUNDARY PATHS (exercises r-clamp + excess decomposition) ==");
  println!(
    "  {:<18} {:<14} {:>9} {:>9} {:>6}",
    "function", "path", "simd", "scalar", "ratio"
  );

  bench_tput!("near-overflow", f64x2, f64, exp, exp, overflow_f64_2(), 2);
  bench_tput!("near-overflow", f64x4, f64, exp, exp, overflow_f64_4(), 4);
  bench_tput!("near-overflow", f64x8, f64, exp, exp, overflow_f64_8(), 8);
  bench_tput!("near-overflow", f32x4, f32, exp, exp, overflow_f32_4(), 4);
  bench_tput!("near-overflow", f32x8, f32, exp, exp, overflow_f32_8(), 8);
  bench_tput!("near-overflow", f32x16, f32, exp, exp, overflow_f32_16(), 16);
  bench_tput!("near-overflow", f64x2, f64, exp_m1, exp_m1, overflow_f64_2(), 2);
  bench_tput!("near-overflow", f32x4, f32, exp_m1, exp_m1, overflow_f32_4(), 4);
  bench_tput!("near-overflow", f64x2, f64, sinh, sinh, overflow_f64_2(), 2);
  bench_tput!("near-overflow", f32x4, f32, sinh, sinh, overflow_f32_4(), 4);
  bench_tput!("all-large", f64x2, f64, tanh, tanh, [50.0, -30.0], 2);
  bench_tput!("all-large", f64x8, f64, tanh, tanh, [50.0; 8], 8);
  bench_tput!("large |x|>20", f64x2, f64, tanh, tanh, [50.0, 0.5], 2);
  bench_tput!(
    "large |x|>20",
    f64x8,
    f64,
    tanh,
    tanh,
    [50.0, 0.5, 2.0, 1.0, 30.0, 0.1, 100.0, -0.3],
    8
  );
  bench_tput!("neg-asymptote", f64x2, f64, exp_m1, exp_m1, [-50.0, -40.0], 2);
  bench_tput!(
    "neg-asymptote",
    f32x4,
    f32,
    exp_m1,
    exp_m1,
    [-20.0, -30.0, -50.0, -100.0],
    4
  );

  println!("\n== SINGLE-VALUE THROUGHPUT (splat input, independent calls) ==");
  println!(
    "  {:<18} {:<14} {:>9} {:>9} {:>6}",
    "function", "path", "simd", "scalar", "ratio"
  );

  bench_lat!("f64", f64x2, f64, exp, exp, 1.5_f64);
  bench_lat!("f64", f64x2, f64, ln, ln, std::f64::consts::E);
  bench_lat!("f64", f64x2, f64, exp_m1, exp_m1, 0.5_f64);
  bench_lat!("f64", f64x2, f64, ln_1p, ln_1p, 0.5_f64);
  bench_lat!("f64", f64x2, f64, sinh, sinh, 0.3_f64);
  bench_lat!("f64", f64x2, f64, cosh, cosh, 0.3_f64);
  bench_lat!("f64", f64x2, f64, tanh, tanh, 0.5_f64);
  bench_lat!("f64", f64x2, f64, cbrt, cbrt, 27.0_f64);

  bench_lat!("f32", f32x4, f32, exp, exp, 1.5_f32);
  bench_lat!("f32", f32x4, f32, ln, ln, std::f32::consts::E);
  bench_lat!("f32", f32x4, f32, exp_m1, exp_m1, 0.5_f32);
  bench_lat!("f32", f32x4, f32, ln_1p, ln_1p, 0.5_f32);
  bench_lat!("f32", f32x4, f32, sinh, sinh, 0.3_f32);
  bench_lat!("f32", f32x4, f32, cosh, cosh, 0.3_f32);
  bench_lat!("f32", f32x4, f32, tanh, tanh, 0.5_f32);
  bench_lat!("f32", f32x4, f32, cbrt, cbrt, 27.0_f32);

  println!("\n== PER-LANE EFFICIENCY (ns/lane, lower is better) ==");
  println!(
    "  {:<18} {:>8} {:>8} {:>8}",
    "function", "x2/x4", "x4/x8", "x8/x16"
  );

  macro_rules! per_lane_f64 {
    ($fn:ident, $m2:expr, $m4:expr, $m8:expr) => {
      let t2 = measure(|| {
        black_box(f64x2::from(black_box($m2)).$fn());
      }) * 1e9
        / 2.0;
      let t4 = measure(|| {
        black_box(f64x4::from(black_box($m4)).$fn());
      }) * 1e9
        / 4.0;
      let t8 = measure(|| {
        black_box(f64x8::from(black_box($m8)).$fn());
      }) * 1e9
        / 8.0;
      println!(
        "  {:<18} {:>7.2}ns {:>7.2}ns {:>7.2}ns",
        concat!(stringify!($fn), "_f64"),
        t2,
        t4,
        t8
      );
    };
  }

  macro_rules! per_lane_f32 {
    ($fn:ident, $m4:expr, $m8:expr, $m16:expr) => {
      let t4 = measure(|| {
        black_box(f32x4::from(black_box($m4)).$fn());
      }) * 1e9
        / 4.0;
      let t8 = measure(|| {
        black_box(f32x8::from(black_box($m8)).$fn());
      }) * 1e9
        / 8.0;
      let t16 = measure(|| {
        black_box(f32x16::from(black_box($m16)).$fn());
      }) * 1e9
        / 16.0;
      println!(
        "  {:<18} {:>7.2}ns {:>7.2}ns {:>7.2}ns",
        concat!(stringify!($fn), "_f32"),
        t4,
        t8,
        t16
      );
    };
  }

  per_lane_f64!(
    exp,
    mixed_lanes_f64_2(),
    mixed_lanes_f64_4(),
    mixed_lanes_f64_8()
  );
  per_lane_f64!(
    ln,
    mixed_lanes_f64_2(),
    mixed_lanes_f64_4(),
    mixed_lanes_f64_8()
  );
  per_lane_f64!(
    exp_m1,
    mixed_lanes_f64_2(),
    mixed_lanes_f64_4(),
    mixed_lanes_f64_8()
  );
  per_lane_f64!(
    ln_1p,
    mixed_lanes_f64_2(),
    mixed_lanes_f64_4(),
    mixed_lanes_f64_8()
  );
  per_lane_f64!(sinh, small_f64_2(), small_f64_4(), small_f64_8());
  per_lane_f64!(cosh, small_f64_2(), small_f64_4(), small_f64_8());
  per_lane_f64!(
    tanh,
    mixed_lanes_f64_2(),
    mixed_lanes_f64_4(),
    mixed_lanes_f64_8()
  );
  per_lane_f64!(cbrt, [8.0, 27.0], cubes_f64_4(), cubes_f64_8());

  per_lane_f32!(
    exp,
    mixed_lanes_f32_4(),
    mixed_lanes_f32_8(),
    mixed_lanes_f32_16()
  );
  per_lane_f32!(
    ln,
    mixed_lanes_f32_4(),
    mixed_lanes_f32_8(),
    mixed_lanes_f32_16()
  );
  per_lane_f32!(
    exp_m1,
    mixed_lanes_f32_4(),
    mixed_lanes_f32_8(),
    mixed_lanes_f32_16()
  );
  per_lane_f32!(
    ln_1p,
    mixed_lanes_f32_4(),
    mixed_lanes_f32_8(),
    mixed_lanes_f32_16()
  );
  per_lane_f32!(sinh, small_f32_4(), small_f32_8(), small_f32_16());
  per_lane_f32!(cosh, small_f32_4(), small_f32_8(), small_f32_16());
  per_lane_f32!(
    tanh,
    mixed_lanes_f32_4(),
    mixed_lanes_f32_8(),
    mixed_lanes_f32_16()
  );
  per_lane_f32!(cbrt, [8.0, 27.0, -1.0, -8.0], cubes_f32_8(), cubes_f32_16());
  println!();
}
