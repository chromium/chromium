use crate::{NotNan, OrderedFloat};
use num_traits::float::FloatCore;
use rkyv_08::{rancor::Fallible, Archive, Deserialize, Place, Portable, Serialize};

// Safety: OrderedFloat and NotNan are #[repr(transparent)] wrappers,
// so they have the same layout as their inner type.
unsafe impl<T: Portable> Portable for OrderedFloat<T> {}
unsafe impl<T: Portable> Portable for NotNan<T> {}

impl<T: FloatCore + Archive> Archive for OrderedFloat<T>
where
    T::Archived: Portable,
{
    type Archived = OrderedFloat<T::Archived>;
    type Resolver = T::Resolver;

    fn resolve(&self, resolver: Self::Resolver, out: Place<Self::Archived>) {
        self.0.resolve(resolver, unsafe { out.cast_unchecked() });
    }
}

impl<T: FloatCore + Serialize<S>, S: Fallible + ?Sized> Serialize<S> for OrderedFloat<T> {
    fn serialize(&self, s: &mut S) -> Result<Self::Resolver, S::Error> {
        self.0.serialize(s)
    }
}

impl<T: FloatCore, AT: Deserialize<T, D>, D: Fallible + ?Sized> Deserialize<OrderedFloat<T>, D>
    for OrderedFloat<AT>
{
    fn deserialize(&self, d: &mut D) -> Result<OrderedFloat<T>, D::Error> {
        self.0.deserialize(d).map(OrderedFloat)
    }
}

impl<T: FloatCore + Archive> Archive for NotNan<T>
where
    T::Archived: Portable,
{
    type Archived = NotNan<T::Archived>;
    type Resolver = T::Resolver;

    fn resolve(&self, resolver: Self::Resolver, out: Place<Self::Archived>) {
        self.0.resolve(resolver, unsafe { out.cast_unchecked() });
    }
}

impl<T: FloatCore + Serialize<S>, S: Fallible + ?Sized> Serialize<S> for NotNan<T> {
    fn serialize(&self, s: &mut S) -> Result<Self::Resolver, S::Error> {
        self.0.serialize(s)
    }
}

impl<T: FloatCore, AT: Deserialize<T, D>, D: Fallible + ?Sized> Deserialize<NotNan<T>, D>
    for NotNan<AT>
{
    fn deserialize(&self, d: &mut D) -> Result<NotNan<T>, D::Error> {
        self.0.deserialize(d).map(NotNan)
    }
}

macro_rules! rkyv_08_eq_ord {
    ($main:ident, $float:ty, $rend:ty) => {
        impl PartialEq<$main<$float>> for $main<$rend> {
            fn eq(&self, other: &$main<$float>) -> bool {
                other.eq(&self.0.to_native())
            }
        }
        impl PartialEq<$main<$rend>> for $main<$float> {
            fn eq(&self, other: &$main<$rend>) -> bool {
                self.eq(&other.0.to_native())
            }
        }

        impl PartialOrd<$main<$float>> for $main<$rend> {
            fn partial_cmp(&self, other: &$main<$float>) -> Option<core::cmp::Ordering> {
                self.0.to_native().partial_cmp(other)
            }
        }

        impl PartialOrd<$main<$rend>> for $main<$float> {
            fn partial_cmp(&self, other: &$main<$rend>) -> Option<core::cmp::Ordering> {
                other
                    .0
                    .to_native()
                    .partial_cmp(self)
                    .map(core::cmp::Ordering::reverse)
            }
        }
    };
}

rkyv_08_eq_ord! { OrderedFloat, f32, rkyv_08::rend::f32_le }
rkyv_08_eq_ord! { OrderedFloat, f32, rkyv_08::rend::f32_be }
rkyv_08_eq_ord! { OrderedFloat, f64, rkyv_08::rend::f64_le }
rkyv_08_eq_ord! { OrderedFloat, f64, rkyv_08::rend::f64_be }
rkyv_08_eq_ord! { NotNan, f32, rkyv_08::rend::f32_le }
rkyv_08_eq_ord! { NotNan, f32, rkyv_08::rend::f32_be }
rkyv_08_eq_ord! { NotNan, f64, rkyv_08::rend::f64_le }
rkyv_08_eq_ord! { NotNan, f64, rkyv_08::rend::f64_be }

#[cfg(feature = "rkyv_08_ck")]
mod checkbytes {
    use super::*;
    use crate::FloatIsNan;
    use rkyv_08::bytecheck::CheckBytes;
    use rkyv_08::rancor::{fail, Source};

