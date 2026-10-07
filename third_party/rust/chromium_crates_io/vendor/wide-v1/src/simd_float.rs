/// Emits functionality shared by all SIMD float types.
///
/// Functions that need a separate implementation for each type (for
/// performance) use `$fn_{name}:item` syntax, and functions that have one
/// shared implementation for all floats are written out normally inside this
/// macro.
///
/// This macro also invokes `impl_simd`.
macro_rules! impl_simd_float {
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
      UintT = $UintT:ident,
      UintSimd = $UintSimd:ident,
      optional_type_x86_inner { $(X86Inner = $X86Inner:ident)? },
      optional_type_arm_inner { $(ArmInner = $ArmInner:ident)? },
      optional_type_wasm_inner { $(WasmInner = $WasmInner:ident)? },
    }
    old_powf_simd_fn_name = $old_powf_simd_fn_name:ident,

    // General SIMD functions
    $fn_neg:item
    $fn_not:item
    $fn_add:item
    $fn_sub:item
    $fn_mul:item
    $fn_div:item
    $fn_rem:item
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
    $fn_transpose:item

    // Float-specific functions
    $fn_is_nan:item
    $fn_is_inf:item
    $fn_is_finite:item
    $fn_is_sign_positive:item
    $fn_is_sign_negative:item
    $fn_recip:item
    $fn_recip_sqrt:item
    $fn_max:item
    $fn_fast_max:item
    $fn_min:item
    $fn_fast_min:item
    $fn_clamp:item
    $fn_fast_clamp:item
    $fn_abs:item
    $fn_floor:item
    $fn_ceil:item
    $fn_round:item
    $fn_round_int:item
    $fn_fast_round_int:item
    $fn_round_ties_even:item
    $fn_trunc:item
    $fn_trunc_int:item
    $fn_fast_trunc_int:item
    $fn_mul_add:item
    $fn_mul_sub:item
    $fn_mul_neg_add:item
    $fn_mul_neg_sub:item
    $fn_powf_simd:item
    $fn_sqrt:item
    $fn_exp:item
    $fn_exp2:item
    $fn_ln:item
    $fn_cbrt:item
    $fn_asin:item
    $fn_acos:item
    $fn_atan:item
    $fn_atan2:item
    $fn_sin_cos:item
    $fn_asin_acos:item
    $fn_exp_m1:item
    $fn_ln_1p:item
    $fn_sinh:item
    $fn_cosh:item
    $fn_tanh:item
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

      $fn_simd_eq

      $fn_simd_ne

      $fn_simd_lt

      $fn_simd_gt

      $fn_simd_le

      $fn_simd_ge

      ///
      /// # Unspecified precision
      ///
      /// The order of addition is non-deterministic. This means it varies by
      /// platform, version, and can even differ within the same execution from
      /// one invocation to the next.
      $fn_reduce_add

      ///
      /// # Unspecified precision
      ///
      /// The order of multiplication is non-deterministic. This means it varies
      /// by platform, version, and can even differ within the same execution
      /// from one invocation to the next.
      $fn_reduce_mul

      $fn_bitselect

      $fn_select

      $fn_to_bitmask

      $fn_any

      $fn_all

      #[inline]
      pub fn shuffle(self, indices: $UintSimd) -> Self {
        Self::from_bits(self.to_bits().shuffle(indices))
      }

      #[inline]
      pub fn shuffle_zeroing(self, indices: $UintSimd) -> Self {
        Self::from_bits(self.to_bits().shuffle_zeroing(indices))
      }

      #[inline]
      pub fn shuffle_wrapping(self, indices: $UintSimd) -> Self {
        Self::from_bits(self.to_bits().shuffle_wrapping(indices))
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

      $fn_transpose

      optional_fn_deserialize {}
    );

    impl_unary_operator!(
      $Simd,
      Neg,
      neg,
      $fn_neg,
      /// Returns the negative of each element of `self`.
      ///
      /// This always returns the precise result, simply flipping the sign-bit.
    );
    impl_unary_operator!(
      $Simd,
      Not,
      not,
      $fn_not,
      /// Computes bitwise NOT for each element of `self`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
    );

    impl_binary_operator!(
      $T,
      $Simd,
      Add,
      add,
      AddAssign,
      add_assign,
      $fn_add,
      /// Computes addition for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes addition for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes addition for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This always returns the precise result.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Sub,
      sub,
      SubAssign,
      sub_assign,
      $fn_sub,
      /// Computes subtraction for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes subtraction for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes subtraction for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This always returns the precise result.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Mul,
      mul,
      MulAssign,
      mul_assign,
      $fn_mul,
      /// Computes multiplication for each element of `self` and the
      /// corresponding element of `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes multiplication for each element of `self` and the uniform
      /// scalar `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes multiplication for the uniform scalar `self` and each element
      /// of `rhs`, returning a SIMD vector.
      ///
      /// This always returns the precise result.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Div,
      div,
      DivAssign,
      div_assign,
      $fn_div,
      /// Computes division for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes division for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes division for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This always returns the precise result.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      Rem,
      rem,
      RemAssign,
      rem_assign,
      $fn_rem,
      /// Computes the remainder for each element of `self` and the
      /// corresponding element of `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes the remainder for each element of `self` and the uniform
      /// scalar `rhs`.
      ///
      /// This always returns the precise result.
      ,
      /// Computes the remainder for the uniform scalar `self` and each element
      /// of `rhs`, returning a SIMD vector.
      ///
      /// This always returns the precise result.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitAnd,
      bitand,
      BitAndAssign,
      bitand_assign,
      $fn_bitand,
      /// Computes bitwise AND for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise AND for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise AND for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitOr,
      bitor,
      BitOrAssign,
      bitor_assign,
      $fn_bitor,
      /// Computes bitwise OR for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise OR for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise OR for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
    );
    impl_binary_operator!(
      $T,
      $Simd,
      BitXor,
      bitxor,
      BitXorAssign,
      bitxor_assign,
      $fn_bitxor,
      /// Computes bitwise XOR for each element of `self` and the corresponding
      /// element of `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise XOR for each element of `self` and the uniform scalar
      /// `rhs`.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
      ,
      /// Computes bitwise XOR for the uniform scalar `self` and each element of
      /// `rhs`, returning a SIMD vector.
      ///
      /// This operator is not implemented for primitive scalar floats, but its
      /// behavior here is the same as for integers.
    );

    impl<Rhs> core::iter::Sum<Rhs> for $Simd
    where
      $Simd: AddAssign<Rhs>,
    {
      /// Computes the sum of multiple SIMD vectors for each lane.
      ///
      /// The order of addition is not specified.
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
      /// Computes the product of multiple SIMD vectors for each lane.
      ///
      /// The order of multiplication is not specified.
      #[inline]
      fn product<I: Iterator<Item = Rhs>>(iter: I) -> Self {
        let mut total = Self::from(1.0);
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
              <$UintT as $Trait>::fmt(&x.to_bits(), f)?;
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
        $Simd::from_bits(self).select(if_true, if_false)
      }
    }

    impl Select<$UintSimd> for $Simd {
      #[inline]
      fn select(self, if_true: $UintSimd, if_false: $UintSimd) -> $UintSimd {
        self.to_bits().select(if_true, if_false)
      }
    }

    impl Select<$Simd> for $IntSimd {
      #[inline]
      fn select(self, if_true: $Simd, if_false: $Simd) -> $Simd {
        $Simd::from_bits(self.cast_unsigned()).select(if_true, if_false)
      }
    }

    impl Select<$IntSimd> for $Simd {
      #[inline]
      fn select(self, if_true: $IntSimd, if_false: $IntSimd) -> $IntSimd {
        self.to_bits().cast_signed().select(if_true, if_false)
      }
    }

    /// The following functionality exists for all SIMD vectors of floats.
    impl $Simd {
      /// A SIMD vector with all elements set to `1.0`.
      pub const ONE: Self = Self::splat(1.0);

      /// A SIMD vector with all elements set to `0.5`.
      pub const HALF: Self = Self::splat(0.5);

      /// A SIMD vector with all elements set to `0.0`.
      pub const ZERO: Self = Self::splat(0.0);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::EPSILON`].")]
      pub const EPSILON: Self = Self::splat($T::EPSILON);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::MIN`].")]
      pub const MIN: Self = Self::splat($T::MIN);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::MIN_POSITIVE`].")]
      pub const MIN_POSITIVE: Self = Self::splat($T::MIN_POSITIVE);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::MAX`].")]
      pub const MAX: Self = Self::splat($T::MAX);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::NAN`].")]
      pub const NAN: Self = Self::splat($T::NAN);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::INFINITY`].")]
      pub const INFINITY: Self = Self::splat($T::INFINITY);

      #[doc = concat!("A SIMD vector with all elements set to [`", stringify!($T) ,"::NEG_INFINITY`].")]
      pub const NEG_INFINITY: Self = Self::splat($T::NEG_INFINITY);

      /// A SIMD vector with all elements set to [Euler's number (e)].
      ///
      #[doc = concat!("[Euler's number (e)]: core::", stringify!($T), "::consts::E")]
      pub const E: Self = Self::splat(core::$T::consts::E);

      /// A SIMD vector with all elements set to [1/π].
      ///
      #[doc = concat!("[1/π]: core::", stringify!($T), "::consts::FRAC_1_PI")]
      pub const FRAC_1_PI: Self = Self::splat(core::$T::consts::FRAC_1_PI);

      /// A SIMD vector with all elements set to [2/π].
      ///
      #[doc = concat!("[2/π]: core::", stringify!($T), "::consts::FRAC_2_PI")]
      pub const FRAC_2_PI: Self = Self::splat(core::$T::consts::FRAC_2_PI);

      /// A SIMD vector with all elements set to [2/sqrt(π)].
      ///
      #[doc = concat!("[2/sqrt(π)]: core::", stringify!($T), "::consts::FRAC_2_SQRT_PI")]
      pub const FRAC_2_SQRT_PI: Self =
        Self::splat(core::$T::consts::FRAC_2_SQRT_PI);

      /// A SIMD vector with all elements set to [1/sqrt(2)].
      ///
      #[doc = concat!("[1/sqrt(2)]: core::", stringify!($T), "::consts::FRAC_1_SQRT_2")]
      pub const FRAC_1_SQRT_2: Self =
        Self::splat(core::$T::consts::FRAC_1_SQRT_2);

      /// A SIMD vector with all elements set to [π/2].
      ///
      #[doc = concat!("[π/2]: core::", stringify!($T), "::consts::FRAC_PI_2")]
      pub const FRAC_PI_2: Self = Self::splat(core::$T::consts::FRAC_PI_2);

      /// A SIMD vector with all elements set to [π/3].
      ///
      #[doc = concat!("[π/3]: core::", stringify!($T), "::consts::FRAC_PI_3")]
      pub const FRAC_PI_3: Self = Self::splat(core::$T::consts::FRAC_PI_3);

      /// A SIMD vector with all elements set to [π/4].
      ///
      #[doc = concat!("[π/4]: core::", stringify!($T), "::consts::FRAC_PI_4")]
      pub const FRAC_PI_4: Self = Self::splat(core::$T::consts::FRAC_PI_4);

      /// A SIMD vector with all elements set to [π/6].
      ///
      #[doc = concat!("[π/6]: core::", stringify!($T), "::consts::FRAC_PI_6")]
      pub const FRAC_PI_6: Self = Self::splat(core::$T::consts::FRAC_PI_6);

      /// A SIMD vector with all elements set to [π/8].
      ///
      #[doc = concat!("[π/8]: core::", stringify!($T), "::consts::FRAC_PI_8")]
      pub const FRAC_PI_8: Self = Self::splat(core::$T::consts::FRAC_PI_8);

      /// A SIMD vector with all elements set to [ln(2)].
      ///
      #[doc = concat!("[ln(2)]: core::", stringify!($T), "::consts::LN_2")]
      pub const LN_2: Self = Self::splat(core::$T::consts::LN_2);

      /// A SIMD vector with all elements set to [ln(10)].
      ///
      #[doc = concat!("[ln(10)]: core::", stringify!($T), "::consts::LN_10")]
      pub const LN_10: Self = Self::splat(core::$T::consts::LN_10);

      /// A SIMD vector with all elements set to [log<sub>2</sub>(e)].
      ///
      #[doc = concat!("[log<sub>2</sub>(e)]: core::", stringify!($T), "::consts::LOG2_E")]
      pub const LOG2_E: Self = Self::splat(core::$T::consts::LOG2_E);

      /// A SIMD vector with all elements set to [log<sub>10</sub>(e)].
      ///
      #[doc = concat!("[log<sub>10</sub>(e)]: core::", stringify!($T), "::consts::LOG10_E")]
      pub const LOG10_E: Self = Self::splat(core::$T::consts::LOG10_E);

      /// A SIMD vector with all elements set to [log<sub>10</sub>(2)].
      ///
      #[doc = concat!("[log<sub>10</sub>(2)]: core::", stringify!($T), "::consts::LOG10_2")]
      pub const LOG10_2: Self = Self::splat(core::$T::consts::LOG10_2);

      /// A SIMD vector with all elements set to [log<sub>2</sub>(10)].
      ///
      #[doc = concat!("[log<sub>2</sub>(10)]: core::", stringify!($T), "::consts::LOG2_10")]
      pub const LOG2_10: Self = Self::splat(core::$T::consts::LOG2_10);

      /// A SIMD vector with all elements set to [Archimedes’ constant (π)].
      ///
      #[doc = concat!("[Archimedes’ constant (π)]: core::", stringify!($T), "::consts::PI")]
      pub const PI: Self = Self::splat(core::$T::consts::PI);

      /// A SIMD vector with all elements set to [sqrt(2)].
      ///
      #[doc = concat!("[sqrt(2)]: core::", stringify!($T), "::consts::SQRT_2")]
      pub const SQRT_2: Self = Self::splat(core::$T::consts::SQRT_2);

      /// A SIMD vector with all elements set to [the full circle constant (τ)].
      ///
      /// Equal to 2π.
      ///
      #[doc = concat!("[the full circle constant (τ)]: core::", stringify!($T), "::consts::TAU")]
      pub const TAU: Self = Self::splat(core::$T::consts::TAU);

      /// Returns a [mask] that checks if each element is NaN.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_nan

      /// Returns a [mask] that checks if each element is infinity (either
      /// positive or negative).
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_inf

      /// Returns a [mask] that checks if each element is neither infinite nor
      /// NaN.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_finite

      /// Returns a [mask] that checks if each element has a positive sign,
      /// including `+0.0`, NaNs with positive sign bit and positive infinity.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_sign_positive

      /// Returns a [mask] that checks if each element has a negative sign,
      /// including `-0.0`, NaNs with negative sign bit and negative infinity.
      ///
      /// [mask]: crate#masks
      #[must_use]
      $fn_is_sign_negative

      /// Returns the reciprocal (inverse) of a number, `1/x`.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      ///
      #[doc = concat!(
        "To compute the reciprocal deterministically, use `",
        stringify!($Simd),
        "::ONE / x`."
      )]
      #[must_use]
      $fn_recip

      /// Returns the square root of the reciprocal (inverse) of a number,
      /// `sqrt(1/x)`.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_recip_sqrt

      /// Converts radians to degrees.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn to_degrees(self) -> Self {
        const RAD_TO_DEG_RATIO: $Simd = $Simd::splat(180.0 / core::$T::consts::PI);
        self * RAD_TO_DEG_RATIO
      }

      /// Converts degrees to radians.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn to_radians(self) -> Self {
        const DEG_TO_RAD_RATIO: $Simd = $Simd::splat(core::$T::consts::PI / 180.0);
        self * DEG_TO_RAD_RATIO
      }

      /// Returns the maximum between each element of `self` and the
      /// corresponding element of `other`, ignoring NaN.
      ///
      /// For each lane, if exactly one of the arguments is NaN, then the other
      /// argument is returned. If both arguments are NaN, the return value is
      /// NaN. If the inputs compare equal (such as for the case of `+0.0` and
      /// `-0.0`), either input may be returned non-deterministically.
      ///
      /// See [`fast_max`] for a faster variant that does not handle NaNs.
      ///
      /// [`fast_max`]: Self::fast_max
      #[must_use]
      $fn_max

      /// Returns the maximum between each element of `self` and the
      /// corresponding element of `other`, not specifying behavior for NaNs.
      ///
      /// For each lane, if both arguments are NaN, the return value is NaN. If
      /// the inputs compare equal (such as for the case of `+0.0` and `-0.0`),
      /// or if exactly one of the arguments is NaN, either input may be
      /// returned non-deterministically.
      ///
      /// See [`max`] for a slower variant that does handle NaNs.
      ///
      /// [`max`]: Self::max
      #[must_use]
      $fn_fast_max

      /// Returns the minimum between each element of `self` and the
      /// corresponding element of `other`, ignoring NaN.
      ///
      /// For each lane, if exactly one of the arguments is NaN, then the other
      /// argument is returned. If both arguments are NaN, the return value is
      /// NaN. If the inputs compare equal (such as for the case of `+0.0` and
      /// `-0.0`), either input may be returned non-deterministically.
      ///
      /// See [`fast_min`] for a faster variant that does not handle NaNs.
      ///
      /// [`fast_min`]: Self::fast_min
      #[must_use]
      $fn_min

      /// Returns the minimum between each element of `self` and the
      /// corresponding element of `other`, not specifying behavior for NaNs.
      ///
      /// For each lane, if both arguments are NaN, the return value is NaN. If
      /// the inputs compare equal (such as for the case of `+0.0` and `-0.0`),
      /// or if exactly one of the arguments is NaN, either input may be
      /// returned non-deterministically.
      ///
      /// See [`min`] for a slower variant that does handle NaNs.
      ///
      /// [`min`]: Self::min
      #[must_use]
      $fn_fast_min

      /// Calculates the midpoint (average) between `self` and `other`.
      ///
      /// This returns NaN when *either* argument is NaN or if a combination of
      /// +inf and -inf is provided as arguments.
      ///
      /// This function currently returns a less precise result than
      #[doc = concat!("[`", stringify!($T), "::midpoint`]")]
      /// in order to gain performance, but this may change in the future.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn midpoint(self, other: Self) -> Self {
        (self + other) * 0.5
      }

      /// Raw transmutation to unsigned integer vector.
      ///
      /// Note that this function preserves the *bitwise* value, and not the
      /// numeric value.
      #[inline]
      #[must_use]
      pub const fn to_bits(self) -> $UintSimd {
        // SAFETY: Both types accept all bit-patterns and only contain
        // initialized memory.
        unsafe { core::mem::transmute::<$Simd, $UintSimd>(self) }
      }

      /// Raw transmutation from unsigned integer vector.
      ///
      /// Note that this function preserves the *bitwise* value, and not the
      /// numeric value.
      #[inline]
      #[must_use]
      pub const fn from_bits(bits: $UintSimd) -> Self {
        // SAFETY: Both types accept all bit-patterns and only contain
        // initialized memory.
        unsafe { core::mem::transmute::<$UintSimd, $Simd>(bits) }
      }

      /// Restrict a value to a certain interval unless it is NaN.
      ///
      /// If `self`, `min` or `max` are NaN, the result is NaN. If `min > max`,
      /// the result is `min`. If inputs compare equal (such as for the case of
      /// `+0.0` and `-0.0`), either input may be returned
      /// non-deterministically.
      ///
      /// See [`fast_clamp`] for a faster variant that does not handle `min` or
      /// `max` being NaN.
      ///
      /// [`fast_clamp`]: Self::fast_clamp
      #[must_use]
      $fn_clamp

      /// Restrict a value to a certain interval unless it is NaN.
      ///
      /// If `self` is NaN, the result is NaN. If `min > max`, the result is
      /// `min`. If inputs compare equal (such as for the case of
      /// `+0.0` and `-0.0`), or if `min` or `max` are NaN, any input may be
      /// returned non-deterministically.
      ///
      /// See [`clamp`] for a slower variant that also handles `min` or `max`
      /// being NaN.
      ///
      /// [`clamp`]: Self::clamp
      #[must_use]
      $fn_fast_clamp

      /// Computes the absolute value of `self`.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_abs

      /// Returns numbers representing the sign of each element.
      ///
      /// - `1.0` if the element is positive, `+0.0`, or `INFINITY`
      /// - `-1.0` if the element is negative, `-0.0`, or `NEG_INFINITY`
      /// - NaN if the element is NaN
      ///
      /// This matches the behavior of [`f32::signum`].
      ///
      /// [`f32::signum`]: https://doc.rust-lang.org/std/primitive.f32.html#method.signum
      #[inline]
      #[must_use]
      pub fn signum(self) -> Self {
        Self::ONE | self & -Self::ZERO | self.is_nan()
      }

      /// Returns numbers composed of the magnitudes of `self` and the signs of
      /// `sign`.
      ///
      /// Equal to `self` if the sign of `self` and `sign` are the same,
      /// otherwise equal to `-self`. Even if `self` or `sign` are NaN, the
      /// result is the exact bit pattern of `self` with the sign bit of `sign`.
      #[inline]
      #[must_use]
      pub fn copysign(self, sign: Self) -> Self {
        Self::splat(-0.0).bitselect(sign, self)
      }

      /// Flips the sign of `self` based on the sign of `sign`.
      ///
      /// If `sign` has a positive sign, the result is `self`. If `sign` has a
      /// negative sign, the result is `-self`. Even if `self` or `sign` are
      /// NaN, the result is the exact bit pattern of `self` with a sign flipped
      /// based on the sign bit of `sign`.
      #[inline]
      #[must_use]
      pub fn flip_signs(self, sign: Self) -> Self {
        self ^ (sign & Self::from(-0.0))
      }

      /// Returns the largest integer less than or equal to each input element.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_floor

      /// Returns the smallest integer greater than or equal to each input
      /// element.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_ceil

      /// Returns the nearest integer to each input element. If a value is
      /// half-way between two integers, round away from `0.0`.
      ///
      /// This function always returns the precise result.
      ///
      /// For most targets architectures, [`round`] is slower than
      /// [`round_ties_even`]. If you do not care about the difference, consider
      /// using that instead.
      ///
      /// [`round`]: Self::round
      /// [`round_ties_even`]: Self::round_ties_even
      #[must_use]
      $fn_round

      /// Returns the nearest integer to each input element.
      ///
      /// The result for values half-way between two integers is currently not
      /// specified.
      ///
      /// This saturates out of range values and turns NaNs to `0`. See
      /// [`fast_round_int`] for a faster variant that does not handle out of
      /// range values or NaNs.
      ///
      /// [`fast_round_int`]: Self::fast_round_int
      #[must_use]
      $fn_round_int

      /// Returns the nearest integer to each input element.
      ///
      /// The result for values half-way between two integers is currently not
      /// specified.
      ///
      /// This function does not handle out of range values or NaNs. See
      /// [`round_int`] for a slower variant that does handle out of range
      /// values and NaNs.
      ///
      /// [`round_int`]: Self::round_int
      #[must_use]
      $fn_fast_round_int

      /// Returns the nearest integer to each input element. Rounds half-way
      /// cases to the number with an even least significant digit.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_round_ties_even

      /// Returns the integer part of each input element. This means that
      /// non-integer numbers are always truncated towards zero.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_trunc

      /// Returns the integer part of each input element. This means that
      /// non-integer numbers are always truncated towards zero.
      ///
      /// This saturates out of range values and turns NaNs to `0`. See
      /// [`fast_trunc_int`] for a faster variant that does not handle out of
      /// range values or NaNs.
      ///
      /// [`fast_trunc_int`]: Self::fast_trunc_int
      #[must_use]
      $fn_trunc_int

      /// Returns the integer part of each input element. This means that
      /// non-integer numbers are always truncated towards zero.
      ///
      /// This function does not handle out of range values or NaNs. See
      /// [`trunc_int`] for a slower variant that does handle out of range
      /// values and NaNs.
      ///
      /// [`trunc_int`]: Self::trunc_int
      #[must_use]
      $fn_fast_trunc_int

      /// Returns the fractional part of each input element.
      ///
      /// This function always returns the precise result.
      #[inline]
      #[must_use]
      pub fn fract(self) -> Self {
        self - self.trunc()
      }

      /// Fused multiply-add. Computes `(self * a) + b`.
      ///
      /// If there is hardware FMA support, this computes the result with only
      /// one rounding error. If not, this falls back to separate multiply and
      /// add operations, resulting in two rounding errors. Note that in the
      /// future, this function may change to always having one rounding error,
      /// at the cost of worse performance.
      #[must_use]
      $fn_mul_add

      /// Fused multiply-sub. Computes `(self * a) - b`.
      ///
      /// If there is hardware FMA support, this computes the result with only
      /// one rounding error. If not, this falls back to separate multiply and
      /// add operations, resulting in two rounding errors. Note that in the
      /// future, this function may change to always having one rounding error,
      /// at the cost of worse performance.
      #[must_use]
      $fn_mul_sub

      /// Fused multiply-negate-add. Computes `-(self * a) + b`.
      ///
      /// If there is hardware FMA support, this computes the result with only
      /// one rounding error. If not, this falls back to separate multiply and
      /// add operations, resulting in two rounding errors. Note that in the
      /// future, this function may change to always having one rounding error,
      /// at the cost of worse performance.
      #[must_use]
      $fn_mul_neg_add

      /// Fused multiply-negate-sub. Computes `-(self * a) - b`.
      ///
      /// If there is hardware FMA support, this computes the result with only
      /// one rounding error. If not, this falls back to separate multiply and
      /// add operations, resulting in two rounding errors. Note that in the
      /// future, this function may change to always having one rounding error,
      /// at the cost of worse performance.
      #[must_use]
      $fn_mul_neg_sub

      /// Calculates Euclidean division, the matching function for
      /// [`rem_euclid`].
      ///
      /// This computes the integer `n` such that
      /// `self = n * rhs + self.rem_euclid(rhs)`. In other words, the result is
      /// `self / rhs` rounded to the integer `n` such that `self >= n * rhs`.
      ///
      /// This function is not guaranteed to exactly match
      #[doc = concat!("[`", stringify!($T), "::div_euclid`].")]
      ///
      /// [`rem_euclid`]: Self::rem_euclid
      #[inline]
      #[must_use]
      pub fn div_euclid(self, rhs: Self) -> Self {
        let q = (self / rhs).trunc();
        (self % rhs)
          .simd_lt(Self::ZERO)
          .select(rhs.simd_gt(Self::ZERO).select(q - Self::ONE, q + Self::ONE), q)
      }

      /// Calculates the least nonnegative remainder of `self` when divided by
      /// `rhs`.
      ///
      /// In particular, the return value `r` satisfies `0.0 <= r < rhs.abs()` in
      /// most cases. However, due to a floating point round-off error it can
      /// result in `r == rhs.abs()`, violating the mathematical definition, if
      /// `self` is much smaller than `rhs.abs()` in magnitude and `self < 0.0`.
      /// This result is not an element of the function's codomain, but it is the
      /// closest floating point number in the real numbers and thus fulfills the
      /// property `self == self.div_euclid(rhs) * rhs + self.rem_euclid(rhs)`
      /// approximately.
      ///
      /// This function is not guaranteed to exactly match
      #[doc = concat!("[`", stringify!($T), "::rem_euclid`].")]
      #[inline]
      #[must_use]
      pub fn rem_euclid(self, rhs: Self) -> Self {
        let r = self % rhs;
        r.simd_lt(Self::ZERO).select(r + rhs.abs(), r)
      }

      /// Raises each element of the number `self` to the corresponding element
      /// of the floating point power `n`.
      ///
      /// This function cannot be named simply `powf`, because a now deprecated
      /// function already uses that name.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_powf_simd

      /// Returns the square root of a number for each input element.
      ///
      /// Returns NaN if `self` is a negative number other than `-0.0`.
      ///
      /// This function always returns the precise result.
      #[must_use]
      $fn_sqrt

      /// Returns `e^(self)`, (the exponential function) for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_exp

      /// Returns `2^(self)` for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_exp2

      /// Returns the natural logarithm of a number for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_ln

      /// Returns the base 2 logarithm of a number for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn log2(self) -> Self {
        Self::ln(self) * Self::LOG2_E
      }

      /// Returns the base 10 logarithm of a number for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn log10(self) -> Self {
        Self::ln(self) * Self::LOG10_E
      }

      /// Returns the cube root of a number for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_cbrt

      /// Computes the sine of a number (in radians) for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn sin(self) -> Self {
        let (s, _) = self.sin_cos();
        s
      }

      /// Computes the cosine of a number (in radians) for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn cos(self) -> Self {
        let (_, c) = self.sin_cos();
        c
      }

      /// Computes the tangent of a number (in radians) for each input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[inline]
      #[must_use]
      pub fn tan(self) -> Self {
        let (s, c) = self.sin_cos();
        s / c
      }

      /// Computes the arcsine of a number for each input element. Return value
      /// is in radians in the range [-pi/2, pi/2] or NaN if the number is
      /// outside the range [-1, 1].
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_asin

      /// Computes the arccosine of a number for each input element. Return
      /// value is in radians in the range [0, pi] or NaN if the number is
      /// outside the range [-1, 1].
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_acos

      /// Computes the arctangent of a number for each input element. Return
      /// value is in radians in the range [-pi/2, pi/2].
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_atan

      /// Computes the four quadrant arctangent of each element of `self` (`y`)
      /// and the corresponding element of `other` (`x`) in radians.
      ///
      /// | `x`     | `y`     | Piecewise Definition | Range         |
      /// |---------|---------|----------------------|---------------|
      /// | `>= +0` | `>= +0` | `arctan(y/x)`        | `[+0, +pi/2]` |
      /// | `>= +0` | `<= -0` | `arctan(y/x)`        | `[-pi/2, -0]` |
      /// | `<= -0` | `>= +0` | `arctan(y/x) + pi`   | `[+pi/2, +pi]`|
      /// | `<= -0` | `<= -0` | `arctan(y/x) - pi`   | `[-pi, -pi/2]`|
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_atan2

      /// Simultaneously computes the sine and cosine of a number `x` for each
      /// input element. Returns `(sin(x), cos(x))`.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_sin_cos

      /// Simultaneously computes the arcsine and arccosine of a number `x` for
      /// each input element. Returns `(asin(x), acos(x))`.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_asin_acos

      /// Returns `e^(self) - 1` for each input element in a way that is
      /// accurate even if a number is close to zero.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_exp_m1

      /// Returns `ln(1+n)` (natural logarithm) for each input element more
      /// accurately than if the operations were performed separately.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_ln_1p

      /// Returns the hyperbolic sine (`(e^self - e^(-self))/2`) for each input
      /// element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_sinh

      /// Returns the hyperbolic cosine (`(e^self + e^(-self))/2`) for each
      /// input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_cosh

      /// Returns the hyperbolic tangent (`sinh(self)/cosh(self)`) for each
      /// input element.
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[must_use]
      $fn_tanh

      /// Raises each element of the number `self` to the corresponding element
      /// of the floating point power `n`.
      ///
      /// This function has been renamed to [`powf_simd`].
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      ///
      /// [`powf_simd`]: Self::powf_simd
      #[deprecated(since = "1.6.0", note = "renamed to `powf_simd`")]
      #[inline]
      #[must_use]
      pub fn $old_powf_simd_fn_name(self, n: Self) -> Self {
        self.powf_simd(n)
      }

      /// Raises each element of the number `self` to the scalar floating point
      /// power `n`.
      ///
      /// This function has been deprecated because it raises all elements of
      /// `x` to the same power, even though that brings no performance benefit.
      #[doc = concat!("Use `x.powf_simd(", stringify!($Simd), "::splat(n))` instead.")]
      ///
      /// # Unspecified precision
      ///
      /// The precision of this function is non-deterministic. This means it
      /// varies by platform, version, and can even differ within the same
      /// execution from one invocation to the next.
      #[deprecated(since = "1.6.0", note = "use `x.powf_simd(splat(n))` instead")]
      #[inline]
      #[must_use]
      pub fn powf(self, n: $T) -> Self {
        self.powf_simd(Self::splat(n))
      }
    }
  };
}
