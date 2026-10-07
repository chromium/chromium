/// Emits functionality shared by all SIMD signed-integer types.
///
/// Functions that need a separate implementation for each type (for
/// performance) use `$fn_{name}:item` syntax, and functions that have one
/// shared implementation for all ints are written out normally inside this
/// macro.
///
/// This macro also invokes `impl_simd`.
macro_rules! impl_simd_int {
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
      T_BITS = $T_BITS:literal,
      T_BITS_MUL_2 = $T_BITS_MUL_2:literal,
      BitmaskType = $BitmaskType:ty,
      [$($index:literal),* $(,)?],
      optional_type_x86_inner { $(X86Inner = $X86Inner:ident)? },
      optional_type_arm_inner { $(ArmInner = $ArmInner:ident)? },
      optional_type_wasm_inner { $(WasmInner = $WasmInner:ident)? },
    }

    // General SIMD functions
    $fn_simd_lt:item
    $fn_simd_gt:item
    $fn_simd_le:item
    $fn_simd_ge:item

    // Int-specific functions
    $fn_shr_unsigned_simd:item
    $fn_shr_u32:item
    $fn_max:item
    $fn_min:item
    $fn_reduce_max:item
    $fn_reduce_min:item
    $fn_unbounded_shr:item
    $fn_unbounded_shr_scalar:item
    $fn_saturating_add:item
    $fn_saturating_sub:item
    $fn_overflowing_mul:item
    optional_fn_widening_mul { $($fn_widening_mul:item)? }
    $fn_mul_keep_low_high:item
    $fn_mul_keep_high:item
    $fn_abs:item
    $fn_is_positive:item
    $fn_is_negative:item
    optional_fn_deserialize { $($fn_deserialize:item)? }
  ) => {
    impl_simd!(
      unsafe {
        T = $T,
        N = $N,
        Simd = $Simd,
        UintSimd = $UintSimd,
        optional_type_x86_inner { $(X86Inner = $X86Inner)? },
        optional_type_arm_inner { $(ArmInner = $ArmInner)? },
        optional_type_wasm_inner { $(WasmInner = $WasmInner)? },
      }

      #[inline]
      fn simd_eq(self, other: Self) -> Self {
        self.cast_unsigned().simd_eq(other.cast_unsigned()).cast_signed()
      }

      #[inline]
      fn simd_ne(self, other: Self) -> Self {
        self.cast_unsigned().simd_ne(other.cast_unsigned()).cast_signed()
      }

      $fn_simd_lt

      $fn_simd_gt

      $fn_simd_le

      $fn_simd_ge

      #[inline]
      pub fn reduce_add(self) -> $T {
        // Wrapping addition is the same for signed and unsigned integers.
        cast::<$Simd, $UintSimd>(self).reduce_add().cast_signed()
      }

      #[inline]
      pub fn reduce_mul(self) -> $T {
        // Wrapping multiplication is the same for signed and unsigned integers.
        cast::<$Simd, $UintSimd>(self).reduce_mul().cast_signed()
      }

      #[inline]
      pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
        self.cast_unsigned()
          .bitselect(if_one.cast_unsigned(), if_zero.cast_unsigned())
          .cast_signed()
      }

      #[inline]
      fn select(self, if_true: Self, if_false: Self) -> Self {
        self.cast_unsigned()
          .select(if_true.cast_unsigned(), if_false.cast_unsigned())
          .cast_signed()
      }

      #[inline]
      pub fn to_bitmask(self) -> $BitmaskType {
        self.cast_unsigned().to_bitmask()
      }

      #[inline]
      pub fn any(self) -> bool {
        self.cast_unsigned().any()
      }

      #[inline]
      pub fn all(self) -> bool {
        self.cast_unsigned().all()
      }

      #[inline]
      pub fn shuffle(self, indices: $UintSimd) -> Self {
        self.cast_unsigned().shuffle(indices).cast_signed()
      }

      #[inline]
      pub fn shuffle_zeroing(self, indices: $UintSimd) -> Self {
        self.cast_unsigned().shuffle_zeroing(indices).cast_signed()
      }

      #[inline]
      pub fn shuffle_wrapping(self, indices: $UintSimd) -> Self {
        self.cast_unsigned().shuffle_wrapping(indices).cast_signed()
      }

      #[inline]
      fn shuffle(self: [$Simd; 2], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 2], [$UintSimd; 2]>(self).shuffle(indices))
      }

      #[inline]
      fn shuffle_zeroing(self: [$Simd; 2], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 2], [$UintSimd; 2]>(self).shuffle_zeroing(indices))
      }

      #[inline]
      fn shuffle_wrapping(self: [$Simd; 2], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 2], [$UintSimd; 2]>(self).shuffle_wrapping(indices))
      }

      #[inline]
      fn shuffle(self: [$Simd; 3], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 3], [$UintSimd; 3]>(self).shuffle(indices))
      }

      #[inline]
      fn shuffle_zeroing(self: [$Simd; 3], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 3], [$UintSimd; 3]>(self).shuffle_zeroing(indices))
      }

      #[inline]
      fn shuffle_wrapping(self: [$Simd; 3], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 3], [$UintSimd; 3]>(self).shuffle_wrapping(indices))
      }

      #[inline]
      fn shuffle(self: [$Simd; 4], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 4], [$UintSimd; 4]>(self).shuffle(indices))
      }

      #[inline]
      fn shuffle_zeroing(self: [$Simd; 4], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 4], [$UintSimd; 4]>(self).shuffle_zeroing(indices))
      }

      #[inline]
      fn shuffle_wrapping(self: [$Simd; 4], indices: $UintSimd) -> $Simd {
        cast(cast::<[$Simd; 4], [$UintSimd; 4]>(self).shuffle_wrapping(indices))
      }

      #[inline]
      pub fn transpose(data: [Self; $N]) -> [Self; $N] {
        cast($UintSimd::transpose(cast::<[$Simd; $N], [$UintSimd; $N]>(data)))
      }

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
    impl_unary_operator!(
      $Simd,
      Not,
      not,
      #[inline]
      fn not(self) -> Self::Output {
        cast::<$UintSimd, $Simd>(!cast::<$Simd, $UintSimd>(self))
      }
    );

    impl_binary_operator!(
      $T,
      $Simd,
      Add,
      add,
      AddAssign,
      add_assign,
      #[inline]
      fn add(self, rhs: Self) -> Self::Output {
        // Wrapping addition is the same for signed and unsigned integers.
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) + cast::<$Simd, $UintSimd>(rhs),
        )
      }
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Sub,
      sub,
      SubAssign,
      sub_assign,
      #[inline]
      fn sub(self, rhs: Self) -> Self::Output {
        // Wrapping subtraction is the same for signed and unsigned integers.
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) - cast::<$Simd, $UintSimd>(rhs),
        )
      }
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Mul,
      mul,
      MulAssign,
      mul_assign,
      #[inline]
      fn mul(self, rhs: Self) -> Self::Output {
        // Wrapping multiplication is the same for signed and unsigned integers.
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) * cast::<$Simd, $UintSimd>(rhs),
        )
      }
    );
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
      $UintSimd,
      $Simd,
      Shl,
      shl,
      ShlAssign,
      shl_assign,
      #[inline]
      fn shl(self, rhs: $UintSimd) -> Self {
        cast(cast::<$Simd, $UintSimd>(self) << rhs)
      },
      #[inline]
      fn shl(self, rhs: u32) -> Self {
        cast(cast::<$Simd, $UintSimd>(self) << rhs)
      },
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
      $UintSimd,
      $Simd,
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
      #[inline]
      fn bitand(self, rhs: Self) -> Self::Output {
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) & cast::<$Simd, $UintSimd>(rhs),
        )
      }
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitOr,
      bitor,
      BitOrAssign,
      bitor_assign,
      #[inline]
      fn bitor(self, rhs: Self) -> Self::Output {
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) | cast::<$Simd, $UintSimd>(rhs),
        )
      }
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitXor,
      bitxor,
      BitXorAssign,
      bitxor_assign,
      #[inline]
      fn bitxor(self, rhs: Self) -> Self::Output {
        cast::<$UintSimd, $Simd>(
          cast::<$Simd, $UintSimd>(self) ^ cast::<$Simd, $UintSimd>(rhs),
        )
      }
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

    impl Select<$Simd> for $UintSimd {
      #[inline]
      fn select(self, if_true: $Simd, if_false: $Simd) -> $Simd {
        self.cast_signed().select(if_true, if_false)
      }
    }

    impl Select<$UintSimd> for $Simd {
      #[inline]
      fn select(self, if_true: $UintSimd, if_false: $UintSimd) -> $UintSimd {
        self.cast_unsigned().select(if_true, if_false)
      }
    }

    /// The following functionality exists for all SIMD vectors of signed
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

      /// Returns the bit patterns of `self` reinterpreted as unsigned integers
      /// of the same size.
      #[inline]
      #[must_use]
      pub const fn cast_unsigned(self) -> $UintSimd {
        // SAFETY: Both types accept all bit-patterns and only contain
        // initialized memory.
        unsafe { core::mem::transmute::<$Simd, $UintSimd>(self) }
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
      #[inline]
      #[must_use]
      pub fn unbounded_shl(self, rhs: $UintSimd) -> Self {
        // Shift left is the same for unsigned and signed integers.
        cast(cast::<$Simd, $UintSimd>(self).unbounded_shl(rhs))
      }

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
      #[inline]
      #[must_use]
      pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
        // Shift left is the same for unsigned and signed integers.
        cast(cast::<$Simd, $UintSimd>(self).unbounded_shl_scalar(rhs))
      }

      /// Shifts right each element of `self` by the corresponding element of
      /// `rhs`, without bounding `rhs`.
      ///
      #[doc = concat!("If `rhs` is larger than or equal to the number of bits in [`", stringify!($T), "`],")]
      /// the entire value is shifted out, which yields `0` for a positive
      /// number, and `-1` for a negative number.
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
      /// the entire value is shifted out, which yields `0` for a positive
      /// number, and `-1` for a negative number.
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
        let (result, overflow) = self.overflowing_mul(rhs);
        let limit = Self::MAX ^ (self ^ rhs).is_negative();
        overflow.select(limit, result)
      }

      /// Saturating integer division. Computes `self / rhs`, saturating at the
      /// numeric bounds instead of overflowing.
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

      /// Computes the absolute value of each input element, returned as an
      /// unsigned integer in order to avoid wrapping.
      #[inline]
      #[must_use]
      pub fn unsigned_abs(self) -> $UintSimd {
        cast::<$Simd, $UintSimd>(self.abs())
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
        let overflow = (!(self ^ rhs) & (self ^ result)).is_negative();

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
        let overflow = ((self ^ rhs) & (self ^ result)).is_negative();

        (result, overflow)
      }

      /// Returns `self * rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The multiplication (returns the wrapped value if an overflow
      ///   occured)
      /// - A mask indicating whether an overflow occured
      #[must_use]
      $fn_overflowing_mul

      /// Returns `self / rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The division (returns `self` if an overflow occured)
      /// - A mask indicating whether an overflow occured
      ///
      /// Note that because division has no hardware support, this operation is
      /// very slow and should be avoided if possible.
      #[inline]
      #[must_use]
      pub fn overflowing_div(self, rhs: Self) -> (Self, Self) {
        // The second field is equivalent to
        // `self.simd_eq(Self::MIN) & rhs.simd_eq(-1)` but may be cheaper.
        (self / rhs, ((self ^ Self::MAX) & rhs).simd_eq(!Self::ZERO))
      }

      /// Returns `self % rhs` and whether an overflow occured.
      ///
      /// Returns a tuple with:
      ///
      /// - The remainder (returns zero if an overflow occured)
      /// - A mask indicating whether an overflow occured
      ///
      /// Note that because division has no hardware support, this operation is
      /// very slow and should be avoided if possible.
      #[inline]
      #[must_use]
      pub fn overflowing_rem(self, rhs: Self) -> (Self, Self) {
        // The second field is equivalent to
        // `self.simd_eq(Self::MIN) & rhs.simd_eq(-1)` but may be cheaper.
        (self % rhs, ((self ^ Self::MAX) & rhs).simd_eq(!Self::ZERO))
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

      /// Returns the absolute value of each input element.
      #[must_use]
      $fn_abs

      /// Returns numbers representing the sign of each element.
      ///
      /// - `0` if the element is zero
      /// - `1` if the element is positive
      /// - `-1` if the element is negative
      #[inline]
      #[must_use]
      pub fn signum(self) -> Self {
        // Flip signs because the result for true in `is_positive/negative` is
        // `-1` (all bits set).
        self.is_negative() - self.is_positive()
      }

      /// Returns a [mask] that is true for each positive element, and false if
      /// it is zero or negative.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_positive

      /// Returns a [mask] that is true for each negative element, and false if
      /// it is zero or positive.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_negative
    }
  };
}