    unsafe impl<C, T> CheckBytes<C> for OrderedFloat<T>
    where
        C: Fallible + ?Sized,
        T: CheckBytes<C>,
    {
        #[inline]
        unsafe fn check_bytes(value: *const Self, context: &mut C) -> Result<(), C::Error> {
            T::check_bytes(value as *const T, context)
        }
    }

    macro_rules! impl_checkbytes_not_nan {
        ($rend:ty) => {
            unsafe impl<C> CheckBytes<C> for NotNan<$rend>
            where
                C: Fallible + ?Sized,
                C::Error: Source,
                $rend: CheckBytes<C>,
            {
                #[inline]
                unsafe fn check_bytes(value: *const Self, context: &mut C) -> Result<(), C::Error> {
                    <$rend>::check_bytes(value as *const $rend, context)?;
                    let val = &*(value as *const $rend);
                    if val.to_native().is_nan() {
                        fail!(FloatIsNan);
                    }
                    Ok(())
                }
            }
        };
    }

    impl_checkbytes_not_nan!(rkyv_08::rend::f32_le);
    impl_checkbytes_not_nan!(rkyv_08::rend::f32_be);
    impl_checkbytes_not_nan!(rkyv_08::rend::f64_le);
    impl_checkbytes_not_nan!(rkyv_08::rend::f64_be);
}

#[cfg(test)]
mod tests {
    use super::*;
    use rkyv_08::rancor::Error;
    use rkyv_08::util::Align;

    /// Serialize a value into an aligned buffer, returning the number of bytes used.
    fn serialize_into<T>(value: &T, buf: &mut Align<[u8; 64]>) -> usize
    where
        T: Archive,
        T: for<'a> Serialize<rkyv_08::rancor::Strategy<rkyv_08::ser::writer::Buffer<'a>, Error>>,
    {
        use rkyv_08::rancor::Strategy;
        use rkyv_08::ser::writer::Buffer;
        use rkyv_08::ser::{Positional, WriterExt};

        let mut writer = Buffer::from(&mut buf.0[..]);
        let mut s = Strategy::<_, Error>::wrap(&mut writer);
        let resolver = value.serialize(&mut s).unwrap();
        unsafe { s.resolve_aligned(value, resolver).unwrap() };
        s.pos()
    }

    fn test_roundtrip<T>(value: T)
    where
        T: Archive + PartialEq + core::fmt::Debug,
        T: for<'a> Serialize<rkyv_08::rancor::Strategy<rkyv_08::ser::writer::Buffer<'a>, Error>>,
        T::Archived: Portable + PartialEq<T> + core::fmt::Debug,
        T::Archived: Deserialize<T, rkyv_08::rancor::Strategy<(), Error>>,
    {
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&value, &mut buf);

        let archived = unsafe { rkyv_08::access_unchecked::<T::Archived>(&buf[..len]) };
        assert_eq!(archived, &value);

