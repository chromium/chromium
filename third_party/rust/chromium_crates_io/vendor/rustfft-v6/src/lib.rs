//! RustFFT is a high-performance FFT library written in pure Rust.
//!
//! On X86_64, RustFFT supports the AVX instruction set for increased performance. No special code is needed to activate AVX:
//! Simply plan a FFT using the FftPlanner on a machine that supports the `avx` and `fma` CPU features, and RustFFT
//! will automatically switch to faster AVX-accelerated algorithms.
//!
//! For machines that do not have AVX, RustFFT also supports the SSE4.1 instruction set.
//! As for AVX, this is enabled automatically when using the FftPlanner.
//!
//! Additionally, there is automatic support for the Neon instruction set on AArch64,
//! and support for WASM SIMD when compiling for WASM targets.
//!
//! ### Usage
//!
//! The recommended way to use RustFFT is to create a [`FftPlanner`] instance and then call its
//! [`plan_fft`](crate::FftPlanner::plan_fft) method. This method will automatically choose which FFT algorithms are best
//! for a given size and initialize the required buffers and precomputed data.
//!
//! ```
//! // Perform a forward FFT of size 1234
//! use rustfft::{FftPlanner, num_complex::Complex};
//!
//! let mut planner = FftPlanner::new();
//! let fft = planner.plan_fft_forward(1234);
//!
//! let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
//! fft.process(&mut buffer);
//! ```
//! The planner returns trait objects of the [`Fft`] trait, allowing for FFT sizes that aren't known
//! until runtime.
//!
//! RustFFT also exposes individual FFT algorithms. For example, if you know beforehand that you need a power-of-two FFT, you can
//! avoid the overhead of the planner and trait object by directly creating instances of the [`Radix4`](crate::algorithm::Radix4) algorithm:
//!
//! ```
//! // Computes a forward FFT of size 4096
//! use rustfft::{Fft, FftDirection, num_complex::Complex, algorithm::Radix4};
//!
//! let fft = Radix4::new(4096, FftDirection::Forward);
//!
//! let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 4096];
//! fft.process(&mut buffer);
//! ```
//!
//! For the vast majority of situations, simply using the [`FftPlanner`] will be enough, but
//! advanced users may have better insight than the planner into which algorithms are best for a specific size. See the
//! [`algorithm`] module for a complete list of scalar algorithms implemented by RustFFT.
//!
//! Users should beware, however, that bypassing the planner will disable all AVX, SSE, Neon, and WASM SIMD optimizations.
//!
//! ### Feature Flags
//!
//! * `avx` (Enabled by default)
//!
//!     On x86_64, the `avx` feature enables compilation of AVX-accelerated code. Enabling it greatly improves performance if the
//!     client CPU supports AVX and FMA, while disabling it reduces compile time and binary size.
//!
//!     On every platform besides x86_64, this feature does nothing, and RustFFT will behave like it's not set.
//! * `sse` (Enabled by default)
//!
//!     On x86_64, the `sse` feature enables compilation of SSE4.1-accelerated code. Enabling it improves performance
//!     if the client CPU supports SSE4.1, while disabling it reduces compile time and binary size. If AVX is also
//!     supported and its feature flag is enabled, RustFFT will use AVX instead of SSE4.1.
//!
//!     On every platform besides x86_64, this feature does nothing, and RustFFT will behave like it's not set.
//! * `neon` (Enabled by default)
//!
//!     On AArch64 (64-bit ARM) the `neon` feature enables compilation of Neon-accelerated code. Enabling it improves
//!     performance, while disabling it reduces compile time and binary size.
//!
//!     On every platform besides AArch64, this feature does nothing, and RustFFT will behave like it's not set.
//! * `wasm_simd` (Disabled by default)
//!
//!     On the WASM platform, this feature enables compilation of WASM SIMD accelerated code.
//!
//!     To execute binaries compiled with `wasm_simd`, you need a [target browser or runtime which supports `fixed-width SIMD`](https://webassembly.org/roadmap/).
//!     If you run your SIMD accelerated code on an unsupported platform, WebAssembly will specify a [trap](https://webassembly.github.io/spec/core/intro/overview.html#trap) leading to immediate execution cancelation.
//!
//!     On every platform besides WASM, this feature does nothing and RustFFT will behave like it is not set.
//!
//! ### Normalization
//!
//! RustFFT does not normalize outputs. Callers must manually normalize the results by scaling each element by
//! `1/len().sqrt()`. Multiple normalization steps can be merged into one via pairwise multiplication, so when
//! doing a forward FFT followed by an inverse callers can normalize once by scaling each element by `1/len()`
//!
//! ### Output Order
//!
//! Elements in the output are ordered by ascending frequency, with the first element corresponding to frequency 0.
//!
//! ### AVX Performance Tips
//!
//! In any FFT computation, the time required to compute a FFT of size N relies heavily on the [prime factorization](https://en.wikipedia.org/wiki/Integer_factorization) of N.
//! If N's prime factors are all very small, computing a FFT of size N will be fast, and it'll be slow if N has large prime
//! factors, or if N is a prime number.
//!
//! In most FFT libraries (Including RustFFT when using non-AVX code), power-of-two FFT sizes are the fastest, and users see a steep
//! falloff in performance when using non-power-of-two sizes. Thankfully, RustFFT using AVX acceleration is not quite as restrictive:
//!
//! - Any FFT whose size is of the form `2^n * 3^m` can be considered the "fastest" in RustFFT.
//! - Any FFT whose prime factors are all 11 or smaller will also be very fast, but the fewer the factors of 2 and 3 the slower it will be.
//!     For example, computing a FFT of size 13552 `(2^4*7*11*11)` is takes 12% longer to compute than 13824 `(2^9 * 3^3)`,
//!     and computing a FFT of size 2541 `(3*7*11*11)` takes 65% longer to compute than 2592 `(2^5 * 3^4)`
//! - Any other FFT size will be noticeably slower. A considerable amount of effort has been put into making these FFT sizes as fast as
//!     they can be, but some FFT sizes just take more work than others. For example, computing a FFT of size 5183 `(71 * 73)` takes about
//!     5x longer than computing a FFT of size 5184 `(2^6 * 3^4)`.
//!
//! In most cases, even prime-sized FFTs will be fast enough for your application. In the example of 5183 above, even that "slow" FFT
//! only takes a few tens of microseconds to compute.
//!
//! Some applications of the FFT allow for choosing an arbitrary FFT size (In many applications the size is pre-determined by whatever you're computing).
//! If your application supports choosing your own size, our advice is still to start by trying the size that's most convenient to your application.
//! If that's too slow, see if you can find a nearby size whose prime factors are all 11 or smaller, and you can expect a 2x-5x speedup.
//! If that's still too slow, find a nearby size whose prime factors are all 2 or 3, and you can expect a 1.1x-1.5x speedup.

