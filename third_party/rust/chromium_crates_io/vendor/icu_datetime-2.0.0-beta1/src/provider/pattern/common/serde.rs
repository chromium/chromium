// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::super::{PatternItem, TimeGranularity};
use ::serde::{de, Deserialize, Deserializer};
use alloc::{fmt, format, vec::Vec};

#[cfg(feature = "datagen")]
use ::serde::{ser, Serialize};

mod reference {
    use super::super::super::reference::Pattern;
    use super::*;

    /// A helper struct that is shaped exactly like `runtime::Pattern`
    /// and is used to aid in quick deserialization.
    #[derive(Debug, Clone, PartialEq, Deserialize)]
    #[cfg_attr(feature = "datagen", derive(Serialize))]
    struct PatternForSerde {
        items: Vec<PatternItem>,
        time_granularity: TimeGranularity,
    }

    impl From<PatternForSerde> for Pattern {
        fn from(pfs: PatternForSerde) -> Self {
            Self {
                items: pfs.items,
                time_granularity: pfs.time_granularity,
            }
        }
    }

    impl From<&Pattern> for PatternForSerde {
        fn from(pfs: &Pattern) -> Self {
            Self {
                items: pfs.items.clone(),
                time_granularity: pfs.time_granularity,
            }
        }
    }

    #[allow(clippy::upper_case_acronyms)]
    pub(crate) struct DeserializePatternUTS35String;

    impl de::Visitor<'_> for DeserializePatternUTS35String {
        type Value = Pattern;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            write!(formatter, "a valid pattern.")
        }

        fn visit_str<E>(self, pattern_string: &str) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            // Parse a string into a list of fields.
            pattern_string.parse().map_err(|err| {
                de::Error::invalid_value(
                    de::Unexpected::Other(&format!("{err}")),
                    &"a valid UTS 35 pattern string",
                )
            })
        }
    }

    impl<'de> Deserialize<'de> for Pattern {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                deserializer.deserialize_str(DeserializePatternUTS35String)
            } else {
                let pattern = PatternForSerde::deserialize(deserializer)?;
                Ok(Pattern::from(pattern))
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for Pattern {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                serializer.serialize_str(&self.to_runtime_pattern().to_string())
            } else {
                let pfs = PatternForSerde::from(self);
                pfs.serialize(serializer)
            }
        }
    }

    #[cfg(all(test, feature = "datagen"))]
    mod test {
        use super::*;

        #[test]
        fn reference_pattern_serde_human_readable_test() {
            let pattern: Pattern = "y-M-d HH:mm".parse().expect("Failed to parse pattern");
            let json = serde_json::to_string(&pattern).expect("Failed to serialize pattern");
            let result: Pattern =
                serde_json::from_str(&json).expect("Failed to deserialize pattern");
            assert_eq!(pattern, result);
        }

        #[test]
        fn reference_pattern_serde_bincode_test() {
            let pattern: Pattern = "y-M-d HH:mm".parse().expect("Failed to parse pattern");
            let bytes = bincode::serialize(&pattern).expect("Failed to serialize pattern");
            let result: Pattern =
                bincode::deserialize(&bytes).expect("Failed to deserialize pattern");
            assert_eq!(pattern, result);
        }
    }
}

mod runtime {
    use super::super::super::{runtime::Pattern, runtime::PatternMetadata, PatternItem};
    use super::*;
    use zerovec::ZeroVec;