        let deser: T = rkyv_08::api::deserialize_using::<T, _, Error>(archived, &mut ()).unwrap();
        assert_eq!(deser, value);
    }

    #[test]
    fn ordered_float_roundtrip() {
        test_roundtrip(OrderedFloat(1.0f64));
        test_roundtrip(OrderedFloat(-1.0f64));
        test_roundtrip(OrderedFloat(0.0f64));
        test_roundtrip(OrderedFloat(-0.0f64));
        test_roundtrip(OrderedFloat(f64::INFINITY));
        test_roundtrip(OrderedFloat(f64::NEG_INFINITY));
        test_roundtrip(OrderedFloat(1.0f32));
        test_roundtrip(OrderedFloat(-1.0f32));
        test_roundtrip(OrderedFloat(0.0f32));
        test_roundtrip(OrderedFloat(f32::INFINITY));
        test_roundtrip(OrderedFloat(f32::NEG_INFINITY));
    }

    #[test]
    fn ordered_float_nan_roundtrip() {
        // NaN != NaN so we can't use test_roundtrip; check manually.
        let float = OrderedFloat(f64::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);
        let archived = unsafe {
            rkyv_08::access_unchecked::<OrderedFloat<rkyv_08::primitive::ArchivedF64>>(&buf[..len])
        };
        let deser: OrderedFloat<f64> =
            rkyv_08::api::deserialize_using::<_, _, Error>(archived, &mut ()).unwrap();
        assert!(deser.0.is_nan());

        let float = OrderedFloat(f32::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);
        let archived = unsafe {
            rkyv_08::access_unchecked::<OrderedFloat<rkyv_08::primitive::ArchivedF32>>(&buf[..len])
        };
        let deser: OrderedFloat<f32> =
            rkyv_08::api::deserialize_using::<_, _, Error>(archived, &mut ()).unwrap();
        assert!(deser.0.is_nan());
    }

    #[test]
    fn not_nan_roundtrip() {
        test_roundtrip(NotNan(1.0f64));
        test_roundtrip(NotNan(-1.0f64));
        test_roundtrip(NotNan(0.0f64));
        test_roundtrip(NotNan(-0.0f64));
        test_roundtrip(NotNan(f64::INFINITY));
        test_roundtrip(NotNan(f64::NEG_INFINITY));
        test_roundtrip(NotNan(1.0f32));
        test_roundtrip(NotNan(-1.0f32));
        test_roundtrip(NotNan(0.0f32));
        test_roundtrip(NotNan(f32::INFINITY));
        test_roundtrip(NotNan(f32::NEG_INFINITY));
    }

    #[test]
    fn archived_eq_ord_ordered_float() {
        let float = OrderedFloat(42.0f64);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);
        let archived = unsafe {
            rkyv_08::access_unchecked::<OrderedFloat<rkyv_08::primitive::ArchivedF64>>(&buf[..len])
        };

        // PartialEq: archived == native
        assert_eq!(archived, &float);
        assert_eq!(&float, archived);

        // PartialOrd: archived vs native
        let smaller = OrderedFloat(10.0f64);
        let larger = OrderedFloat(100.0f64);
        assert!(archived > &smaller);
        assert!(archived < &larger);
        assert!(&smaller < archived);
        assert!(&larger > archived);
    }

    #[test]
    fn archived_eq_ord_not_nan() {
        let float = NotNan(42.0f64);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);
        let archived = unsafe {
            rkyv_08::access_unchecked::<NotNan<rkyv_08::primitive::ArchivedF64>>(&buf[..len])
        };

        assert_eq!(archived, &float);
        assert_eq!(&float, archived);

        let smaller = NotNan(10.0f64);
        let larger = NotNan(100.0f64);
        assert!(archived > &smaller);
        assert!(archived < &larger);
        assert!(&smaller < archived);
        assert!(&larger > archived);
    }

    #[test]
    fn unchecked_access_allows_nan_in_not_nan() {
        // Serialize an OrderedFloat(NaN), then access those bytes as a NotNan
        // via the unchecked path. This demonstrates why CheckBytes validation
        // matters: without it, a NaN can sneak through.
        let nan = OrderedFloat(f64::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&nan, &mut buf);

        let archived = unsafe {
            rkyv_08::access_unchecked::<NotNan<rkyv_08::primitive::ArchivedF64>>(&buf[..len])
        };

        let deser: NotNan<f64> =
            rkyv_08::api::deserialize_using::<_, _, Error>(archived, &mut ()).unwrap();
        assert!(deser.into_inner().is_nan());
    }

    #[cfg(feature = "rkyv_08_ck")]
    #[test]
    fn checkbytes_rejects_nan_as_not_nan() {
        // Serialize an OrderedFloat(NaN), then use checked access to try to
        // interpret the bytes as NotNan. CheckBytes should reject this.
        let nan = OrderedFloat(f64::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&nan, &mut buf);

        let result = rkyv_08::api::low::access::<NotNan<rkyv_08::primitive::ArchivedF64>, Error>(
            &buf[..len],
        );
        assert!(result.is_err());
    }

    #[cfg(feature = "rkyv_08_ck")]
    #[test]
    fn checkbytes_rejects_nan_as_not_nan_f32() {
        let nan = OrderedFloat(f32::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&nan, &mut buf);

        let result = rkyv_08::api::low::access::<NotNan<rkyv_08::primitive::ArchivedF32>, Error>(
            &buf[..len],
        );
        assert!(result.is_err());
    }

    #[cfg(feature = "rkyv_08_ck")]
    #[test]
    fn checkbytes_accepts_valid_not_nan() {
        let float = NotNan(42.0f64);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);

        let result = rkyv_08::api::low::access::<NotNan<rkyv_08::primitive::ArchivedF64>, Error>(
            &buf[..len],
        );
        assert!(result.is_ok());
        let archived = result.unwrap();
        assert_eq!(archived, &float);
    }

    #[cfg(feature = "rkyv_08_ck")]
    #[test]
    fn checkbytes_accepts_ordered_float_nan() {
        // OrderedFloat allows NaN, so CheckBytes should accept it.
        let float = OrderedFloat(f64::NAN);
        let mut buf = Align([0u8; 64]);
        let len = serialize_into(&float, &mut buf);

        let result = rkyv_08::api::low::access::<
            OrderedFloat<rkyv_08::primitive::ArchivedF64>,
            Error,
        >(&buf[..len]);
        assert!(result.is_ok());
    }
}
