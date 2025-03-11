// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for the lstm

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use zerovec::{ZeroMap, ZeroVec};

// We do this instead of const generics because ZeroFrom and Yokeable derives, as well as serde
// don't support them
macro_rules! lstm_matrix {
    ($name:ident, $generic:literal) => {
        /// The struct that stores a LSTM's matrix.
        ///
        /// <div class="stab unstable">
        /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
        /// including in SemVer minor releases. While the serde representation of data structs is guaranteed
        /// to be stable, their Rust representation might not be. Use with caution.
        /// </div>
        #[derive(PartialEq, Debug, Clone, zerofrom::ZeroFrom, yoke::Yokeable)]
        #[cfg_attr(feature = "datagen", derive(serde::Serialize))]
        pub struct $name<'data> {
            // Invariant: dims.product() == data.len()
            #[allow(missing_docs)]
            pub(crate) dims: [u16; $generic],
            #[allow(missing_docs)]
            pub(crate) data: ZeroVec<'data, f32>,
        }

        impl<'data> $name<'data> {
            #[cfg(any(feature = "serde", feature = "datagen"))]
            /// Creates a LstmMatrix with the given dimensions. Fails if the dimensions don't match the data.
            pub fn from_parts(
                dims: [u16; $generic],
                data: ZeroVec<'data, f32>,
            ) -> Result<Self, DataError> {
                if dims.iter().map(|&i| i as usize).product::<usize>() != data.len() {
                    Err(DataError::custom("Dimension mismatch"))
                } else {
                    Ok(Self { dims, data })
                }
            }

            #[doc(hidden)] // databake
            pub const fn from_parts_unchecked(
                dims: [u16; $generic],
                data: ZeroVec<'data, f32>,
            ) -> Self {
                Self { dims, data }
            }
        }

        #[cfg(feature = "serde")]
        impl<'de: 'data, 'data> serde::Deserialize<'de> for $name<'data> {
            fn deserialize<S>(deserializer: S) -> Result<Self, S::Error>
            where
                S: serde::de::Deserializer<'de>,
            {
                #[derive(serde::Deserialize)]
                struct Raw<'data> {
                    dims: [u16; $generic],
                    #[serde(borrow)]
                    data: ZeroVec<'data, f32>,
                }

                let raw = Raw::deserialize(deserializer)?;

                use serde::de::Error;
                Self::from_parts(raw.dims, raw.data)
                    .map_err(|_| S::Error::custom("Dimension mismatch"))
            }
        }

        #[cfg(feature = "datagen")]
        impl databake::Bake for $name<'_> {
            fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
                let dims = self.dims.bake(env);
                let data = self.data.bake(env);
                databake::quote! {
                    icu_segmenter::provider::$name::from_parts_unchecked(#dims, #data)
                }
            }
        }

        #[cfg(feature = "datagen")]
        impl databake::BakeSize for $name<'_> {
            fn borrows_size(&self) -> usize {
                self.data.borrows_size()
            }
        }
    };
}

lstm_matrix!(LstmMatrix1, 1);
lstm_matrix!(LstmMatrix2, 2);
lstm_matrix!(LstmMatrix3, 3);

#[derive(PartialEq, Debug, Clone, Copy)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_segmenter::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
/// The type of LSTM model
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
pub enum ModelType {
    /// A model working on code points
    Codepoints,
    /// A model working on grapheme clusters
    GraphemeClusters,
}

/// The struct that stores a LSTM model.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(PartialEq, Debug, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[yoke(prove_covariance_manually)]
pub struct LstmDataFloat32<'data> {
    /// Type of the model
    pub(crate) model: ModelType,
    /// The grapheme cluster dictionary used to train the model
    pub(crate) dic: ZeroMap<'data, PotentialUtf8, u16>,
    /// The embedding layer. Shape (dic.len + 1, e)
    pub(crate) embedding: LstmMatrix2<'data>,
    /// The forward layer's first matrix. Shape (h, 4, e)
    pub(crate) fw_w: LstmMatrix3<'data>,
    /// The forward layer's second matrix. Shape (h, 4, h)
    pub(crate) fw_u: LstmMatrix3<'data>,
    /// The forward layer's bias. Shape (h, 4)
    pub(crate) fw_b: LstmMatrix2<'data>,
    /// The backward layer's first matrix. Shape (h, 4, e)
    pub(crate) bw_w: LstmMatrix3<'data>,
    /// The backward layer's second matrix. Shape (h, 4, h)
    pub(crate) bw_u: LstmMatrix3<'data>,
    /// The backward layer's bias. Shape (h, 4)
    pub(crate) bw_b: LstmMatrix2<'data>,
    /// The output layer's weights. Shape (2, 4, h)
    pub(crate) time_w: LstmMatrix3<'data>,
    /// The output layer's bias. Shape (4)
    pub(crate) time_b: LstmMatrix1<'data>,
}

impl<'data> LstmDataFloat32<'data> {
    #[doc(hidden)] // databake
    #[allow(clippy::too_many_arguments)] // constructor
    pub const fn from_parts_unchecked(
        model: ModelType,
        dic: ZeroMap<'data, PotentialUtf8, u16>,
        embedding: LstmMatrix2<'data>,
        fw_w: LstmMatrix3<'data>,
        fw_u: LstmMatrix3<'data>,
        fw_b: LstmMatrix2<'data>,
        bw_w: LstmMatrix3<'data>,
        bw_u: LstmMatrix3<'data>,
        bw_b: LstmMatrix2<'data>,
        time_w: LstmMatrix3<'data>,
        time_b: LstmMatrix1<'data>,
    ) -> Self {
        Self {
            model,
            dic,
            embedding,
            fw_w,
            fw_u,
            fw_b,
            bw_w,
            bw_u,
            bw_b,
            time_w,
            time_b,
        }
    }