use std::fmt::Display;

pub use num_complex;
pub use num_traits;

#[macro_use]
mod common;

/// Individual FFT algorithms
pub mod algorithm;
mod array_utils;
mod fft_cache;
mod fft_helper;
mod math_utils;
mod plan;
mod twiddles;

use num_complex::Complex;
use num_traits::Zero;

pub use crate::common::FftNum;
pub use crate::plan::{FftPlanner, FftPlannerScalar};

/// A trait that allows FFT algorithms to report their expected input/output size
pub trait Length {
    /// The FFT size that this algorithm can process
    fn len(&self) -> usize;
}

/// Represents a FFT direction, IE a forward FFT or an inverse FFT
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum FftDirection {
    Forward,
    Inverse,
}
impl FftDirection {
    /// Returns the opposite direction of `self`.
    ///
    ///  - If `self` is `FftDirection::Forward`, returns `FftDirection::Inverse`
    ///  - If `self` is `FftDirection::Inverse`, returns `FftDirection::Forward`
    #[inline]
    pub fn opposite_direction(&self) -> FftDirection {
        match self {
            Self::Forward => Self::Inverse,
            Self::Inverse => Self::Forward,
        }
    }
}
impl Display for FftDirection {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> Result<(), ::std::fmt::Error> {
        match self {
            Self::Forward => f.write_str("Forward"),
            Self::Inverse => f.write_str("Inverse"),
        }
    }
}

