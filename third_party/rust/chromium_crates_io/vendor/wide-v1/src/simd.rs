/// Emits functionality shared by all SIMD types.
///
/// Functions that need a separate implementation for each type (for
/// performance) use `$fn_{name}:item` syntax, and functions that have one
/// shared implementation are written out normally inside this macro.
///
/// `impl_simd` is not invoked directly from each SIMD type module.
/// Instead, type modules invoke the `simd_float`, `simd_int` and `simd_uint`
/// macros, which invoke this macro. This allows avoiding boilerplate for
/// functions that have one shared implementation for one numerical category
/// (float, int, uint), but not one implementation for *all* SIMD types.
macro_rules! impl_simd {
  (
    // SAFETY: The contents of this macro assume that:
    //
    // - `T` implements `Pod`
    // - `Pod` can be implemented for `Simd`
    // - `size_of::<Simd>()` is `size_of::<T>() * N`
    // - `align_of::<Simd>()` is `size_of::<Simd>()`
    // - `Pod` can be implemented for the optional native SIMD types
    unsafe {
      T = $T:ident,
      N = $N:literal,
      Simd = $Simd:ident,
      UintSimd = $UintSimd:ident,
      optional_type_x86_inner { $(X86Inner = $X86Inner:ident)? },
      optional_type_arm_inner { $(ArmInner = $ArmInner:ident)? },
      optional_type_wasm_inner { $(WasmInner = $WasmInner:ident)? },
    }

    $fn_simd_eq:item
    $fn_simd_ne:item
    $fn_simd_lt:item
    $fn_simd_gt:item
    $fn_simd_le:item
    $fn_simd_ge:item
    $fn_reduce_add:item
    $fn_reduce_mul:item
    $fn_bitselect:item
    $fn_select:item
    $fn_to_bitmask:item
    $fn_any:item
    $fn_all:item
    $fn_shuffle:item
    $fn_shuffle_zeroing:item
    $fn_shuffle_wrapping:item
    $fn_shuffle_2:item
    $fn_shuffle_zeroing_2:item
    $fn_shuffle_wrapping_2:item
    $fn_shuffle_3:item
    $fn_shuffle_zeroing_3:item
    $fn_shuffle_wrapping_3:item
    $fn_shuffle_4:item
    $fn_shuffle_zeroing_4:item
    $fn_shuffle_wrapping_4:item
    $fn_transpose:item
    optional_fn_deserialize { $($fn_deserialize:item)? }
  ) => {
    impl From<[$T; $N]> for $Simd {
      /// Converts an array to a SIMD vector.
      #[inline]
      fn from(arr: [$T; $N]) -> Self {
        Self::new(arr)
      }
    }

    impl From<$Simd> for [$T; $N] {
      /// Converts a SIMD vector to an array.
      #[inline]
      fn from(simd: $Simd) -> Self {
        simd.to_array()
      }
    }

    impl From<$T> for $Simd {
      /// Converts a single value to a SIMD vector by setting all elements to
      /// that value.
      #[inline]
      fn from(elem: $T) -> Self {
        Self::splat(elem)
      }
    }

    impl From<&[$T]> for $Simd {
      /// Converts a slice to a SIMD vector, filling in zeros if there are not
      /// enough elements, and panicking if there are too many elements.
      ///
      /// Note that in the future, handling of too many elements may change.
      #[inline]
      fn from(value: &[$T]) -> Self {
        assert!(
          value.len() <= $N,
          concat!(
            "slice has more elements than `",
            stringify!($Simd),
            "` can store",
          ),
        );

        // SAFETY: `$Simd` accepts all bit-patterns, including all zeros.
        let mut result = unsafe { core::mem::zeroed::<$Simd>() };

        // SAFETY: `value` is valid for its own length, and `result` is valid
        // because its length is checked to be less than or equal to
        // `value.len()`. Both pointers are properly aligned because they
        // originate from a slice of `$T`. The regions of memory do not overlap
        // because they originate from a shared reference and a mutable
        // reference.
        unsafe {
          core::ptr::copy_nonoverlapping::<$T>(
            value.as_ptr(),
            result.as_mut_array().as_mut_ptr(),
            value.len()
          );
        }

        result
      }
    }

    $(
      #[cfg(target_arch = "x86")]
      impl From<core::arch::x86::$X86Inner> for $Simd {
        /// Converts a native intrinsics SIMD type to a high-level SIMD type.
        #[inline]
        fn from(value: core::arch::x86::$X86Inner) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<core::arch::x86::$X86Inner, $Simd>(value) }
        }
      }

      #[cfg(target_arch = "x86")]
      impl From<$Simd> for core::arch::x86::$X86Inner {
        /// Converts a high-level SIMD type to a native intrinsics SIMD type.
        #[inline]
        fn from(value: $Simd) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<$Simd, core::arch::x86::$X86Inner>(value) }
        }
      }

      #[cfg(target_arch = "x86_64")]
      impl From<core::arch::x86_64::$X86Inner> for $Simd {
        /// Converts a native intrinsics SIMD type to a high-level SIMD type.
        #[inline]
        fn from(value: core::arch::x86_64::$X86Inner) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<core::arch::x86_64::$X86Inner, $Simd>(value) }
        }
      }

      #[cfg(target_arch = "x86_64")]
      impl From<$Simd> for core::arch::x86_64::$X86Inner {
        /// Converts a high-level SIMD type to a native intrinsics SIMD type.
        #[inline]
        fn from(value: $Simd) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<$Simd, core::arch::x86_64::$X86Inner>(value) }
        }
      }
    )?
    $(
      #[cfg(target_arch = "aarch64")]
      impl From<core::arch::aarch64::$ArmInner> for $Simd {
        /// Converts a native intrinsics SIMD type to a high-level SIMD type.
        #[inline]
        fn from(value: core::arch::aarch64::$ArmInner) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<core::arch::aarch64::$ArmInner, $Simd>(value) }
        }
      }

      #[cfg(target_arch = "aarch64")]
      impl From<$Simd> for core::arch::aarch64::$ArmInner {
        /// Converts a high-level SIMD type to a native intrinsics SIMD type.
        #[inline]
        fn from(value: $Simd) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<$Simd, core::arch::aarch64::$ArmInner>(value) }
        }
      }
    )?
    $(
      #[cfg(target_arch = "wasm32")]
      impl From<core::arch::wasm32::$WasmInner> for $Simd {
        /// Converts a native intrinsics SIMD type to a high-level SIMD type.
        #[inline]
        fn from(value: core::arch::wasm32::$WasmInner) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<core::arch::wasm32::$WasmInner, $Simd>(value) }
        }
      }

      #[cfg(target_arch = "wasm32")]
      impl From<$Simd> for core::arch::wasm32::$WasmInner {
        /// Converts a high-level SIMD type to a native intrinsics SIMD type.
        #[inline]
        fn from(value: $Simd) -> Self {
          // SAFETY: Both types are expected to accept all bit-patterns and to
          // only contain initialized memory.
          unsafe { core::mem::transmute::<$Simd, core::arch::wasm32::$WasmInner>(value) }
        }
      }
    )?

    macro_rules! impl_formatting_trait {
      ($Trait:path) => {
        impl $Trait for $Simd {
          #[allow(clippy::missing_inline_in_public_items)]
          fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
            write!(f, "(")?;
            for (i, x) in self.to_array().iter().enumerate() {
              if i > 0 {
                write!(f, ", ")?;
              }
              <$T as $Trait>::fmt(x, f)?;
            }
            write!(f, ")")
          }
        }
      }
    }
    impl_formatting_trait!(core::fmt::Debug);
    impl_formatting_trait!(core::fmt::Display);
    impl_formatting_trait!(core::fmt::LowerExp);
    impl_formatting_trait!(core::fmt::UpperExp);

    impl Select<$Simd> for $Simd {
      $fn_select
    }

    impl ShuffleExt for [$Simd; 2] {
      type Indices = $UintSimd;
      type Output = $Simd;

      $fn_shuffle_2

      $fn_shuffle_zeroing_2

      $fn_shuffle_wrapping_2
    }

    impl ShuffleExt for [$Simd; 3] {
      type Indices = $UintSimd;
      type Output = $Simd;

      $fn_shuffle_3

      $fn_shuffle_zeroing_3

      $fn_shuffle_wrapping_3
    }

    impl ShuffleExt for [$Simd; 4] {
      type Indices = $UintSimd;
      type Output = $Simd;

      $fn_shuffle_4

      $fn_shuffle_zeroing_4

      $fn_shuffle_wrapping_4
    }

    #[expect(deprecated)]
    impl CmpEq for $Simd {
      type Output = Self;

      $fn_simd_eq
    }

    #[expect(deprecated)]
    impl CmpEq<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_eq(self, rhs: $T) -> Self::Output {
        self.simd_eq(Self::splat(rhs))
      }
    }

    #[expect(deprecated)]
    impl CmpNe for $Simd {
      type Output = Self;

      $fn_simd_ne
    }

    #[expect(deprecated)]
    impl CmpNe<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_ne(self, rhs: $T) -> Self::Output {
        self.simd_ne(Self::splat(rhs))
      }
    }

    #[expect(deprecated)]
    impl CmpLt for $Simd {
      type Output = Self;

      $fn_simd_lt
    }

    #[expect(deprecated)]
    impl CmpLt<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_lt(self, rhs: $T) -> Self::Output {
        self.simd_lt(Self::splat(rhs))
      }
    }

    #[expect(deprecated)]
    impl CmpGt for $Simd {
      type Output = Self;

      $fn_simd_gt
    }

    #[expect(deprecated)]
    impl CmpGt<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_gt(self, rhs: $T) -> Self::Output {
        self.simd_gt(Self::splat(rhs))
      }
    }

    #[expect(deprecated)]
    impl CmpLe for $Simd {
      type Output = Self;

      $fn_simd_le
    }

    #[expect(deprecated)]
    impl CmpLe<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_le(self, rhs: $T) -> Self::Output {
        self.simd_le(Self::splat(rhs))
      }
    }

    #[expect(deprecated)]
    impl CmpGe for $Simd {
      type Output = Self;

      $fn_simd_ge
    }

    #[expect(deprecated)]
    impl CmpGe<$T> for $Simd {
      type Output = Self;

      #[inline]
      fn simd_ge(self, rhs: $T) -> Self::Output {
        self.simd_ge(Self::splat(rhs))
      }
    }

    impl AlignTo for $Simd {
      type Elem = $T;
    }

    impl<const N: usize> Sealed for [$Simd; N] {}

    /// The following functionality exists for all SIMD vectors.
    impl $Simd {
      /// Converts an array to a SIMD vector.
      #[inline]
      #[must_use]
      pub const fn new(array: [$T; $N]) -> Self {
        unsafe { core::mem::transmute::<[$T; $N], $Simd>(array) }
      }

      /// Constructs a new SIMD vector with all elements set to the given value.
      #[inline]
      #[must_use]
      pub const fn splat(elem: $T) -> Self {
        unsafe { core::mem::transmute::<[$T; $N], $Simd>([elem; $N]) }
      }

      /// Converts a SIMD vector to an array.
      #[inline]
      #[must_use]
      pub const fn to_array(self) -> [$T; $N] {
        // SAFETY: Both types accept all bit-patterns and only contain
        // initialized memory.
        unsafe { core::mem::transmute::<$Simd, [$T; $N]>(self) }
      }

      /// Returns an array reference containing the entire SIMD vector.
      #[inline]
      #[must_use]
      pub const fn as_array(&self) -> &[$T; $N] {
        // SAFETY: The input type has greater alignment than the output type,
        // and both pointed-at types have the same size, accept all bit-patterns
        // and only contain initialized memory.
        unsafe { core::mem::transmute::<&$Simd, &[$T; $N]>(self) }
      }

      /// Returns a mutable array reference containing the entire SIMD vector.
      #[inline]
      #[must_use]
      pub const fn as_mut_array(&mut self) -> &mut [$T; $N] {
        // SAFETY: The input type has greater alignment than the output type,
        // and both pointed-at types have the same size, accept all bit-patterns
        // and only contain initialized memory.
        unsafe { core::mem::transmute::<&mut $Simd, &mut [$T; $N]>(self) }
      }

      /// Creates a SIMD vector from a native-endian byte array.
      ///
      /// The bytes are interpreted as the raw memory representation of the
      /// vector. This is equivalent to the `from_ne_bytes` functions on the
      /// primitive scalar types, but operates on the entire vector at once.
      ///
      /// This is the inverse of [`to_ne_bytes`].
      ///
      /// [`to_ne_bytes`]: Self::to_ne_bytes
      #[inline]
      #[must_use]
      pub const fn from_ne_bytes(bytes: [u8; size_of::<$Simd>()]) -> Self {
        // SAFETY: `$Simd` accepts all bit-patterns and only contains
        // initialized memory, and the byte array has the same size.
        unsafe { core::mem::transmute::<[u8; size_of::<$Simd>()], $Simd>(bytes) }
      }

      /// Returns the raw memory representation of `self` as a native-endian
      /// byte array.
      ///
      /// This is the inverse of [`from_ne_bytes`].
      ///
      /// [`from_ne_bytes`]: Self::from_ne_bytes
      #[inline]
      #[must_use]
      pub const fn to_ne_bytes(self) -> [u8; size_of::<$Simd>()] {
        // SAFETY: `$Simd` accepts all bit-patterns and only contains
        // initialized memory, and the byte array has the same size.
        unsafe { core::mem::transmute::<$Simd, [u8; size_of::<$Simd>()]>(self) }
      }

      /// Returns a [mask] that checks if each element of `self` is equal to the
      /// corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_eq<Rhs>(self, other: Rhs) -> <Self as CmpEq<Rhs>>::Output
      where
        Self: CmpEq<Rhs>,
      {
        CmpEq::simd_eq(self, other)
      }

      /// Returns a [mask] that checks if each element of `self` is not equal to
      /// the corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_ne<Rhs>(self, other: Rhs) -> <Self as CmpNe<Rhs>>::Output
      where
        Self: CmpNe<Rhs>,
      {
        CmpNe::simd_ne(self, other)
      }

      /// Returns a [mask] that checks if each element of `self` is less than
      /// the corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_lt<Rhs>(self, other: Rhs) -> <Self as CmpLt<Rhs>>::Output
      where
        Self: CmpLt<Rhs>,
      {
        CmpLt::simd_lt(self, other)
      }

      /// Returns a [mask] that checks if each element of `self` is greater than
      /// the corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_gt<Rhs>(self, other: Rhs) -> <Self as CmpGt<Rhs>>::Output
      where
        Self: CmpGt<Rhs>,
      {
        CmpGt::simd_gt(self, other)
      }

      /// Returns a [mask] that checks if each element of `self` is less than or
      /// equal to the corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_le<Rhs>(self, other: Rhs) -> <Self as CmpLe<Rhs>>::Output
      where
        Self: CmpLe<Rhs>,
      {
        CmpLe::simd_le(self, other)
      }

      /// Returns a [mask] that checks if each element of `self` is greater than
      /// or equal to the corresponding element of `other`.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[expect(deprecated)]
      pub fn simd_ge<Rhs>(self, other: Rhs) -> <Self as CmpGe<Rhs>>::Output
      where
        Self: CmpGe<Rhs>,
      {
        CmpGe::simd_ge(self, other)
      }

      /// Reducing addition. Returns the sum of the vector's elements.
      ///
      /// Equivalent to `self[0] + self[1] + ...`.
      #[must_use]
      $fn_reduce_add

      /// Reducing multiplication. Returns the product of the vector's elements.
      ///
      /// Equivalent to `self[0] * self[1] * ...`.
      #[must_use]
      $fn_reduce_mul

      /// Bitwise selection.
      ///
      /// For each bit of `self`:
      ///
      /// - If the bit is one, return the corresponding bit of `if_one`
      /// - If the bit is zero, return the corresponding bit of `if_zero`
      ///
      /// If you know `self` is a [mask], meaning each element is either all
      /// zeros or all ones, consider using [`select`] which is faster.
      ///
      /// [mask]: crate#masks
      /// [`select`]: Self::select
      #[must_use]
      $fn_bitselect

      /// Lanewise SIMD selection.
      ///
      /// For each lane:
      ///
      /// - If all bits of `self` are one, return the corresponding lane of
      ///   `if_true`
      /// - If all bits of `self` are zero, return the corresponding lane of
      ///   `if_false`
      ///
      /// This function assumes `self` is a [mask], meaning each element is
      /// either all zeros or all ones. For bitwise selection use [`bitselect`].
      ///
      /// In addition to selecting SIMD vectors, this method can be used to
      /// select composite types (e.g., a downstream `Vec3<f32x4>` structure)
      /// if you implement `Select<Composite> for Simd`.
      ///
      /// [mask]: crate#masks
      /// [`bitselect`]: Self::bitselect
      #[inline]
      #[must_use]
      pub fn select<Output>(self, if_true: Output, if_false: Output) -> Output
      where
        Self: Select<Output>,
      {
        Select::select(self, if_true, if_false)
      }

      /// Converts to a bitmask, where each bit is `1` if the element of `self`
      /// is true or `0` if the element of `self` is false.
      ///
      /// Each bit of the output corresponds to an element of `self`. The least
      /// significant bit corresponds to the lowest element. Remaining bits are
      /// `0`.
      ///
      /// This function assumes `self` is a [mask], meaning each element is
      /// either all zeros or all ones. If the input is not a mask, the result
      /// is unspecified.
      ///
      /// [mask]: crate#masks
      #[must_use]
      #[doc(alias("movemask", "move_mask"))]
      $fn_to_bitmask

      /// Returns `true` if any element of `self` is true.
      ///
      /// This function assumes `self` is a [mask], meaning each element is
      /// either all zeros or all ones. If the input is not a mask, the result
      /// is unspecified.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_any

      /// Returns `true` if all elements of `self` are true.
      ///
      /// This function assumes `self` is a [mask], meaning each element is
      /// either all zeros or all ones. If the input is not a mask, the result
      /// is unspecified.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_all

      /// Returns `true` if none of the elements of `self` are true.
      ///
      /// This function assumes `self` is a [mask], meaning each element is
      /// either all zeros or all ones. If the input is not a mask, the result
      /// is unspecified.
      ///
      /// [mask]: crate#masks
      #[inline]
      #[must_use]
      pub fn none(self) -> bool {
        !self.any()
      }

      /// Returns a SIMD vector whose elements are selected from `self` using
      /// the corresponding runtime `indices`.
      ///
      /// If an index is out of bounds, the corresponding result element is
      /// unspecified.
      ///
      /// Equivalent to
      /// `[self[indices[0]], self[indices[1]], ..., self[[indices[N - 1]]]]`.
      ///
      /// # Type-specific guarantees
      ///
      /// For [`u8x16`] and [`i8x16`], it is guaranteed that for out of bounds
      /// indices, if the high bit of an index is set, zero is returned.
      /// Otherwise, either zero is returned or the index wraps around,
      /// non-deterministically.
      ///
      /// For all other 8-bit types, it is guaranteed that for out of bounds
      /// indices, either zero is returned or the index wraps around,
      /// non-deterministically (unlike other types, which can return arbitrary
      /// values).
      #[must_use]
      $fn_shuffle

      /// Returns a SIMD vector whose elements are selected from `self` using
      /// the corresponding runtime `indices`.
      ///
      /// If an index is out of bounds, the corresponding result element is
      /// the number zero.
      ///
      /// Equivalent to
      /// `[self[indices[0]], self[indices[1]], ..., self[[indices[N - 1]]]]`
      /// with a zero fallback.
      #[must_use]
      $fn_shuffle_zeroing

      /// Returns a SIMD vector whose elements are selected from `self` using
      /// the corresponding runtime `indices`.
      ///
      /// Indices are wrapped by the number of elements in `self`.
      ///
      /// Equivalent to
      /// `[self[indices[0] % N], self[indices[1] % N], ..., self[[indices[N - 1] % N]]]`.
      #[must_use]
      $fn_shuffle_wrapping

      /// Transposes an array of SIMD vectors interpreted as a square matrix.
      #[must_use]
      $fn_transpose

      /// Elementwise selection.
      ///
      /// For each element of `self`:
      ///
      /// - If all bits are one, return the corresponding element of `if_true`
      /// - If all bits are zero, return the corresponding element of `if_false`
      ///
      /// Originally this function did not specify whether it supported per-bit
      /// selection, or if it assumed `self` is a [mask], meaning each element
      /// is either all zeros or all ones (for better performance). Because of
      /// this, [`blend`] has been split into two new functions: [`select`] and
      /// [`bitselect`].
      ///
      /// [mask]: crate#masks
      /// [`blend`]: Self::blend
      /// [`select`]: Self::select
      /// [`bitselect`]: Self::bitselect
      #[deprecated(
        since = "1.6.0",
        note = "split into `select` and `bitselect` functions"
      )]
      #[inline]
      #[must_use]
      pub fn blend(self, if_true: Self, if_false: Self) -> Self {
        self.select(if_true, if_false)
      }
    }

    mod bytemuck {
      use bytemuck::{Pod, Zeroable};

      use crate::$Simd;

      // SAFETY: All SIMD types in this library contain fully initialized memory
      // and accept all bits patterns.
      unsafe impl Zeroable for $Simd {}

      // SAFETY: All SIMD types in this library contain fully initialized memory
      // and accept all bits patterns.
      unsafe impl Pod for $Simd {}
    }

    #[cfg(feature = "serde")]
    mod serde {
      use serde_core::{
        Deserialize, Serialize,
        ser::SerializeTuple,
      };

      use crate::$Simd;

      impl Serialize for $Simd {
        #[inline]
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
          S: serde_core::Serializer,
        {
          let array = self.as_array();
          let mut seq = serializer.serialize_tuple($N)?;
          for e in array {
            seq.serialize_element(e)?;
          }
          seq.end()
        }
      }

      impl_optional_deserialize!{
        T = $T,
        N = $N,
        Simd = $Simd,
        $($fn_deserialize)?
      }
    }
  };
}

