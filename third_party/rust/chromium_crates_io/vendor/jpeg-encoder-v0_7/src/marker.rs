#![allow(clippy::upper_case_acronyms)]

// Table B.1
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Marker {
    ZERO,
    /// Start Of Frame markers
    SOF(SOFType),
    /// Reserved for JPEG extensions
    JPG,
    /// Define Huffman table(s)
    DHT,
    /// Define arithmetic coding conditioning(s)
    DAC,
    /// Restart with modulo 8 count `m`
    RST(u8),
    /// Start of image
    SOI,
    /// End of image
    EOI,
    /// Start of scan
    SOS,
    /// Define quantization table(s)
    DQT,
    /// Define number of lines
    DNL,
    /// Define restart interval
    DRI,
    /// Define hierarchical progression
    DHP,
    /// Expand reference component(s)
    EXP,
    /// Reserved for application segments
    APP(u8),
    /// Reserved for JPEG extensions
    JPGn(u8),
    /// Comment
    COM,
    /// For temporary private use in arithmetic coding
    TEM,
    /// Reserved
    RES,
    /// Fill byte
    FILL,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SOFType {
    /// SOF(0)
    BaselineDCT,
    /// SOF(1)
    ExtendedSequentialDCT,
    /// SOF(2)
    ProgressiveDCT,
    /// SOF(3)
    Lossless,
    /// SOF(5)
    DifferentialSequentialDCT,
    /// SOF(6)
    DifferentialProgressiveDCT,
    /// SOF(7)
    DifferentialLossless,
    /// SOF(9)
    ExtendedSequentialDCTArithmetic,
    /// SOF(10)
    ProgressiveDCTArithmetic,
    /// SOF(11)
    LosslessArithmeticCoding,
    /// SOF(13)
    DifferentialSequentialDCTArithmetic,
    /// SOF(14)
    DifferentialProgressiveDCTArithmetic,
    /// SOF(15)
    DifferentialLosslessArithmetic,
}

impl From<Marker> for u8 {
    fn from(marker: Marker) -> Self {
        use self::{Marker::*, SOFType::*};

        match marker {
            ZERO => 0x00,
            TEM => 0x01,
            RES => 0x02,
            SOF(BaselineDCT) => 0xC0,
            SOF(ExtendedSequentialDCT) => 0xC1,
            SOF(ProgressiveDCT) => 0xC2,
            SOF(Lossless) => 0xC3,
            DHT => 0xC4,
            SOF(DifferentialSequentialDCT) => 0xC5,
            SOF(DifferentialProgressiveDCT) => 0xC6,
            SOF(DifferentialLossless) => 0xC7,
            JPG => 0xC8,
            SOF(ExtendedSequentialDCTArithmetic) => 0xC9,
            SOF(ProgressiveDCTArithmetic) => 0xCA,
            SOF(LosslessArithmeticCoding) => 0xCB,
            DAC => 0xCC,
            SOF(DifferentialSequentialDCTArithmetic) => 0xCD,
            SOF(DifferentialProgressiveDCTArithmetic) => 0xCE,
            SOF(DifferentialLosslessArithmetic) => 0xCF,
            RST(v) => 0xD0 + v,
            SOI => 0xD8,
            EOI => 0xD9,
            SOS => 0xDA,
            DQT => 0xDB,
            DNL => 0xDC,
            DRI => 0xDD,
            DHP => 0xDE,
            EXP => 0xDF,
            APP(v) => 0xE0 + v,
            JPGn(v) => 0xF0 + v,
            COM => 0xFE,
            FILL => 0xFF,
        }
    }
}
