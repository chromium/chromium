// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[derive(Debug)]
pub enum VarZeroVecFormatError {
    /// The byte buffer was not in the appropriate format for VarZeroVec.
    Metadata,
    #[allow(dead_code)]
    Values(crate::ule::UleError),
}