macro_rules! impl_unary_operator {
  ($Simd:ident, $Op:ident, $op:ident, $impl:item $(, $(#[$doc:meta])*)?) => {
    impl $Op for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      $impl
    }

    impl $Op for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self) -> Self::Output {
        (*self).$op()
      }
    }
  }
}

macro_rules! impl_binary_operator {
  (
    $T:ident,
    $Simd:ident,
    $Op:ident,
    $op:ident,
    $OpAssign:ident,
    $op_assign:ident,
    $impl:item
    $(,
      $(#[$doc:meta])*,
      $(#[$doc_scalar:meta])*,
      $(#[$scalar_doc:meta])*
    )?
  ) => {
    impl $Op for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      $impl
    }

    impl $Op<$T> for $Simd {
      type Output = Self;

      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op(self, rhs: $T) -> Self::Output {
        self.$op(Self::splat(rhs))
      }
    }

    impl $Op<$Simd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $Simd) -> Self::Output {
        $Simd::splat(self).$op(rhs)
      }
    }

    impl $OpAssign for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: Self) {
        *self = (*self).$op(rhs);
      }
    }

    impl $OpAssign<$T> for $Simd {
      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: $T) {
        *self = (*self).$op(Self::splat(rhs));
      }
    }

    impl $Op<&Self> for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &Self) -> Self::Output {
        self.$op(*rhs)
      }
    }

    impl $Op<&$T> for $Simd {
      type Output = Self;

      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op(self, rhs: &$T) -> Self::Output {
        self.$op(Self::splat(*rhs))
      }
    }

    impl $Op<&$Simd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$Simd) -> Self::Output {
        $Simd::splat(self).$op(*rhs)
      }
    }

    impl $OpAssign<&Self> for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: &Self) {
        *self = (*self).$op(*rhs);
      }
    }

    impl $OpAssign<&$T> for $Simd {
      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: &$T) {
        *self = (*self).$op(Self::splat(*rhs));
      }
    }

    impl $Op<$Simd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: $Simd) -> Self::Output {
        (*self).$op(rhs)
      }
    }

    impl $Op<$T> for &$Simd {
      type Output = $Simd;

      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op(self, rhs: $T) -> Self::Output {
        (*self).$op($Simd::splat(rhs))
      }
    }

    impl $Op<$Simd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $Simd) -> Self::Output {
        $Simd::splat(*self).$op(rhs)
      }
    }

    impl $Op<&$Simd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &$Simd) -> Self::Output {
        (*self).$op(*rhs)
      }
    }

    impl $Op<&$T> for &$Simd {
      type Output = $Simd;

      $($(#[$doc_scalar])*)?
      #[inline]
      fn $op(self, rhs: &$T) -> Self::Output {
        (*self).$op($Simd::splat(*rhs))
      }
    }

    impl $Op<&$Simd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$Simd) -> Self::Output {
        $Simd::splat(*self).$op(*rhs)
      }
    }
  }
}