/// A trait that allows FFT algorithms to report whether they compute forward FFTs or inverse FFTs
pub trait Direction {
    /// Returns FftDirection::Forward if this instance computes forward FFTs, or FftDirection::Inverse for inverse FFTs
    fn fft_direction(&self) -> FftDirection;
}

/// Trait for algorithms that compute FFTs.
///
/// This trait has a few methods for computing FFTs. Its most conveinent method is [`process(slice)`](crate::Fft::process).
/// It takes in a slice of `Complex<T>` and computes a FFT on that slice, in-place. It may copy the data over to internal scratch buffers
/// if that speeds up the computation, but the output will always end up in the same slice as the input.
pub trait Fft<T: FftNum>: Length + Direction + Sync + Send {
    /// Computes a FFT in-place.
    ///
    /// Convenience method that allocates a `Vec` with the required scratch space and calls `self.process_with_scratch`.
    /// If you want to re-use that allocation across multiple FFT computations, consider calling `process_with_scratch` instead.
    ///
    /// # Panics
    ///
    /// This method panics if:
    /// - `buffer.len() % self.len() > 0`
    /// - `buffer.len() < self.len()`
    fn process(&self, buffer: &mut [Complex<T>]) {
        let mut scratch = vec![Complex::zero(); self.get_inplace_scratch_len()];
        self.process_with_scratch(buffer, &mut scratch);
    }

    /// Divides `buffer` into chunks of size `self.len()`, and computes a FFT on each chunk.
    ///
    /// Uses the `scratch` buffer as scratch space, so the contents of `scratch` should be considered garbage
    /// after calling.
    ///
    /// # Panics
    ///
    /// This method panics if:
    /// - `buffer.len() % self.len() > 0`
    /// - `buffer.len() < self.len()`
    /// - `scratch.len() < self.get_inplace_scratch_len()`
    fn process_with_scratch(&self, buffer: &mut [Complex<T>], scratch: &mut [Complex<T>]);

    /// Divides `input` and `output` into chunks of size `self.len()`, and computes a FFT on each chunk.
    ///
    /// This method uses both the `input` buffer and `scratch` buffer as scratch space, so the contents of both should be
    /// considered garbage after calling.
    ///
    /// This is a more niche way of computing a FFT. It's useful to avoid a `copy_from_slice()` if you need the output
    /// in a different buffer than the input for some reason. This happens frequently in RustFFT internals, but is probably
    /// less common among RustFFT users.
    ///
    /// For many FFT sizes, `self.get_outofplace_scratch_len()` returns 0
    ///
    /// # Panics
    ///
    /// This method panics if:
    /// - `output.len() != input.len()`
    /// - `input.len() % self.len() > 0`
    /// - `input.len() < self.len()`
    /// - `scratch.len() < self.get_outofplace_scratch_len()`
    fn process_outofplace_with_scratch(
        &self,
        input: &mut [Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    );

    /// Divides `input` and `output` into chunks of `self.len()`, and computes a FFT on each chunk while
    /// keeping `input` untouched.
    ///
    /// This method uses the `scratch` buffer as scratch space, so the contents should be considered garbage after calling.
    ///
    /// # Panics
    ///
    /// This method panics if:
    /// - `output.len() ! input.len()`
    /// - `input.len() % self.len() > 0`
    /// - `input.len() < self.len()`
    /// - `scratch.len() < get_immutable_scratch_len()`
    fn process_immutable_with_scratch(
        &self,
        input: &[Complex<T>],
        output: &mut [Complex<T>],
        scratch: &mut [Complex<T>],
    );

    /// Returns the size of the scratch buffer required by `process_with_scratch`
    ///
    /// For most FFT sizes, this method will return `self.len()`. For a few small sizes it will return 0, and for some special FFT sizes
    /// (Sizes that require the use of Bluestein's Algorithm), this may return a scratch size larger than `self.len()`.
    /// The returned value may change from one version of RustFFT to the next.
    fn get_inplace_scratch_len(&self) -> usize;

    /// Returns the size of the scratch buffer required by `process_outofplace_with_scratch`
    ///
    /// For most FFT sizes, this method will return 0. For some special FFT sizes
    /// (Sizes that require the use of Bluestein's Algorithm), this may return a scratch size larger than `self.len()`.
    /// The returned value may change from one version of RustFFT to the next.
    fn get_outofplace_scratch_len(&self) -> usize;

    /// Returns the size of the scratch buffer required by `process_immutable_with_scratch`
    ///
    /// For most FFT sizes, this method will return something between self.len() and self.len() * 2.
    /// For a few small sizes it will return 0, and for some special FFT sizes
    /// (Sizes that require the use of Bluestein's Algorithm), this may return a scratch size larger than `self.len()`.
    /// The returned value may change from one version of RustFFT to the next.
    fn get_immutable_scratch_len(&self) -> usize;
}

// Algorithms implemented to use AVX instructions. Only compiled on x86_64, and only compiled if the "avx" feature flag is set.
#[cfg(all(target_arch = "x86_64", feature = "avx"))]
mod avx;

// If we're not on x86_64, or if the "avx" feature was disabled, keep a stub implementation around that has the same API, but does nothing
// That way, users can write code using the AVX planner and compile it on any platform
#[cfg(not(all(target_arch = "x86_64", feature = "avx")))]
mod avx {
    pub mod avx_planner {
        use crate::{Fft, FftDirection, FftNum};
        use std::sync::Arc;

