use alloc::boxed::Box;
use core::num::NonZeroU16;

/// # Quantization table used for encoding
///
/// Tables are based on tables from mozjpeg
#[derive(Debug, Clone)]
pub enum QuantizationTableType {
    /// Sample quantization tables given in Annex K (Clause K.1) of Recommendation ITU-T T.81 (1992) | ISO/IEC 10918-1:1994.
    Default,

    /// Flat
    Flat,

    /// Custom, tuned for MS-SSIM
    CustomMsSsim,

    /// Custom, tuned for PSNR-HVS
    CustomPsnrHvs,

    /// ImageMagick table by N. Robidoux
    ///
    /// From <http://www.imagemagick.org/discourse-server/viewtopic.php?f=22&t=20333&p=98008#p98008>
    ImageMagick,

    /// Relevance of human vision to JPEG-DCT compression (1992) Klein, Silverstein and Carney.
    KleinSilversteinCarney,

    /// DCTune perceptual optimization of compressed dental X-Rays (1997) Watson, Taylor, Borthwick
    DentalXRays,

    /// A visual detection model for DCT coefficient quantization (12/9/93) Ahumada, Watson, Peterson
    VisualDetectionModel,

    /// An improved detection model for DCT coefficient quantization (1993) Peterson, Ahumada and Watson
    ImprovedDetectionModel,

    /// A user supplied quantization table
    Custom(Box<[u16; 64]>),
}

impl QuantizationTableType {
    fn index(&self) -> usize {
        use QuantizationTableType::*;

        match self {
            Default => 0,
            Flat => 1,
            CustomMsSsim => 2,
            CustomPsnrHvs => 3,
            ImageMagick => 4,
            KleinSilversteinCarney => 5,
            DentalXRays => 6,
            VisualDetectionModel => 7,
            ImprovedDetectionModel => 8,
            Custom(_) => panic!("Custom types not supported"),
        }
    }
}

// Tables are based on mozjpeg jcparam.c
static DEFAULT_LUMA_TABLES: [[u16; 64]; 9] = [
    [
        // Annex K
        16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69,
        56, 14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81,
        104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99,
    ],
    [
        // Flat
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    ],
    [
        // Custom, tuned for MS-SSIM
        12, 17, 20, 21, 30, 34, 56, 63, 18, 20, 20, 26, 28, 51, 61, 55, 19, 20, 21, 26, 33, 58, 69,
        55, 26, 26, 26, 30, 46, 87, 86, 66, 31, 33, 36, 40, 46, 96, 100, 73, 40, 35, 46, 62, 81,
        100, 111, 91, 46, 66, 76, 86, 102, 121, 120, 101, 68, 90, 90, 96, 113, 102, 105, 103,
    ],
    [
        // Custom, tuned for PSNR-HVS
        9, 10, 12, 14, 27, 32, 51, 62, 11, 12, 14, 19, 27, 44, 59, 73, 12, 14, 18, 25, 42, 59, 79,
        78, 17, 18, 25, 42, 61, 92, 87, 92, 23, 28, 42, 75, 79, 112, 112, 99, 40, 42, 59, 84, 88,
        124, 132, 111, 42, 64, 78, 95, 105, 126, 125, 99, 70, 75, 100, 102, 116, 100, 107, 98,
    ],
    [
        // ImageMagick table by N. Robidoux
        // From http://www.imagemagick.org/discourse-server/viewtopic.php?f=22&t=20333&p=98008#p98008
        16, 16, 16, 18, 25, 37, 56, 85, 16, 17, 20, 27, 34, 40, 53, 75, 16, 20, 24, 31, 43, 62, 91,
        135, 18, 27, 31, 40, 53, 74, 106, 156, 25, 34, 43, 53, 69, 94, 131, 189, 37, 40, 62, 74,
        94, 124, 169, 238, 56, 53, 91, 106, 131, 169, 226, 311, 85, 75, 135, 156, 189, 238, 311,
        418,
    ],
    [
        // Relevance of human vision to JPEG-DCT compression (1992) Klein, Silverstein and Carney.
        10, 12, 14, 19, 26, 38, 57, 86, 12, 18, 21, 28, 35, 41, 54, 76, 14, 21, 25, 32, 44, 63, 92,
        136, 19, 28, 32, 41, 54, 75, 107, 157, 26, 35, 44, 54, 70, 95, 132, 190, 38, 41, 63, 75,
        95, 125, 170, 239, 57, 54, 92, 107, 132, 170, 227, 312, 86, 76, 136, 157, 190, 239, 312,
        419,
    ],
    [
        // DCTune perceptual optimization of compressed dental X-Rays (1997) Watson, Taylor, Borthwick
        7, 8, 10, 14, 23, 44, 95, 241, 8, 8, 11, 15, 25, 47, 102, 255, 10, 11, 13, 19, 31, 58, 127,
        255, 14, 15, 19, 27, 44, 83, 181, 255, 23, 25, 31, 44, 72, 136, 255, 255, 44, 47, 58, 83,
        136, 255, 255, 255, 95, 102, 127, 181, 255, 255, 255, 255, 241, 255, 255, 255, 255, 255,
        255, 255,
    ],
    [
        // A visual detection model for DCT coefficient quantization (12/9/93) Ahumada, Watson, Peterson
        15, 11, 11, 12, 15, 19, 25, 32, 11, 13, 10, 10, 12, 15, 19, 24, 11, 10, 14, 14, 16, 18, 22,
        27, 12, 10, 14, 18, 21, 24, 28, 33, 15, 12, 16, 21, 26, 31, 36, 42, 19, 15, 18, 24, 31, 38,
        45, 53, 25, 19, 22, 28, 36, 45, 55, 65, 32, 24, 27, 33, 42, 53, 65, 77,
    ],
    [
        // An improved detection model for DCT coefficient quantization (1993) Peterson, Ahumada and Watson
        14, 10, 11, 14, 19, 25, 34, 45, 10, 11, 11, 12, 15, 20, 26, 33, 11, 11, 15, 18, 21, 25, 31,
        38, 14, 12, 18, 24, 28, 33, 39, 47, 19, 15, 21, 28, 36, 43, 51, 59, 25, 20, 25, 33, 43, 54,
        64, 74, 34, 26, 31, 39, 51, 64, 77, 91, 45, 33, 38, 47, 59, 74, 91, 108,
    ],
];