    /// A helper struct that is shaped exactly like `runtime::Pattern`
    /// and is used to aid in quick deserialization.
    #[derive(Debug, Clone, PartialEq, Deserialize)]
    #[cfg_attr(feature = "datagen", derive(Serialize))]
    struct PatternForSerde<'data> {
        #[serde(borrow)]
        pub items: ZeroVec<'data, PatternItem>,
        pub time_granularity: TimeGranularity,
    }

    impl<'data> From<PatternForSerde<'data>> for Pattern<'data> {
        fn from(pfs: PatternForSerde<'data>) -> Self {
            Self {
                items: pfs.items,
                metadata: PatternMetadata::from_time_granularity(pfs.time_granularity),
            }
        }
    }

    #[allow(clippy::upper_case_acronyms)]
    struct DeserializePatternUTS35String;

    impl<'de> de::Visitor<'de> for DeserializePatternUTS35String {
        type Value = Pattern<'de>;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            write!(formatter, "a valid pattern.")
        }

        fn visit_str<E>(self, pattern_string: &str) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            // Parse a string into a list of fields.
            let reference_deserializer = super::reference::DeserializePatternUTS35String;
            let pattern = reference_deserializer.visit_str(pattern_string)?;

            Ok(Self::Value::from(&pattern))
        }
    }

    impl<'de: 'data, 'data> Deserialize<'de> for Pattern<'data> {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                deserializer.deserialize_str(DeserializePatternUTS35String)
            } else {
                let pattern = PatternForSerde::deserialize(deserializer)?;
                Ok(Pattern::from(pattern))
            }
        }
    }

    #[cfg(feature = "datagen")]
    impl Serialize for Pattern<'_> {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                serializer.serialize_str(&self.to_string())
            } else {
                let pfs = PatternForSerde {
                    items: self.items.clone(),
                    time_granularity: self.metadata.time_granularity(),
                };
                pfs.serialize(serializer)
            }
        }
    }

    #[cfg(all(test, feature = "datagen"))]
    mod test {
        use super::*;

        #[test]
        fn runtime_pattern_serde_human_readable_test() {
            let pattern: Pattern = "y-M-d HH:mm".parse().expect("Failed to parse pattern");
            let json = serde_json::to_string(&pattern).expect("Failed to serialize pattern");
            let result: Pattern =
                serde_json::from_str(&json).expect("Failed to deserialize pattern");
            assert_eq!(pattern, result);
        }

        #[test]
        fn runtime_pattern_serde_bincode_test() {
            let pattern: Pattern = "y-M-d HH:mm".parse().expect("Failed to parse pattern");
            let bytes = bincode::serialize(&pattern).expect("Failed to serialize pattern");
            let result: Pattern =
                bincode::deserialize(&bytes).expect("Failed to deserialize pattern");
            assert_eq!(pattern, result);
        }
    }

    mod generic {
        use super::super::super::super::runtime::GenericPattern;
        use super::*;

        #[allow(clippy::upper_case_acronyms)]
        struct DeserializeGenericPatternUTS35String;

        impl<'de> de::Visitor<'de> for DeserializeGenericPatternUTS35String {
            type Value = GenericPattern<'de>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "a valid pattern.")
            }

            fn visit_str<E>(self, pattern_string: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                // Parse a string into a list of fields.
                let pattern = pattern_string
                    .parse()
                    .map_err(|_| E::custom("Failed to parse pattern"))?;
                Ok(GenericPattern::from(&pattern))
            }
        }

        impl<'de: 'data, 'data> Deserialize<'de> for GenericPattern<'data> {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: Deserializer<'de>,
            {
                if deserializer.is_human_readable() {
                    deserializer.deserialize_str(DeserializeGenericPatternUTS35String)
                } else {
                    let items = ZeroVec::deserialize(deserializer)?;
                    Ok(Self { items })
                }
            }
        }

        #[cfg(feature = "datagen")]
        impl Serialize for GenericPattern<'_> {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: ser::Serializer,
            {
                if serializer.is_human_readable() {
                    // Serialize into the UTS 35 string representation.
                    let string = self.to_string();
                    serializer.serialize_str(&string)
                } else {
                    self.items.serialize(serializer)
                }
            }
        }

        #[cfg(all(test, feature = "datagen"))]
        mod test {
            use super::*;

            #[test]
            fn runtime_generic_pattern_serde_human_readable_test() {
                let pattern: GenericPattern =
                    "{0} 'and' {1}".parse().expect("Failed to parse pattern");
                let json = serde_json::to_string(&pattern).expect("Failed to serialize pattern");
                let result: GenericPattern =
                    serde_json::from_str(&json).expect("Failed to deserialize pattern");
                assert_eq!(pattern, result);
            }

            #[test]
            fn runtime_generic_pattern_serde_bincode_test() {
                let pattern: GenericPattern =
                    "{0} 'and' {1}".parse().expect("Failed to parse pattern");
                let bytes = bincode::serialize(&pattern).expect("Failed to serialize pattern");
                let result: GenericPattern =
                    bincode::deserialize(&bytes).expect("Failed to deserialize pattern");
                assert_eq!(pattern, result);
            }
        }
    }
}