    #[cfg(any(feature = "serde", feature = "datagen"))]
    /// Creates a LstmDataFloat32 with the given data. Fails if the matrix dimensions are inconsistent.
    #[allow(clippy::too_many_arguments)] // constructor
    pub fn try_from_parts(
        model: ModelType,
        dic: ZeroMap<'data, PotentialUtf8, u16>,
        embedding: LstmMatrix2<'data>,
        fw_w: LstmMatrix3<'data>,
        fw_u: LstmMatrix3<'data>,
        fw_b: LstmMatrix2<'data>,
        bw_w: LstmMatrix3<'data>,
        bw_u: LstmMatrix3<'data>,
        bw_b: LstmMatrix2<'data>,
        time_w: LstmMatrix3<'data>,
        time_b: LstmMatrix1<'data>,
    ) -> Result<Self, DataError> {
        let dic_len = u16::try_from(dic.len())
            .map_err(|_| DataError::custom("Dictionary does not fit in u16"))?;

        let num_classes = embedding.dims[0];
        let embedd_dim = embedding.dims[1];
        let hunits = fw_u.dims[2];
        if num_classes - 1 != dic_len
            || fw_w.dims != [4, hunits, embedd_dim]
            || fw_u.dims != [4, hunits, hunits]
            || fw_b.dims != [4, hunits]
            || bw_w.dims != [4, hunits, embedd_dim]
            || bw_u.dims != [4, hunits, hunits]
            || bw_b.dims != [4, hunits]
            || time_w.dims != [2, 4, hunits]
            || time_b.dims != [4]
        {
            return Err(DataError::custom("LSTM dimension mismatch"));
        }

        #[cfg(debug_assertions)]
        if !dic.iter_copied_values().all(|(_, g)| g < dic_len) {
            return Err(DataError::custom("Invalid cluster id"));
        }

        Ok(Self {
            model,
            dic,
            embedding,
            fw_w,
            fw_u,
            fw_b,
            bw_w,
            bw_u,
            bw_b,
            time_w,
            time_b,
        })
    }
}

#[cfg(feature = "serde")]
impl<'de: 'data, 'data> serde::Deserialize<'de> for LstmDataFloat32<'data> {
    fn deserialize<S>(deserializer: S) -> Result<Self, S::Error>
    where
        S: serde::de::Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        struct Raw<'data> {
            model: ModelType,
            #[cfg_attr(feature = "serde", serde(borrow))]
            dic: ZeroMap<'data, PotentialUtf8, u16>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            embedding: LstmMatrix2<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            fw_w: LstmMatrix3<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            fw_u: LstmMatrix3<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            fw_b: LstmMatrix2<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            bw_w: LstmMatrix3<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            bw_u: LstmMatrix3<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            bw_b: LstmMatrix2<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            time_w: LstmMatrix3<'data>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            time_b: LstmMatrix1<'data>,
        }

        let raw = Raw::deserialize(deserializer)?;

        use serde::de::Error;
        Self::try_from_parts(
            raw.model,
            raw.dic,
            raw.embedding,
            raw.fw_w,
            raw.fw_u,
            raw.fw_b,
            raw.bw_w,
            raw.bw_u,
            raw.bw_b,
            raw.time_w,
            raw.time_b,
        )
        .map_err(|_| S::Error::custom("Invalid dimensions"))
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for LstmDataFloat32<'_> {
    fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
        let model = self.model.bake(env);
        let dic = self.dic.bake(env);
        let embedding = self.embedding.bake(env);
        let fw_w = self.fw_w.bake(env);
        let fw_u = self.fw_u.bake(env);
        let fw_b = self.fw_b.bake(env);
        let bw_w = self.bw_w.bake(env);
        let bw_u = self.bw_u.bake(env);
        let bw_b = self.bw_b.bake(env);
        let time_w = self.time_w.bake(env);
        let time_b = self.time_b.bake(env);
        databake::quote! {
            icu_segmenter::provider::LstmDataFloat32::from_parts_unchecked(
                #model,
                #dic,
                #embedding,
                #fw_w,
                #fw_u,
                #fw_b,
                #bw_w,
                #bw_u,
                #bw_b,
                #time_w,
                #time_b,
            )
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for LstmDataFloat32<'_> {
    fn borrows_size(&self) -> usize {
        self.model.borrows_size()
            + self.dic.borrows_size()
            + self.embedding.borrows_size()
            + self.fw_w.borrows_size()
            + self.fw_u.borrows_size()
            + self.fw_b.borrows_size()
            + self.bw_w.borrows_size()
            + self.bw_u.borrows_size()
            + self.bw_b.borrows_size()
            + self.time_w.borrows_size()
            + self.time_b.borrows_size()
    }
}

/// The data to power the LSTM segmentation model.
///
/// This data enum is extensible: more backends may be added in the future.
/// Old data can be used with newer code but not vice versa.
///
/// Examples of possible future extensions:
///
/// 1. Variant to store data in 16 instead of 32 bits
/// 2. Minor changes to the LSTM model, such as different forward/backward matrix sizes
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_segmenter::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
#[non_exhaustive]
pub enum LstmData<'data> {
    /// The data as matrices of zerovec f32 values.
    Float32(#[cfg_attr(feature = "serde", serde(borrow))] LstmDataFloat32<'data>),
    // new variants should go BELOW existing ones
    // Serde serializes based on variant name and index in the enum
    // https://docs.rs/serde/latest/serde/trait.Serializer.html#tymethod.serialize_unit_variant
}

icu_provider::data_struct!(
    LstmData<'_>,
    #[cfg(feature = "datagen")]
);
