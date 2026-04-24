use core::fmt::Debug;


macro_rules! __declare_const_marker_serde_impls {
    ($struct:ident($ty:ty) $deser_type:ident) => {
        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "serde")))]
        impl<'de, const VAL: $ty> serde_::Deserialize<'de> for $struct<VAL> {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: serde_::Deserializer<'de>,
            {
                let this = $crate::const_marker::ValidateEquals { equals_to: &VAL };
                crate::const_marker::__const_marker_deserialize_seed_impl!{
                    $deser_type => this, deserializer, $ty
                }?;

                Ok(Self)
            }
        }

        #[cfg(feature = "adt_const_marker")]
        impl<'de> serde_::de::DeserializeSeed<'de> 
        for $crate::const_marker::ValidateEquals<&'static $ty> 
        {
            type Value = ();

            fn deserialize<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
            where D: serde_::Deserializer<'de>
            {
                crate::const_marker::__const_marker_deserialize_seed_impl!{
                    $deser_type => self, deserializer, $ty
                }
            }
        }

        #[cfg_attr(feature = "docsrs", doc(cfg(feature = "serde")))]
        impl<const VAL: $ty> serde_::Serialize for $struct<VAL> {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: serde_::Serializer,
            {
                serde_::Serialize::serialize(&VAL, serializer)
            }
        }
    }
} 

pub(crate) use __declare_const_marker_serde_impls;

//////////////

macro_rules! __const_marker_deserialize_seed_impl {
    ($(int)? $(primitive)? => $seed:ident, $deser:ident, $ty:ty) => {
        CheckExpected {
            expected: $seed.equals_to,
            found: &<$ty as serde_::Deserialize>::deserialize($deser)?,
        }.call()
    };
    (slice => $seed:ident, $deser:ident, $ty:ty) => {
        $deser.deserialize_seq($seed)
    };
    (str => $seed:ident, $deser:ident, $ty:ty) => {
        $deser.deserialize_str($seed)
    };
}


pub(crate) use __const_marker_deserialize_seed_impl;


//////////////


pub(crate) struct CheckExpected<'a, T: ?Sized> {
    pub(crate) expected: &'a T,
    pub(crate) found: &'a T,
}

impl<T> CheckExpected<'_, T> 
where
    T: PartialEq + Debug + ?Sized
{
    pub(crate) fn call<E>(self) -> Result<(), E>
    where
        E: serde_::de::Error,
    {
        let Self { expected, found } = self;

        if found == expected {
            Ok(())
        } else {
            Err(serde_::de::Error::custom(format_args!(
                "expected `{:?}` found `{:?}`",
                self.expected,
                self.found,
            )))
        }
    }
}


//////////////

pub(crate) struct ValidateEquals<T> { 
    pub(crate) equals_to: T,
}


