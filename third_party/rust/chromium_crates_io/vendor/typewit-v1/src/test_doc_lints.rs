#![deny(missing_docs)]

crate::simple_type_witness!{
    /// Docs for enum
    derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Equals)
    pub enum AllDerives {
        /// docs for U8 variant
        U8 = u8, 
        /// docs for U16 variant
        U16 = u16,
    }
}