        /// The AVX FFT planner creates new FFT algorithm instances which take advantage of the AVX instruction set.
        ///
        /// Creating an instance of `FftPlannerAvx` requires the `avx` and `fma` instructions to be available on the current machine, and it requires RustFFT's
        ///  `avx` feature flag to be set. A few algorithms will use `avx2` if it's available, but it isn't required.
        ///
        /// For the time being, AVX acceleration is black box, and AVX accelerated algorithms are not available without a planner. This may change in the future.
        ///
        /// ~~~
        /// // Perform a forward Fft of size 1234, accelerated by AVX
        /// use std::sync::Arc;
        /// use rustfft::{FftPlannerAvx, num_complex::Complex};
        ///
        /// // If FftPlannerAvx::new() returns Ok(), we'll know AVX algorithms are available
        /// // on this machine, and that RustFFT was compiled with the `avx` feature flag
        /// if let Ok(mut planner) = FftPlannerAvx::new() {
        ///     let fft = planner.plan_fft_forward(1234);
        ///
        ///     let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
        ///     fft.process(&mut buffer);
        ///
        ///     // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
        ///     // where T is the numeric type, ie f32 or f64, so it's cheap to clone
        ///     let fft_clone = Arc::clone(&fft);
        /// }
        /// ~~~
        ///
        /// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
        /// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
        /// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
        /// by a different planner)
        ///
        /// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
        /// safe to drop the planner after creating Fft instances.
        pub struct FftPlannerAvx<T: FftNum> {
            _phantom: std::marker::PhantomData<T>,
        }
        impl<T: FftNum> FftPlannerAvx<T> {
            /// Constructs a new `FftPlannerAvx` instance.
            ///
            /// Returns `Ok(planner_instance)` if this machine has the required instruction sets and the `avx` feature flag is set.
            /// Returns `Err(())` if some instruction sets are missing, or if the `avx` feature flag is not set.
            pub fn new() -> Result<Self, ()> {
                Err(())
            }
            /// Returns a `Fft` instance which uses AVX instructions to compute FFTs of size `len`.
            ///
            /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft(&mut self, _len: usize, _direction: FftDirection) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses AVX instructions to compute forward FFTs of size `len`.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_forward(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses AVX instructions to compute inverse FFTs of size `len.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_inverse(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
        }
    }
}

pub use self::avx::avx_planner::FftPlannerAvx;

// Algorithms implemented to use SSE4.1 instructions. Only compiled on x86_64, and only compiled if the "sse" feature flag is set.
#[cfg(all(target_arch = "x86_64", feature = "sse"))]
mod sse;

// If we're not on x86_64, or if the "sse" feature was disabled, keep a stub implementation around that has the same API, but does nothing
// That way, users can write code using the SSE planner and compile it on any platform
#[cfg(not(all(target_arch = "x86_64", feature = "sse")))]
mod sse {
    pub mod sse_planner {
        use crate::{Fft, FftDirection, FftNum};
        use std::sync::Arc;

        /// The SSE FFT planner creates new FFT algorithm instances using a mix of scalar and SSE accelerated algorithms.
        /// It requires at least SSE4.1, which is available on all reasonably recent x86_64 cpus.
        ///
        /// RustFFT has several FFT algorithms available. For a given FFT size, the `FftPlannerSse` decides which of the
        /// available FFT algorithms to use and then initializes them.
        ///
        /// ~~~
        /// // Perform a forward Fft of size 1234
        /// use std::sync::Arc;
        /// use rustfft::{FftPlannerSse, num_complex::Complex};
        ///
        /// if let Ok(mut planner) = FftPlannerSse::new() {
        ///   let fft = planner.plan_fft_forward(1234);
        ///
        ///   let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
        ///   fft.process(&mut buffer);
        ///
        ///   // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
        ///   // where T is the numeric type, ie f32 or f64, so it's cheap to clone
        ///   let fft_clone = Arc::clone(&fft);
        /// }
        /// ~~~
        ///
        /// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
        /// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
        /// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
        /// by a different planner)
        ///
        /// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
        /// safe to drop the planner after creating Fft instances.
        pub struct FftPlannerSse<T: FftNum> {
            _phantom: std::marker::PhantomData<T>,
        }
        impl<T: FftNum> FftPlannerSse<T> {
            /// Creates a new `FftPlannerSse` instance.
            ///
            /// Returns `Ok(planner_instance)` if this machine has the required instruction sets.
            /// Returns `Err(())` if some instruction sets are missing.
            pub fn new() -> Result<Self, ()> {
                Err(())
            }
            /// Returns a `Fft` instance which uses SSE4.1 instructions to compute FFTs of size `len`.
            ///
            /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft(&mut self, _len: usize, _direction: FftDirection) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses SSE4.1 instructions to compute forward FFTs of size `len`.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_forward(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses SSE4.1 instructions to compute inverse FFTs of size `len.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_inverse(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
        }
    }
}

pub use self::sse::sse_planner::FftPlannerSse;

// Algorithms implemented to use Neon instructions. Only compiled on AArch64, and only compiled if the "neon" feature flag is set.
#[cfg(all(target_arch = "aarch64", feature = "neon"))]
mod neon;

// If we're not on AArch64, or if the "neon" feature was disabled, keep a stub implementation around that has the same API, but does nothing
// That way, users can write code using the Neon planner and compile it on any platform
#[cfg(not(all(target_arch = "aarch64", feature = "neon")))]
mod neon {
    pub mod neon_planner {
        use crate::{Fft, FftDirection, FftNum};
        use std::sync::Arc;