macro_rules! impl_shift_operator {
  (
    $T:ident,
    $Simd:ident,
    $UintSimd:ident,
    $IntSimd:ident,
    $Op:ident,
    $op:ident,
    $OpAssign:ident,
    $op_assign:ident,
    $impl_unsigned_simd:item,
    $impl_u32:item
    $(,
      $(#[$doc:meta])*,
      $(#[$doc_scalar:meta])*,
      $(#[$scalar_doc:meta])*
    )?
  ) => {
    impl $Op<$UintSimd> for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      $impl_unsigned_simd
    }

    impl $Op<$IntSimd> for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: $IntSimd) -> Self::Output {
        self.$op(cast::<$IntSimd, $UintSimd>(rhs))
      }
    }

    impl $Op<$UintSimd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $UintSimd) -> Self::Output {
        $Simd::splat(self).$op(rhs)
      }
    }

    impl $Op<$IntSimd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $IntSimd) -> Self::Output {
        $Simd::splat(self).$op(rhs)
      }
    }

    impl $OpAssign<$UintSimd> for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: $UintSimd) {
        *self = (*self).$op(rhs);
      }
    }

    impl $OpAssign<$IntSimd> for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: $IntSimd) {
        *self = (*self).$op(rhs);
      }
    }

    impl $Op<&$UintSimd> for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &$UintSimd) -> Self::Output {
        self.$op(*rhs)
      }
    }

    impl $Op<&$IntSimd> for $Simd {
      type Output = Self;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &$IntSimd) -> Self::Output {
        self.$op(*rhs)
      }
    }

    impl $Op<&$UintSimd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$UintSimd) -> Self::Output {
        $Simd::splat(self).$op(*rhs)
      }
    }

    impl $Op<&$IntSimd> for $T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$IntSimd) -> Self::Output {
        $Simd::splat(self).$op(*rhs)
      }
    }

    impl $OpAssign<&$UintSimd> for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: &$UintSimd) {
        *self = (*self).$op(*rhs);
      }
    }

    impl $OpAssign<&$IntSimd> for $Simd {
      $($(#[$doc])*)?
      #[inline]
      fn $op_assign(&mut self, rhs: &$IntSimd) {
        *self = (*self).$op(*rhs);
      }
    }

    impl $Op<$UintSimd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: $UintSimd) -> Self::Output {
        (*self).$op(rhs)
      }
    }

    impl $Op<$IntSimd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: $IntSimd) -> Self::Output {
        (*self).$op(rhs)
      }
    }

    impl $Op<$UintSimd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $UintSimd) -> Self::Output {
        $Simd::splat(*self).$op(rhs)
      }
    }

    impl $Op<$IntSimd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: $IntSimd) -> Self::Output {
        $Simd::splat(*self).$op(rhs)
      }
    }

    impl $Op<&$UintSimd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &$UintSimd) -> Self::Output {
        (*self).$op(*rhs)
      }
    }

    impl $Op<&$IntSimd> for &$Simd {
      type Output = $Simd;

      $($(#[$doc])*)?
      #[inline]
      fn $op(self, rhs: &$IntSimd) -> Self::Output {
        (*self).$op(*rhs)
      }
    }

    impl $Op<&$UintSimd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$UintSimd) -> Self::Output {
        $Simd::splat(*self).$op(*rhs)
      }
    }

    impl $Op<&$IntSimd> for &$T {
      type Output = $Simd;

      $($(#[$scalar_doc])*)?
      #[inline]
      fn $op(self, rhs: &$IntSimd) -> Self::Output {
        $Simd::splat(*self).$op(*rhs)
      }
    }

    macro_rules! impl_scalar {
      ($T2:ident, $impl:item) => {
        impl $Op<$T2> for $Simd {
          type Output = Self;

          $($(#[$doc_scalar])*)?
          $impl
        }

        impl $OpAssign<$T2> for $Simd {
          $($(#[$doc_scalar])*)?
          #[inline]
          fn $op_assign(&mut self, rhs: $T2) {
            *self = (*self).$op(rhs);
          }
        }

        impl $Op<&$T2> for $Simd {
          type Output = Self;

          $($(#[$doc_scalar])*)?
          #[inline]
          fn $op(self, rhs: &$T2) -> Self::Output {
            self.$op(*rhs)
          }
        }

        impl $OpAssign<&$T2> for $Simd {
          $($(#[$doc_scalar])*)?
          #[inline]
          fn $op_assign(&mut self, rhs: &$T2) {
            *self = (*self).$op(*rhs);
          }
        }

        impl $Op<$T2> for &$Simd {
          type Output = $Simd;

          $($(#[$doc_scalar])*)?
          #[inline]
          fn $op(self, rhs: $T2) -> Self::Output {
            (*self).$op(rhs)
          }
        }

        impl $Op<&$T2> for &$Simd {
          type Output = $Simd;

          $($(#[$doc_scalar])*)?
          #[inline]
          fn $op(self, rhs: &$T2) -> Self::Output {
            (*self).$op(*rhs)
          }
        }
      }
    }
    impl_scalar!(u32, $impl_u32);

    macro_rules! impl_scalar_with_cast {
      ($T2:ident) => {
        impl_scalar!(
          $T2,
          #[inline]
          fn $op(self, rhs: $T2) -> Self::Output {
            self.$op(rhs as u32)
          }
        );
      }
    }
    impl_scalar_with_cast!(i8);
    impl_scalar_with_cast!(i16);
    impl_scalar_with_cast!(i32);
    impl_scalar_with_cast!(i64);
    impl_scalar_with_cast!(i128);
    impl_scalar_with_cast!(isize);
    impl_scalar_with_cast!(u8);
    impl_scalar_with_cast!(u16);
    impl_scalar_with_cast!(u64);
    impl_scalar_with_cast!(u128);
    impl_scalar_with_cast!(usize);
  }
}

#[cfg(feature = "serde")]
macro_rules! impl_optional_deserialize {
  (T = $T:ident, N = $N:literal, Simd = $Simd:ty, $fn_deserialize:item) => {
    impl<'de> Deserialize<'de> for $Simd {
      $fn_deserialize
    }
  };
  (T = $T:ident, N = $N:literal, Simd = $Simd:ty,) => {
    impl<'de> Deserialize<'de> for $Simd {
      #[inline]
      fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
      where
        D: serde_core::Deserializer<'de>,
      {
        Ok(<[$T; $N]>::deserialize(deserializer)?.into())
      }
    }
  };
}

#[cfg(feature = "serde")]
pub(crate) fn deserialize_array<'de, Simd, T, const N: usize, D>(
  deserializer: D,
) -> Result<Simd, D::Error>
where
  D: serde_core::Deserializer<'de>,
  T: serde_core::Deserialize<'de> + Default + Copy,
  Simd: From<[T; N]>,
{
  // Serde does not implement [T; 64]: Deserialize, so we do this manually.
  struct ArrayVisitor<T, const N: usize>(core::marker::PhantomData<T>);

  impl<'de, T, const N: usize> serde_core::de::Visitor<'de> for ArrayVisitor<T, N>
  where
    T: serde_core::Deserialize<'de> + Default + Copy,
  {
    type Value = [T; N];

    fn expecting(
      &self,
      formatter: &mut core::fmt::Formatter<'_>,
    ) -> core::fmt::Result {
      formatter.write_str("an array of size {N}")
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<[T; N], A::Error>
    where
      A: serde_core::de::SeqAccess<'de>,
    {
      use serde_core::de::Error;
      let mut array = [T::default(); N];
      for (index, element) in array.iter_mut().enumerate() {
        *element = seq
          .next_element()?
          .ok_or_else(|| A::Error::invalid_length(index, &self))?;
      }
      if seq.next_element::<T>()?.is_some() {
        return Err(A::Error::invalid_length(N + 1, &self));
      }
      Ok(array)
    }
  }

  Ok(
    deserializer
      .deserialize_tuple(N, ArrayVisitor::<T, N>(core::marker::PhantomData))?
      .into(),
  )
}