// Tables are based on mozjpeg jcparam.c
static DEFAULT_CHROMA_TABLES: [[u16; 64]; 9] = [
    [
        // Annex K
        17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99,
        99, 47, 66, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    ],
    [
        // Flat
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    ],
    [
        // Custom, tuned for MS-SSIM
        8, 12, 15, 15, 86, 96, 96, 98, 13, 13, 15, 26, 90, 96, 99, 98, 12, 15, 18, 96, 99, 99, 99,
        99, 17, 16, 90, 96, 99, 99, 99, 99, 96, 96, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    ],
    [
        //Custom, tuned for PSNR-HVS
        9, 10, 17, 19, 62, 89, 91, 97, 12, 13, 18, 29, 84, 91, 88, 98, 14, 19, 29, 93, 95, 95, 98,
        97, 20, 26, 84, 88, 95, 95, 98, 94, 26, 86, 91, 93, 97, 99, 98, 99, 99, 100, 98, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 97, 97, 99, 99, 99, 99, 97, 99,
    ],
    [
        // ImageMagick table by N. Robidoux
        // From http://www.imagemagick.org/discourse-server/viewtopic.php?f=22&t=20333&p=98008#p98008
        16, 16, 16, 18, 25, 37, 56, 85, 16, 17, 20, 27, 34, 40, 53, 75, 16, 20, 24, 31, 43, 62, 91,
        135, 18, 27, 31, 40, 53, 74, 106, 156, 25, 34, 43, 53, 69, 94, 131, 189, 37, 40, 62, 74,
        94, 124, 169, 238, 56, 53, 91, 106, 131, 169, 226, 311, 85, 75, 135, 156, 189, 238, 311,
        418,
    ],
    [
        // Relevance of human vision to JPEG-DCT compression (1992) Klein, Silverstein and Carney.
        10, 12, 14, 19, 26, 38, 57, 86, 12, 18, 21, 28, 35, 41, 54, 76, 14, 21, 25, 32, 44, 63, 92,
        136, 19, 28, 32, 41, 54, 75, 107, 157, 26, 35, 44, 54, 70, 95, 132, 190, 38, 41, 63, 75,
        95, 125, 170, 239, 57, 54, 92, 107, 132, 170, 227, 312, 86, 76, 136, 157, 190, 239, 312,
        419,
    ],
    [
        // DCTune perceptual optimization of compressed dental X-Rays (1997) Watson, Taylor, Borthwick
        7, 8, 10, 14, 23, 44, 95, 241, 8, 8, 11, 15, 25, 47, 102, 255, 10, 11, 13, 19, 31, 58, 127,
        255, 14, 15, 19, 27, 44, 83, 181, 255, 23, 25, 31, 44, 72, 136, 255, 255, 44, 47, 58, 83,
        136, 255, 255, 255, 95, 102, 127, 181, 255, 255, 255, 255, 241, 255, 255, 255, 255, 255,
        255, 255,
    ],
    [
        // A visual detection model for DCT coefficient quantization (12/9/93) Ahumada, Watson, Peterson
        15, 11, 11, 12, 15, 19, 25, 32, 11, 13, 10, 10, 12, 15, 19, 24, 11, 10, 14, 14, 16, 18, 22,
        27, 12, 10, 14, 18, 21, 24, 28, 33, 15, 12, 16, 21, 26, 31, 36, 42, 19, 15, 18, 24, 31, 38,
        45, 53, 25, 19, 22, 28, 36, 45, 55, 65, 32, 24, 27, 33, 42, 53, 65, 77,
    ],
    [
        // An improved detection model for DCT coefficient quantization (1993) Peterson, Ahumada and Watson
        14, 10, 11, 14, 19, 25, 34, 45, 10, 11, 11, 12, 15, 20, 26, 33, 11, 11, 15, 18, 21, 25, 31,
        38, 14, 12, 18, 24, 28, 33, 39, 47, 19, 15, 21, 28, 36, 43, 51, 59, 25, 20, 25, 33, 43, 54,
        64, 74, 34, 26, 31, 39, 51, 64, 77, 91, 45, 33, 38, 47, 59, 74, 91, 108,
    ],
];

