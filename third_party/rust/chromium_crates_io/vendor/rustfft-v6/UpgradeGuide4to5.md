# 4.0 to 5.0 Upgrade Guide
RustFFT 5.0 has several breaking changes compared to 4.0. This document will guide users through the upgrade process, explaining each breaking change and how to upgrade code to fit the new style.

Each section is ordered by how likely they are to impact you: Things at the top are likely to affect every user of RustFFT, while things at the bottom are unlikely to affect most users.

## Renaming Structs
Several structs and traits in RustFFT were renamed to follow the [Rust API guidlines](https://rust-lang.github.io/api-guidelines/naming.html) regarding acronyms:

> In UpperCamelCase, acronyms and contractions of compound words count as one word: use Uuid rather than UUID, Usize rather than USize or Stdin rather than StdIn.

The following were renamed in RustFFT 5.0 to conform to this style:
* The `FFT` trait was renamed to `Fft`
* The `FFTnum` trait was renamed to `FftNum`
* The `FFTplanner` struct was renamed to `FftPlanner`
* The `DFT` struct was renamed to `Dft`

## FFT Direction
In RustFFT 4.0, forward FFTs vs inverse FFTs were specified by a boolean. For example, the 4.0 `FFTplanner` constructor expects a boolean parameter for direction: If you pass `false`, the planner will plan forward FFTs. If you pass `true`, the planner will plan inverse FFTs.

In 5.0, there is a new `FftDirection` enum with `Forward` and `Inverse` variants. FFT algorithms that took a `bool` for direction now take `FftDirection` instead. For example, if you were constructing a `Radix4` instance to compute a power-of-two FFT, you will have to change the parameters to the constructor:

```rust 
// RustFFT 4.0
let fft_forward = Radix4::new(4096, false);
let fft_inverse = Radix4::new(4096, true);

// RustFFT 5.0
let fft_forward = Radix4::new(4096, FftDirection::Forward);
let fft_inverse = Radix4::new(4096, FftDirection::Inverse);
```

A few traits and methods were renamed to support the new `FftDirection` trait:
* The `IsInverse` trait was renamed to `Direction`
* `IsInverse`'s only method, `is_inverse(&self) -> bool`, was renamed to `fft_direction(&self) -> FftDirection`
* The `Fft` trait inherited the `IsInverse` trait, and now inherits the `Direction` trait instead, so if you were calling `is_inverse()` on a `Fft` instance you'll have to change that to `fft_direction()` as well.

Finally, the way the `FftPlanner` handles forward vs inverse FFTs has changed. In 4.0, the `FFTplanner` took a direction in its constructor -- In 5.0, its constructor is empty, and it takes a direction in its `plan_fft` method. This means a single planner can be used to plan both forward and inverse FFTs.

The `FftPlanner` also has `plan_fft_forward` and `plan_fft_inverse` convenince methods so that you don't have to import the FftDirection enum.

```rust
// RustFFT 4.0
let planner_forward = FFTplanner::new(false);
let fft_forward = planner.plan_fft(1234);

let planner_inverse = FFTplanner::new(true);
let fft_inverse = planner.plan_fft(1234);

// RustFFT 5.0
let planner = FftPlanner::new();

let fft_forward1 = planner.plan_fft(1234, FftDirection::Forward);
let fft_forward2 = planner.plan_fft_forward(1234);

let fft_inverse1 = planner.plan_fft(1234, FftDirection::Inverse);
let fft_inverse2 = planner.plan_fft_inverse(1234);
```

## Fft Trait Methods
In RustFFT 4.0, the `Fft` trait has two methods:
 * `FFT::process()` took an input and output buffer, and computed a single FFT, storing the result in the output buffer.
 * `FFT::process_multi()` took an input and output buffer, divided those buffers into chunks of size `fft.len()`, and computed a FFT on each chunk.

RustFFT 5.0 makes a few changes to this setup. First, there is no longer a distinction between "single" and "multi" FFT methods: All `Fft` trait methods compute multiple FFTs if provided a buffer whose length is a multiple of the FFT length.

Second, the `Fft` trait now has three methods. Most users will want the first method:
1. `Fft::process()` takes a single buffer instead of two, and computes FFTs in-place. Internally, it allocates scratch space as needed.
1. `Fft::process_with_scratch()` takes two buffers: A data buffer, and a scratch buffer. It computes FFTs in-place on the data buffer, using the provided scratch space as needed.
1. `Fft::process_outofplace_with_scratch()` takes three buffers: An input buffer, an output buffer, and a scratch buffer. It computes FFTs from the input buffer and stores the results in the output buffer, using the provided scratch space as needed.

Example for users who want to use the new in-place `process()` behavior:
```rust
// RustFFT 4.0
let fft = Radix4::new(4096, false);

let mut input : Vec<Complex<f32>> = get_my_input_data();
let mut output = vec![Complex::zero(); fft.len()];
fft.process(&mut input, &mut output);

// RustFFT 5.0
let fft = Radix4::new(4096, FftDirection::Forward);

let mut buffer : Vec<Complex<f32>> = get_my_input_data();
fft.process(&mut buffer);
```

Example for users who want to keep the old out-of-place `process()` behavior from RustFFT 4.0:
```rust
// RustFFT 4.0
let fft = Radix4::new(4096, false);

let mut input : Vec<Complex<f32>> = get_my_input_data();
let mut output = vec![Complex::zero(); fft.len()];
fft.process(&mut input, &mut output);

// RustFFT 5.0
let fft = Radix4::new(4096, FftDirection::Forward);

let mut input : Vec<Complex<f32>> = get_my_input_data();
let mut output = vec![Complex::zero(); fft.len()];
let mut scratch = vec![Complex::zero(); fft.get_outofplace_scratch_len()];
fft.process_outofplace_with_scratch(&mut input, &mut output, &mut scratch);
```

## Rader's Algorithm Constructor
The constructor for `RadersAlgorithm` has changed. In RustFFT 4.0, its signature was `pub fn new(len: usize, inner_fft: Arc<dyn FFT<T>>)`, and it asserted that `len == inner_fft.len() + 1`

RustFFT 5.0 removes the `len: usize` parameter, and `RadersAlgorithm` derives its FFT length from the inner FFT length instead.

```rust
// RustFFT 4.0
let inner_fft : Arc<dyn Fft<T>> = ...;
let fft = RadersAlgorithm::new(inner_fft.len() + 1, inner_fft);

// RustFFT 5.0
let inner_fft : Arc<dyn Fft<T>> = ...;
let fft = RadersAlgorithm::new(inner_fft);
```

## Deleted the `FFTButterfly` trait
In RustFFT 4.0, there was a trait called `FFTbutterfly`. This trait has been deleted. It had two methods which were merged into the `Fft` trait:
* `FFTButterfly::process_inplace` is replaced by `Fft::process_inplace` or `Fft::process_inplace_with_scratch`
* `FFTButterfly::process_multi_inplace` is replaced by `Fft::process_inplace_multi`

Two FFT algorithms relied on the deleted trait: `MixedRadixDoubleButterfly` and `GoodThomasAlgorithmDoubleButterfly`. They took `FFTbutterfly` trait objects in their constructor. They've been renamed to `MixedRadixSmall` and `GoodThomasAlgorithmSmall` respectively, and take `Fft` trait objects in their constructor.

```rust
// RustFFT 4.0
let butterfly8 : Arc<dyn FFTbutterfly<T>> = Arc::new(Butterfly8::new(false));
let butterfly3 : Arc<dyn FFTbutterfly<T>> = Arc::new(Butterfly3::new(false));

let fft1 = MixedRadixDoubleButterfly::new(Arc::clone(&butterfly8), Arc::clone(&butterfly3));
let fft2 = GoodThomasAlgorithmDoubleButterfly::new(Arc::clone(&butterfly8), Arc::clone(&butterfly3));

// RustFFT 5.0
let butterfly8 : Arc<dyn Fft<T>> = Arc::new(Butterfly8::new(FftDirection::Forward));
let butterfly3 : Arc<dyn Fft<T>> = Arc::new(Butterfly3::new(FftDirection::Forward));

let fft1 = MixedRadixSmall::new(Arc::clone(&butterfly8), Arc::clone(&butterfly3));
let fft2 = GoodThomasAlgorithmSmall::new(Arc::clone(&butterfly8), Arc::clone(&butterfly3));
```