        /// The Neon FFT planner creates new FFT algorithm instances using a mix of scalar and Neon accelerated algorithms.
        /// It is supported when using the 64-bit AArch64 instruction set.
        ///
        /// RustFFT has several FFT algorithms available. For a given FFT size, the `FftPlannerNeon` decides which of the
        /// available FFT algorithms to use and then initializes them.
        ///
        /// ~~~
        /// // Perform a forward Fft of size 1234
        /// use std::sync::Arc;
        /// use rustfft::{FftPlannerNeon, num_complex::Complex};
        ///
        /// if let Ok(mut planner) = FftPlannerNeon::new() {
        ///   let fft = planner.plan_fft_forward(1234);
        ///
        ///   let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
        ///   fft.process(&mut buffer);
        ///
        ///   // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
        ///   // where T is the numeric type, ie f32 or f64, so it's cheap to clone
        ///   let fft_clone = Arc::clone(&fft);
        /// }
        /// ~~~
        ///
        /// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
        /// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
        /// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
        /// by a different planner)
        ///
        /// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
        /// safe to drop the planner after creating Fft instances.
        pub struct FftPlannerNeon<T: FftNum> {
            _phantom: std::marker::PhantomData<T>,
        }
        impl<T: FftNum> FftPlannerNeon<T> {
            /// Creates a new `FftPlannerNeon` instance.
            ///
            /// Returns `Ok(planner_instance)` if this machine has the required instruction sets.
            /// Returns `Err(())` if some instruction sets are missing.
            pub fn new() -> Result<Self, ()> {
                Err(())
            }
            /// Returns a `Fft` instance which uses Neon instructions to compute FFTs of size `len`.
            ///
            /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft(&mut self, _len: usize, _direction: FftDirection) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses Neon instructions to compute forward FFTs of size `len`.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_forward(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses Neon instructions to compute inverse FFTs of size `len.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_inverse(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
        }
    }
}

pub use self::neon::neon_planner::FftPlannerNeon;

#[cfg(all(target_arch = "wasm32", feature = "wasm_simd"))]
mod wasm_simd;

// If we're not compiling to WebAssembly, or if the "wasm_simd" feature was disabled, keep a stub implementation around that has the same API, but does nothing
// That way, users can write code using the WASM planner and compile it on any platform
#[cfg(not(all(target_arch = "wasm32", feature = "wasm_simd")))]
mod wasm_simd {
    pub mod wasm_simd_planner {
        use crate::{Fft, FftDirection, FftNum};
        use std::sync::Arc;

        /// The WASM FFT planner creates new FFT algorithm instances using a mix of scalar and WASM SIMD accelerated algorithms.
        /// It is supported when using fairly recent browser versions as outlined in [the WebAssembly roadmap](https://webassembly.org/roadmap/).
        ///
        /// RustFFT has several FFT algorithms available. For a given FFT size, `FftPlannerWasmSimd` decides which of the
        /// available FFT algorithms to use and then initializes them.
        ///
        /// ~~~
        /// // Perform a forward Fft of size 1234
        /// use std::sync::Arc;
        /// use rustfft::{FftPlannerWasmSimd, num_complex::Complex};
        ///
        /// if let Ok(mut planner) = FftPlannerWasmSimd::new() {
        ///   let fft = planner.plan_fft_forward(1234);
        ///
        ///   let mut buffer = vec![Complex{ re: 0.0f32, im: 0.0f32 }; 1234];
        ///   fft.process(&mut buffer);
        ///
        ///   // The FFT instance returned by the planner has the type `Arc<dyn Fft<T>>`,
        ///   // where T is the numeric type, ie f32 or f64, so it's cheap to clone
        ///   let fft_clone = Arc::clone(&fft);
        /// }
        /// ~~~
        ///
        /// If you plan on creating multiple FFT instances, it is recommended to reuse the same planner for all of them. This
        /// is because the planner re-uses internal data across FFT instances wherever possible, saving memory and reducing
        /// setup time. (FFT instances created with one planner will never re-use data and buffers with FFT instances created
        /// by a different planner)
        ///
        /// Each FFT instance owns [`Arc`s](std::sync::Arc) to its internal data, rather than borrowing it from the planner, so it's perfectly
        /// safe to drop the planner after creating Fft instances.
        pub struct FftPlannerWasmSimd<T: FftNum> {
            _phantom: std::marker::PhantomData<T>,
        }
        impl<T: FftNum> FftPlannerWasmSimd<T> {
            /// Creates a new `FftPlannerWasmSimd` instance.
            ///
            /// Returns `Ok(planner_instance)` if this machine has the required instruction sets.
            /// Returns `Err(())` if some instruction sets are missing.
            pub fn new() -> Result<Self, ()> {
                Err(())
            }
            /// Returns a `Fft` instance which uses WebAssembly SIMD instructions to compute FFTs of size `len`.
            ///
            /// If the provided `direction` is `FftDirection::Forward`, the returned instance will compute forward FFTs. If it's `FftDirection::Inverse`, it will compute inverse FFTs.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft(&mut self, _len: usize, _direction: FftDirection) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses WebAssembly SIMD instructions to compute forward FFTs of size `len`.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_forward(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
            /// Returns a `Fft` instance which uses WebAssembly SIMD instructions to compute inverse FFTs of size `len.
            ///
            /// If this is called multiple times, the planner will attempt to re-use internal data between calls, reducing memory usage and FFT initialization time.
            pub fn plan_fft_inverse(&mut self, _len: usize) -> Arc<dyn Fft<T>> {
                unreachable!()
            }
        }
    }
}

pub use self::wasm_simd::wasm_simd_planner::FftPlannerWasmSimd;

#[cfg(test)]
mod test_utils;