const SHIFT: u32 = 2 * 8 - 1;

fn compute_reciprocal(divisor: u32) -> (i32, i32) {
    if divisor <= 1 {
        return (1, 0);
    }

    let mut reciprocals = (1 << SHIFT) / divisor;
    let fractional = (1 << SHIFT) % divisor;

    // Correction for rounding errors in division
    let mut correction = divisor / 2;

    if fractional != 0 {
        if fractional <= correction {
            correction += 1;
        } else {
            reciprocals += 1;
        }
    }

    (reciprocals as i32, correction as i32)
}

pub struct QuantizationTable {
    table: [NonZeroU16; 64],
    reciprocals: [i32; 64],
    corrections: [i32; 64],
}

impl QuantizationTable {
    pub fn new_with_quality(
        table: &QuantizationTableType,
        quality: u8,
        luma: bool,
    ) -> QuantizationTable {
        let table = match table {
            QuantizationTableType::Custom(table) => Self::get_user_table(table),
            table => {
                let table = if luma {
                    &DEFAULT_LUMA_TABLES[table.index()]
                } else {
                    &DEFAULT_CHROMA_TABLES[table.index()]
                };
                Self::get_with_quality(table, quality)
            }
        };

        let mut reciprocals = [0i32; 64];
        let mut corrections = [0i32; 64];

        for i in 0..64 {
            let (reciprocal, correction) = compute_reciprocal(table[i].get() as u32);

            reciprocals[i] = reciprocal;
            corrections[i] = correction;
        }

        QuantizationTable {
            table,
            reciprocals,
            corrections,
        }
    }

    fn get_user_table(table: &[u16; 64]) -> [NonZeroU16; 64] {
        let mut q_table = [NonZeroU16::new(1).unwrap(); 64];
        for (i, &v) in table.iter().enumerate() {
            q_table[i] = match NonZeroU16::new(v.clamp(1, 2 << 10) << 3) {
                Some(v) => v,
                None => panic!("Invalid quantization table value: {}", v),
            };
        }
        q_table
    }

    fn get_with_quality(table: &[u16; 64], quality: u8) -> [NonZeroU16; 64] {
        let quality = quality.clamp(1, 100) as u32;

        let scale = if quality < 50 {
            5000 / quality
        } else {
            200 - quality * 2
        };

        let mut q_table = [NonZeroU16::new(1).unwrap(); 64];

        for (i, &v) in table.iter().enumerate() {
            let v = v as u32;

            let v = (v * scale + 50) / 100;

            let v = v.clamp(1, 255) as u16;

            // Table values are premultiplied with 8 because dct is scaled by 8
            q_table[i] = NonZeroU16::new(v << 3).unwrap();
        }
        q_table
    }

    #[inline]
    pub fn get(&self, index: usize) -> u8 {
        (self.table[index].get() >> 3) as u8
    }

    #[inline]
    pub fn quantize(&self, in_value: i16, index: usize) -> i16 {
        let value = in_value as i32;

        let reciprocal = self.reciprocals[index];
        let corrections = self.corrections[index];

        let abs_value = value.abs();

        let mut product = (abs_value + corrections) * reciprocal;
        product >>= SHIFT;

        if value != abs_value {
            product *= -1;
        }

        product as i16
    }
}

#[cfg(test)]
mod tests {
    use crate::quantization::{QuantizationTable, QuantizationTableType};

    #[test]
    fn test_new_100() {
        let q = QuantizationTable::new_with_quality(&QuantizationTableType::Default, 100, true);

        for &v in &q.table {
            let v = v.get();
            assert_eq!(v, 1 << 3);
        }

        let q = QuantizationTable::new_with_quality(&QuantizationTableType::Default, 100, false);

        for &v in &q.table {
            let v = v.get();
            assert_eq!(v, 1 << 3);
        }
    }

    #[test]
    fn test_new_100_quantize() {
        let q = QuantizationTable::new_with_quality(&QuantizationTableType::Default, 100, true);

        for i in -255..255 {
            assert_eq!(i, q.quantize(i << 3, 0));
        }
    }
}
