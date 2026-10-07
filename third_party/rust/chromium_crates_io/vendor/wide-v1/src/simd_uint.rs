/// Emits functionality shared by all SIMD unsigned-integer types.
///
/// Functions that need a separate implementation for each type (for
/// performance) use `$fn_{name}:item` syntax, and functions that have one
/// shared implementation for all uints are written out normally inside this
/// macro.
///
/// This macro also invokes `impl_simd`.
macro_rules! impl_simd_uint {
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
      IntSimd = $IntSimd:ident,
      T_BITS = $T_BITS:literal,
      T_BITS_MUL_2 = $T_BITS_MUL_2:literal,
      [$($index:literal),* $(,)?],
      optional_type_x86_inner { $(X86Inner = $X86Inner:ident)? },
      optional_type_arm_inner { $(ArmInner = $ArmInner:ident)? },
      optional_type_wasm_inner { $(WasmInner = $WasmInner:ident)? },
    }

    // General SIMD functions
    $fn_not:item
    $fn_add:item
    $fn_sub:item
    $fn_mul:item
    $fn_bitand:item
    $fn_bitor:item
    $fn_bitxor:item
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

    // Uint-specific functions
    $fn_shl_unsigned_simd:item
    $fn_shl_u32:item
    $fn_shr_unsigned_simd:item
    $fn_shr_u32:item
    $fn_max:item
    $fn_min:item
    $fn_reduce_max:item
    $fn_reduce_min:item
    $fn_unbounded_shl:item
    $fn_unbounded_shl_scalar:item
    $fn_unbounded_shr:item
    $fn_unbounded_shr_scalar:item
    $fn_saturating_add:item
    $fn_saturating_sub:item
    $fn_overflowing_mul:item
    optional_fn_widening_mul { $($fn_widening_mul:item)? }
    $fn_mul_keep_low_high:item
    $fn_mul_keep_high:item
    optional_fn_deserialize { $($fn_deserialize:item)? }
  ) => {
    impl_simd!(
      unsafe {
        T = $T,
        N = $N,
        Simd = $Simd,
        UintSimd = $Simd,
        optional_type_x86_inner { $(X86Inner = $X86Inner)? },
        optional_type_arm_inner { $(ArmInner = $ArmInner)? },
        optional_type_wasm_inner { $(WasmInner = $WasmInner)? },
      }

      $fn_simd_eq

      $fn_simd_ne

      $fn_simd_lt

      $fn_simd_gt

      $fn_simd_le

      $fn_simd_ge

      $fn_reduce_add

      $fn_reduce_mul

      $fn_bitselect

      $fn_select

      $fn_to_bitmask

      $fn_any

      $fn_all

      $fn_shuffle

      $fn_shuffle_zeroing

      $fn_shuffle_wrapping

      $fn_shuffle_2

      $fn_shuffle_zeroing_2

      $fn_shuffle_wrapping_2

      $fn_shuffle_3

      $fn_shuffle_zeroing_3

      $fn_shuffle_wrapping_3

      $fn_shuffle_4

      $fn_shuffle_zeroing_4

      $fn_shuffle_wrapping_4

      $fn_transpose

      optional_fn_deserialize { $($fn_deserialize)? }
    );

    impl_unary_operator!(
      $Simd,
      Neg,
      neg,
      #[inline]
      fn neg(self) -> Self::Output {
        Self::default() - self
      }
    );
    impl_unary_operator!($Simd, Not, not, $fn_not);

    impl_binary_operator!($T, $Simd, Add, add, AddAssign, add_assign, $fn_add);
    impl_binary_operator!($T, $Simd, Sub, sub, SubAssign, sub_assign, $fn_sub);
    impl_binary_operator!($T, $Simd, Mul, mul, MulAssign, mul_assign, $fn_mul);
    impl_binary_operator!(
        $T,
        $Simd,
        Div,
        div,
        DivAssign,
        div_assign,
        #[inline]
        fn div(self, rhs: Self) -> Self::Output {
            let self_array = self.to_array();
            let rhs_array = rhs.to_array();

            Self::new([$(self_array[$index].wrapping_div(rhs_array[$index])),*])
        },
        /// Divides each element of `left` by the corresponding element `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if any element of `right` is zero.
        ,
        /// Divides each element of `left` by the scalar `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if `right` is zero.
        ,
        /// Divides the scalar `left` by each element of `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if any element of `right` is zero.
    );
    impl_binary_operator!(
        $T,
        $Simd,
        Rem,
        rem,
        RemAssign,
        rem_assign,
        #[inline]
        fn rem(self, rhs: Self) -> Self::Output {
            let self_array = self.to_array();
            let rhs_array = rhs.to_array();

            Self::new([$(self_array[$index].wrapping_rem(rhs_array[$index])),*])
        },
        /// Returns the remainder of each element of `left` divided by the
        /// corresponding element `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if any element of `right` is zero.
        ,
        /// Returns the remainder of each element of `left` divided by the
        /// scalar `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if `right` is zero.
        ,
        /// Returns the remainder of the scalar `left` divided by each element
        /// of `right`.
        ///
        /// Note that because division has no hardware support, this operation
        /// is very slow and should be avoided if possible.
        ///
        /// # Panics
        ///
        /// Panics if any element of `right` is zero.
    );
    impl_shift_operator!(
      $T,
      $Simd,
      $Simd,
      $IntSimd,
      Shl,
      shl,
      ShlAssign,
      shl_assign,
      $fn_shl_unsigned_simd,
      $fn_shl_u32,
      /// Shifts left each element of `self` by the corresponding element of
      /// `rhs`.
      ///
      /// This operator behaves like [`wrapping_shl`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shl`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shl`]: ", stringify!($T), "::wrapping_shl")]
      #[doc = concat!("[`unbounded_shl`]: ", stringify!($Simd), "::unbounded_shl")]
      ,
      /// Shifts left each element of `self` by the uniform scalar `rhs`.
      ///
      /// This operator behaves like [`wrapping_shl`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shl_scalar`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shl`]: ", stringify!($T), "::wrapping_shl")]
      #[doc = concat!("[`unbounded_shl_scalar`]: ", stringify!($Simd), "::unbounded_shl_scalar")]
      ,
      /// Shifts left the scalar `self` by each element of `rhs`.
      ///
      /// This operator behaves like [`wrapping_shl`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shl`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shl`]: ", stringify!($T), "::wrapping_shl")]
      #[doc = concat!("[`unbounded_shl`]: ", stringify!($Simd), "::unbounded_shl")]
    );
    impl_shift_operator!(
      $T,
      $Simd,
      $Simd,
      $IntSimd,
      Shr,
      shr,
      ShrAssign,
      shr_assign,
      $fn_shr_unsigned_simd,
      $fn_shr_u32,
      /// Shifts right each element of `self` by the corresponding element of
      /// `rhs`.
      ///
      /// This operator behaves like [`wrapping_shr`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shr`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shr`]: ", stringify!($T), "::wrapping_shr")]
      #[doc = concat!("[`unbounded_shr`]: ", stringify!($Simd), "::unbounded_shr")]
      ,
      /// Shifts right each element of `self` by the uniform scalar `rhs`.
      ///
      /// This operator behaves like [`wrapping_shr`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shr_scalar`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shr`]: ", stringify!($T), "::wrapping_shr")]
      #[doc = concat!("[`unbounded_shr_scalar`]: ", stringify!($Simd), "::unbounded_shr_scalar")]
      ,
      /// Shifts right the scalar `self` by each element of `rhs`.
      ///
      /// This operator behaves like [`wrapping_shr`].
      ///
      /// Note that for most targets, this operator is slower than
      /// [`unbounded_shr`], so consider using that instead.
      ///
      #[doc = concat!("[`wrapping_shr`]: ", stringify!($T), "::wrapping_shr")]
      #[doc = concat!("[`unbounded_shr`]: ", stringify!($Simd), "::unbounded_shr")]
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitAnd,
      bitand,
      BitAndAssign,
      bitand_assign,
      $fn_bitand
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitOr,
      bitor,
      BitOrAssign,
      bitor_assign,
      $fn_bitor
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitXor,
      bitxor,
      BitXorAssign,
      bitxor_assign,
      $fn_bitxor
    );

    impl<Rhs> core::iter::Sum<Rhs> for $Simd
    where
      $Simd: AddAssign<Rhs>,
    {
      #[inline]
      fn sum<I: Iterator<Item = Rhs>>(iter: I) -> Self {
        let mut total = Self::zeroed();
        for val in iter {
          total += val;
        }
        total
      }
    }

    impl<Rhs> core::iter::Product<Rhs> for $Simd
    where
      $Simd: MulAssign<Rhs>,
    {
      #[inline]
      fn product<I: Iterator<Item = Rhs>>(iter: I) -> Self {
        let mut total = Self::from(1);
        for val in iter {
          total *= val;
        }
        total
      }
    }

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
    impl_formatting_trait!(core::fmt::Binary);
    impl_formatting_trait!(core::fmt::LowerHex);
    impl_formatting_trait!(core::fmt::Octal);
    impl_formatting_trait!(core::fmt::UpperHex);

    /// The following functionality exists for all SIMD vectors of unsigned
    /// integers.
    impl $Simd {
      /// A SIMD vector with all elements set to `1`.
      pub const ONE: Self = Self::splat(1);

      /// A SIMD vector with all elements set to `0`.
      pub const ZERO: Self = Self::splat(0);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::MAX`].")]
      pub const MAX: Self = Self::splat($T::MAX);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::MIN`].")]
      pub const MIN: Self = Self::splat($T::MIN);

      /// The number of elements in this SIMD vector.
      pub const LANES: u16 = $N;

      /// The size of this SIMD vector in bits.
      pub const BITS: u16 = (size_of::<Self>() * 8) as u16;

      /// Returns the maximum between each element of `self` and the
      /// corresponding element of `other`.
      #[must_use]
      $fn_max

      /// Returns the minimum between each element of `self` and the
      /// corresponding element of `other`.
      #[must_use]
      $fn_min

      /// Clamps each element of `self` between the corresponding elements of
      /// `min` and `max`.
      ///
      /// If `min > max`, the result is unspecified. Consider manually checking
      /// for that case.
      #[inline]
      #[must_use]
      pub fn clamp(self, min: Self, max: Self) -> Self {
        self.max(min).min(max)
      }

      /// Reducing maximum. Returns the maximum of the vector's elements.
      ///
      /// Equivalent to `self[0].max(self[1].max(...))`.
      #[must_use]
      $fn_reduce_max

      /// Reducing minimum. Returns the minimum of the vector's elements.
      ///
      /// Equivalent to `self[0].min(self[1].min(...))`.
      #[must_use]
      $fn_reduce_min

      /// Returns the bit patterns of `self` reinterpreted as signed integers of
      /// the same size.
      #[inline]
      #[must_use]
      pub const fn cast_signed(self) -> $IntSimd {
        // SAFETY: Both types accept all bit-patterns and only contain
        // initialized memory.
        unsafe { core::mem::transmute::<$Simd, $IntSimd>(self) }
      }

      /// Shifts left each element of `self` by the corresponding element of
      /// `rhs`, without bounding `rhs`.
      ///
      #[doc = concat!("If `rhs` is larger than or equal to the number of bits in [`", stringify!($T), "`],")]
      /// the entire value is shifted out, and `0` is returned.
      ///
      /// This is different from the standard operator, which behaves like
      /// [`wrapping_shl`]. For most targets, `unbounded_shl` is faster than the
      /// standard operator.
      ///
      /// If you intend to shift all elements by the same value, consider using
      /// [`unbounded_shl_scalar`] which is faster.
      ///
      #[doc = concat!("[`wrapping_shl`]: ", stringify!($T), "::wrapping_shl")]
      /// [`unbounded_shl_scalar`]: Self::unbounded_shl_scalar
      #[must_use]
      $fn_unbounded_shl

      /// Shifts left each element of `self` by the uniform scalar `rhs`,
      /// without bounding `rhs`.
      ///
      #[doc = concat!("If `rhs` is larger than or equal to the number of bits in [`", stringify!($T), "`],")]
      /// the entire value is shifted out, and `0` is returned.
      ///
      /// This is different from the standard operator, which behaves like
      /// [`wrapping_shl`]. For most targets, `unbounded_shl_scalar` is faster
      /// than the standard operator.
      ///
      /// This function is faster than `self.unbounded_shl(splat(rhs))` because
      /// it has special hardware support.
      ///
      #[doc = concat!("[`wrapping_shl`]: ", stringify!($T), "::wrapping_shl")]
      #[must_use]
      $fn_unbounded_shl_scalar

      /// Shifts right each element of `self` by the corresponding element of
      /// `rhs`, without bounding `rhs`.
      ///
      #[doc = concat!("If `rhs` is larger than or equal to the number of bits in [`", stringify!($T), "`],")]
      /// the entire value is shifted out, and `0` is returned.
      ///
      /// This is different from the standard operator, which behaves like
      /// [`wrapping_shr`]. For most targets, `unbounded_shr` is faster than the
      /// standard operator.
      ///
      /// If you intend to shift all elements by the same value, consider using
      /// [`unbounded_shr_scalar`] which is faster.
      ///
      #[doc = concat!("[`wrapping_shr`]: ", stringify!($T), "::wrapping_shr")]
      /// [`unbounded_shr_scalar`]: Self::unbounded_shr_scalar
      #[must_use]
      $fn_unbounded_shr

      /// Shifts right each element of `self` by the uniform scalar `rhs`,
      /// without bounding `rhs`.
      ///
      #[doc = concat!("If `rhs` is larger than or equal to the number of bits in [`", stringify!($T), "`],")]
      /// the entire value is shifted out, and `0` is returned.
      ///
      /// This is different from the standard operator, which behaves like
      /// [`wrapping_shr`]. For most targets, `unbounded_shr_scalar` is faster
      /// than the standard operator.
      ///
      /// This function is faster than `self.unbounded_shr(splat(rhs))` because
      /// it has special hardware support.
      ///
      #[doc = concat!("[`wrapping_shr`]: ", stringify!($T), "::wrapping_shr")]
      #[must_use]
      $fn_unbounded_shr_scalar

      /// Saturating integer addition. Computes `self + rhs`, saturating at the
      /// numeric bounds instead of overflowing.
      #[must_use]
      $fn_saturating_add

      /// Saturating integer subtraction. Computes `self - rhs`, saturating at
      /// the numeric bounds instead of overflowing.
      #[must_use]
      $fn_saturating_sub

      /// Saturating integer multiplication. Computes `self * rhs`, saturating
      /// at the numeric bounds instead of overflowing.
      #[inline]
      #[must_use]
      pub fn saturating_mul(self, rhs: Self) -> Self {
        let (low, high) = self.mul_keep_low_high(rhs);
        low | high.simd_ne(Self::ZERO)
      }

      /// Saturating integer division. Computes `self / rhs`, saturating at the
      /// numeric bounds instead of overflowing.
      ///
      /// Note that for unsigned integers overflow never occurs, so this is
      /// identical to regular division. This function only exists for
      /// consistency with signed integers.
      ///
      /// Note that because division has no hardware support, this operation is
      /// very slow and should be avoided if possible.
      ///
      /// # Panics
      ///
      /// Panics if any element of `rhs` is zero.
      #[inline]
      #[must_use]
      pub fn saturating_div(self, rhs: Self) -> Self {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([$(self_array[$index].saturating_div(rhs_array[$index])),*])
      }

      /// Returns `self + rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The addition (returns the wrapped value if an overflow occured)
      /// - A mask indicating whether an overflow occured
      #[inline]
      #[must_use]
      pub fn overflowing_add(self, rhs: Self) -> (Self, Self) {
        let result = self + rhs;
        let overflow = result.simd_lt(self);

        (result, overflow)
      }

      /// Returns `self - rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The subtraction (returns the wrapped value if an overflow occured)
      /// - A mask indicating whether an overflow occured
      #[inline]
      #[must_use]
      pub fn overflowing_sub(self, rhs: Self) -> (Self, Self) {
        let result = self - rhs;
        let overflow = result.simd_gt(self);

        (result, overflow)
      }

      /// Returns `self * rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The multiplication (returns the wrapped value if an overflow occured)
      /// - A mask indicating whether an overflow occured
      #[must_use]
      $fn_overflowing_mul

      /// Returns `self / rhs` and whether an overflow occured.
      ///
      /// Note that for unsigned integers overflow never occurs, so the second
      /// value is always `false`. This function only exists for consistency
      /// with signed integers.
      ///
      /// Returns a tuple with:
      ///
      /// - The division (returns `self` if an overflow occured)
      /// - A mask indicating whether an overflow occured (always false)
      ///
      /// Note that because division has no hardware support, this operation is
      /// very slow and should be avoided if possible.
      #[inline]
      #[must_use]
      pub fn overflowing_div(self, rhs: Self) -> (Self, Self) {
        (self / rhs, Self::ZERO)
      }

      /// Returns `self % rhs` and whether an overflow occured.
      ///
      /// Note that for unsigned integers overflow never occurs, so the second
      /// value is always `false`. This function only exists for consistency
      /// with signed integers.
      ///
      /// Returns a tuple with:
      ///
      /// - The remainder (returns zero if an overflow occured)
      /// - A mask indicating whether an overflow occured (always false)
      ///
      /// Note that because division has no hardware support, this operation is
      /// very slow and should be avoided if possible.
      #[inline]
      #[must_use]
      pub fn overflowing_rem(self, rhs: Self) -> (Self, Self) {
        (self % rhs, Self::ZERO)
      }

      $(
        /// Widening multiplication. Computes `self * rhs`, widening to a SIMD
        /// vector of a larger integer type.
        ///
        /// The returned value is always exact and can never overflow.
        ///
        /// This function is different from [`mul_keep_low_high`], which returns
        /// two seperate SIMD vectors for low and high parts, instead of a
        /// single SIMD vector of a larger integer type. Also note that while
        /// [`mul_keep_low_high`] exists for all types, `widening_mul` does not
        /// exist for types with no wider variant (e.g., for `i32x16` because
        /// there is no `i64x16`).
        ///
        /// [`mul_keep_low_high`]: Self::mul_keep_low_high
        #[must_use]
        $fn_widening_mul
      )?

      #[doc = concat!(
        "Computes `self * rhs`, producing intermediate ",
        $T_BITS_MUL_2,
        "-bit integers, then returns their low ",
        $T_BITS,
        "-bit parts and high ",
        $T_BITS,
        "-bit parts in two seperate SIMD vectors."
      )]
      ///
      /// This function is different from `widening_mul`, which returns a single
      /// SIMD vector of a larger integer type, instead of two seperate SIMD
      /// vectors for low and high parts. Also note that while
      /// `mul_keep_low_high` exists for all types, `widening_mul` does not
      /// exist for types with no wider variant (e.g., for `i32x16` because
      /// there is no `i64x16`).
      #[must_use]
      $fn_mul_keep_low_high

      #[doc = concat!(
        "Computes `self * rhs`, producing intermediate ",
        $T_BITS_MUL_2,
        "-bit integers, then returns their high ",
        $T_BITS,
        "-bit parts."
      )]
      #[must_use]
      $fn_mul_keep_high
    }
  };
}
