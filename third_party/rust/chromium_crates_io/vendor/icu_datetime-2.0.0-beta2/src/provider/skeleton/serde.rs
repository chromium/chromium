// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::format;
#[cfg(feature = "datagen")]
use alloc::string::ToString;
use core::convert::TryFrom;
use smallvec::SmallVec;

pub mod reference {
    use super::super::reference::Skeleton;
    use super::*;

    #[cfg(feature = "datagen")]
    use ::serde::{ser, Serialize};
    use serde::{de, Deserialize, Deserializer};
    /// This is an implementation of the serde deserialization visitor pattern.
    #[allow(clippy::upper_case_acronyms)]
    pub(super) struct DeserializeSkeletonUTS35String;

    impl de::Visitor<'_> for DeserializeSkeletonUTS35String {
        type Value = Skeleton;

        fn expecting(&self, formatter: &mut core::fmt::Formatter) -> core::fmt::Result {
            write!(formatter, "Expected to find a valid skeleton.")
        }

        /// A [`Skeleton`] serialized into a string follows UTS-35.
        /// <https://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table>
        /// This string consists of a symbol that is repeated N times. This string is
        /// deserialized here into the Skeleton format which is used in memory
        /// when working with formatting datetimes.
        fn visit_str<E>(self, skeleton_string: &str) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            Skeleton::try_from(skeleton_string).map_err(|err| {
                de::Error::invalid_value(
                    de::Unexpected::Other(&format!("{skeleton_string:?} {err}")),
                    &"field symbols representing a skeleton",
                )
            })
        }
    }

    impl<'de> Deserialize<'de> for Skeleton {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                deserializer.deserialize_str(DeserializeSkeletonUTS35String)
            } else {
                let sv = SmallVec::deserialize(deserializer)?;
                Ok(sv.into())
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for Skeleton {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                // Serialize into the UTS 35 string representation.
                let string = self.to_string();
                serializer.serialize_str(&string)
            } else {
                self.0.serialize(serializer)
            }
        }
    }
}

pub mod runtime {
    use super::super::runtime::Skeleton;
    use super::*;

    #[cfg(feature = "datagen")]
    use ::serde::{ser, Serialize};
    use serde::{de, Deserialize, Deserializer};
    use zerovec::ZeroVec;
    /// This is an implementation of the serde deserialization visitor pattern.
    #[allow(clippy::upper_case_acronyms)]
    struct DeserializeSkeletonUTS35String;

    impl<'de> de::Visitor<'de> for DeserializeSkeletonUTS35String {
        type Value = Skeleton<'de>;

        fn expecting(&self, formatter: &mut core::fmt::Formatter) -> core::fmt::Result {
            write!(formatter, "Expected to find a valid skeleton.")
        }

        fn visit_borrowed_str<E>(self, skeleton_string: &'de str) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            let reference_deserializer = super::reference::DeserializeSkeletonUTS35String;
            let skeleton = reference_deserializer.visit_str(skeleton_string)?;

            Ok(skeleton.into())
        }
    }

    impl<'de> Deserialize<'de> for Skeleton<'de> {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                deserializer.deserialize_str(DeserializeSkeletonUTS35String)
            } else {
                let zv = ZeroVec::deserialize(deserializer)?;
                Ok(zv.into())
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for Skeleton<'_> {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                // Serialize into the UTS 35 string representation.
                let string = self.to_string();
                serializer.serialize_str(&string)
            } else {
                self.0.serialize(serializer)
            }
        }
    }
}
