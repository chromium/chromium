//! # Concolic Tracing
#[cfg(feature = "std")]
use alloc::vec::Vec;
use core::{
    fmt::{Debug, Display, Error, Formatter},
    num::NonZeroUsize,
};

#[cfg(feature = "std")]
use bincode::{Decode, Encode};
#[cfg(feature = "std")]
use serde::{Deserialize, Serialize};

/// A `SymExprRef` identifies a [`SymExpr`] in a trace.
///
/// Reading a `SymExpr` from a trace will always also yield its
/// `SymExprRef`, which can be used later in the trace to identify the `SymExpr`.
/// It is also never zero, which allows for efficient use of `Option<SymExprRef>`.
///
/// In a trace, `SymExprRef`s are monotonically increasing and start at 1.
/// `SymExprRef`s are not valid across traces.
pub type SymExprRef = NonZeroUsize;

/// [`Location`]s are code locations encountered during concolic tracing
///
/// [`Location`]s are constructed from pointers, but not always in a meaningful way.
/// Therefore, a location is an opaque value that can only be compared against itself.
///
/// It is possible to get at the underlying value using [`Into::into`], should this restriction be too inflexible for your usecase.
#[cfg_attr(feature = "std", derive(Serialize, Deserialize, Encode, Decode))]
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Location(usize);

impl Debug for Location {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        Debug::fmt(&self.0, f)
    }
}

impl Display for Location {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        Display::fmt(&self.0, f)
    }
}

impl From<Location> for usize {
    fn from(l: Location) -> Self {
        l.0
    }
}

impl From<usize> for Location {
    fn from(v: usize) -> Self {
        Self(v)
    }
}

/// `SymExpr` represents a message in the serialization format.
/// The messages in the format are a perfect mirror of the methods that are called on the runtime during execution.
#[cfg(feature = "std")]
#[allow(missing_docs)]
#[derive(Serialize, Deserialize, Debug, PartialEq, Encode, Decode)]
pub enum SymExpr {
    InputByte {
        offset: usize,
        value: u8,
    },
    Integer {
        value: u64,
        bits: u8,
    },
    Integer128 {
        high: u64,
        low: u64,
    },
    IntegerFromBuffer {},
    Float {
        value: f64,
        is_double: bool,
    },
    NullPointer,
    True,
    False,
    Bool {
        value: bool,
    },

    Neg {
        op: SymExprRef,
    },
    Add {
        a: SymExprRef,
        b: SymExprRef,
    },
    Sub {
        a: SymExprRef,
        b: SymExprRef,
    },
    Mul {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedDiv {
        a: SymExprRef,
        b: SymExprRef,
    },
    SignedDiv {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedRem {
        a: SymExprRef,
        b: SymExprRef,
    },
    SignedRem {
        a: SymExprRef,
        b: SymExprRef,
    },
    ShiftLeft {
        a: SymExprRef,
        b: SymExprRef,
    },
    LogicalShiftRight {
        a: SymExprRef,
        b: SymExprRef,
    },
    ArithmeticShiftRight {
        a: SymExprRef,
        b: SymExprRef,
    },

    SignedLessThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    SignedLessEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    SignedGreaterThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    SignedGreaterEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedLessThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedLessEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedGreaterThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    UnsignedGreaterEqual {
        a: SymExprRef,
        b: SymExprRef,
    },

    Not {
        op: SymExprRef,
    },
    Equal {
        a: SymExprRef,
        b: SymExprRef,
    },
    NotEqual {
        a: SymExprRef,
        b: SymExprRef,
    },

    BoolAnd {
        a: SymExprRef,
        b: SymExprRef,
    },
    BoolOr {
        a: SymExprRef,
        b: SymExprRef,
    },
    BoolXor {
        a: SymExprRef,
        b: SymExprRef,
    },

    And {
        a: SymExprRef,
        b: SymExprRef,
    },
    Or {
        a: SymExprRef,
        b: SymExprRef,
    },
    Xor {
        a: SymExprRef,
        b: SymExprRef,
    },

    FloatOrdered {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedGreaterThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedGreaterEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedLessThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedLessEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatOrderedNotEqual {
        a: SymExprRef,
        b: SymExprRef,
    },

    FloatUnordered {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedGreaterThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedGreaterEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedLessThan {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedLessEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatUnorderedNotEqual {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatNeg {
        op: SymExprRef,
    },
    FloatAbs {
        op: SymExprRef,
    },
    FloatAdd {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatSub {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatMul {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatDiv {
        a: SymExprRef,
        b: SymExprRef,
    },
    FloatRem {
        a: SymExprRef,
        b: SymExprRef,
    },

    Ite {
        cond: SymExprRef,
        a: SymExprRef,
        b: SymExprRef,
    },
    Sext {
        op: SymExprRef,
        bits: u8,
    },
    Zext {
        op: SymExprRef,
        bits: u8,
    },
    Trunc {
        op: SymExprRef,
        bits: u8,
    },
    IntToFloat {
        op: SymExprRef,
        is_double: bool,
        is_signed: bool,
    },
    FloatToFloat {
        op: SymExprRef,
        to_double: bool,
    },
    BitsToFloat {
        op: SymExprRef,
        to_double: bool,
    },
    FloatToBits {
        op: SymExprRef,
    },
    FloatToSignedInteger {
        op: SymExprRef,
        bits: u8,
    },
    FloatToUnsignedInteger {
        op: SymExprRef,
        bits: u8,
    },
    BoolToBit {
        op: SymExprRef,
    },

    Concat {
        a: SymExprRef,
        b: SymExprRef,
    },
    Extract {
        op: SymExprRef,
        first_bit: usize,
        last_bit: usize,
    },
    Insert {
        target: SymExprRef,
        to_insert: SymExprRef,
        offset: u64,
        little_endian: bool,
    },

    PathConstraint {
        constraint: SymExprRef,
        taken: bool,
        location: Location,
    },

    /// These expressions won't be referenced again
    ExpressionsUnreachable {
        exprs: Vec<SymExprRef>,
    },

    /// Location information regarding a call. Tracing this information is optional.
    Call {
        location: Location,
    },
    /// Location information regarding a return. Tracing this information is optional.
    Return {
        location: Location,
    },
    /// Location information regarding a basic block. Tracing this information is optional.
    BasicBlock {
        location: Location,
    },
}

#[cfg(feature = "std")]
pub mod serialization_format;

/// The environment name used to identify the hitmap for the concolic runtime.
pub const HITMAP_ENV_NAME: &str = "LIBAFL_CONCOLIC_HITMAP";

/// The name of the environment variable that contains the byte offsets to be symbolized.
pub const SELECTIVE_SYMBOLICATION_ENV_NAME: &str = "LIBAFL_SELECTIVE_SYMBOLICATION";

/// The name of the environment variable that signals the runtime to concretize floating point operations.
pub const NO_FLOAT_ENV_NAME: &str = "LIBAFL_CONCOLIC_NO_FLOAT";

/// The name of the environment variable that signals the runtime to perform expression pruning.
pub const EXPRESSION_PRUNING: &str = "LIBAFL_CONCOLIC_EXPRESSION_PRUNING";

#[cfg(feature = "std")]
mod metadata;
#[cfg(feature = "std")]
pub use metadata::ConcolicMetadata;

#[cfg(feature = "std")]
mod observer;
#[cfg(feature = "std")]
pub use observer::ConcolicObserver;
