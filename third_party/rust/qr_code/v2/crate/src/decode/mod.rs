//! Decode QR-Codes

pub use self::decode::{decode, MetaData, Version};
pub use crate::decode::bmp_grid::BmpDecode;
use std::error::Error;
use std::io::Write;

mod bmp_grid;
mod decode;
mod version_db;

/// A simple point in (some) space
#[derive(Debug, Clone, Copy, Eq, PartialEq, Default)]
pub struct Point {
    /// x
    pub x: i32,
    /// y
    pub y: i32,
}

/// Wrapper around any grid that can be interpreted as a QR code
#[derive(Debug, Clone)]
pub struct Grid<G> {
    /// The backing binary square
    pub grid: G,
    /// The bounds of the square, in underlying coordinates.
    ///
    /// If this grid references for example an underlying image, these values will be set to
    /// coordinates in that image.
    pub bounds: [Point; 4],
}

impl<G> Grid<G>
where
    G: BitGrid,
{
    /// Create a new grid from a BitGrid.
    ///
    /// This just initialises the bounds to 0.
    pub fn new(grid: G) -> Self {
        Grid {
            grid,
            bounds: [
                Point { x: 0, y: 0 },
                Point { x: 0, y: 0 },
                Point { x: 0, y: 0 },
                Point { x: 0, y: 0 },
            ],
        }
    }

    /// Try to decode the grid.
    ///
    /// If successful returns the decoded string as well as metadata about the code.
    pub fn decode(&self) -> DeQRResult<(MetaData, String)> {
        let mut out = Vec::new();
        let meta = self.decode_to(&mut out)?;
        let out = String::from_utf8(out)?;
        Ok((meta, out))
    }

    /// Try to decode the grid.
    ///
    /// Instead of returning a String, this methode writes the decoded result to the given writer
    ///
    /// **Warning**: This may lead to half decoded content to be written to the writer.
    pub fn decode_to<W>(&self, writer: W) -> DeQRResult<MetaData>
    where
        W: Write,
    {
        crate::decode::decode(&self.grid, writer)
    }
}

/// A grid that contains exactly one QR code square.
///
/// The common trait for everything that can be decoded as a QR code. Given a normal image, we first
/// need to find the QR grids in it.
///
/// This trait can be implemented when some object is known to be exactly the bit-pattern
/// of a QR code.
pub trait BitGrid {
    /// Return the size of the grid.
    ///
    /// Since QR codes are always squares, the grid is assumed to be size * size.
    fn size(&self) -> usize;

    /// Return the value of the bit at the given location.
    ///
    /// `true` means 'black', `false` means 'white'
    fn bit(&self, y: usize, x: usize) -> bool;

    #[cfg(feature = "img")]
    fn write_grid_to(&self, p: &str) {
        let mut dyn_img = image::GrayImage::new(self.size() as u32, self.size() as u32);
        for y in 0..self.size() {
            for x in 0..self.size() {
                let color = match self.bit(x, y) {
                    true => 0,
                    false => 255,
                };
                dyn_img.get_pixel_mut(x as u32, y as u32).data[0] = color;
            }
        }
        dyn_img.save(p).unwrap();
    }
}

/// A basic GridImage that can be generated from a given function.
///
/// # Example
///
/// ```rust
/// # fn main() -> Result<(), qr_code::decode::DeQRError> {
/// let grid = [
///     [1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, ],
///     [1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, ],
///     [1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, ],
///     [1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, ],
///     [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ],
///     [1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, ],
///     [1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, ],
///     [0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, ],
///     [1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, ],
///     [0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, ],
///     [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, ],
///     [1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, ],
///     [1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, ],
///     [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, ],
///     [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, ],
///     [1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, ],
/// ];
///
/// let simple = qr_code::decode::SimpleGrid::from_func(21, |x, y| {
///     grid[y][x] == 1
/// });
/// let grid = qr_code::decode::Grid::new(simple);
/// let (_meta, content) = grid.decode()?;
/// assert_eq!(content, "rqrr");
/// # Ok(())
/// # }
/// ```
#[derive(Debug, Clone)]
pub struct SimpleGrid {
    cell_bitmap: Vec<u8>,
    size: usize,
}

impl SimpleGrid {
    /// from_func
    pub fn from_func<F>(size: usize, fill_func: F) -> Self
    where
        F: Fn(usize, usize) -> bool,
    {
        let mut cell_bitmap = vec![0; (size * size + 7) / 8];
        let mut c = 0;
        for y in 0..size {
            for x in 0..size {
                if fill_func(x, y) {
                    cell_bitmap[c >> 3] |= 1 << (c & 7) as u8;
                }
                c += 1;
            }
        }

        SimpleGrid { cell_bitmap, size }
    }
}

impl BitGrid for SimpleGrid {
    fn size(&self) -> usize {
        self.size
    }

    fn bit(&self, y: usize, x: usize) -> bool {
        let c = y * self.size + x;
        self.cell_bitmap[c >> 3] & (1 << (c & 7) as u8) != 0
    }
}

/// Possible errors that can happen during decoding
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum DeQRError {
    /// Could not write the output to the output stream/string
    IoError,
    /// Expected more bits to decode
    DataUnderflow,
    /// Expected less bits to decode
    DataOverflow,
    /// Unknown data type in encoding
    UnknownDataType,
    /// Could not correct errors / code corrupt
    DataEcc,
    /// Could not read format information from both locations
    FormatEcc,
    /// Unsupported / non-existent version read
    InvalidVersion,
    /// Unsupported / non-existent grid size read
    InvalidGridSize,
    /// Output was not encoded in expected UTF8
    EncodingError,
}

type DeQRResult<T> = Result<T, DeQRError>;

impl Error for DeQRError {}

impl From<::std::string::FromUtf8Error> for DeQRError {
    fn from(_: ::std::string::FromUtf8Error) -> Self {
        DeQRError::EncodingError
    }
}

impl ::std::fmt::Display for DeQRError {
    fn fmt<'a>(&self, f: &mut std::fmt::Formatter<'a>) -> std::fmt::Result {
        let msg = match self {
            DeQRError::IoError => "IoError(Could not write to output)",
            DeQRError::DataUnderflow => "DataUnderflow(Expected more bits to decode)",
            DeQRError::DataOverflow => "DataOverflow(Expected less bits to decode)",
            DeQRError::UnknownDataType => "UnknownDataType(DataType not known or not implemented)",
            DeQRError::DataEcc => "Ecc(Too many errors to correct)",
            DeQRError::FormatEcc => "Ecc(Version information corrupt)",
            DeQRError::InvalidVersion => "InvalidVersion(Invalid version or corrupt)",
            DeQRError::InvalidGridSize => "InvalidGridSize(Invalid version or corrupt)",
            DeQRError::EncodingError => "Encoding(Not UTF8)",
        };
        write!(f, "{}", msg)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rqrr() {
        let grid = [
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
            [
                1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0,
            ],
            [
                1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1,
            ],
            [
                0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1,
            ],
            [
                1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1,
            ],
            [
                0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1,
            ],
        ];

        let img = crate::decode::SimpleGrid::from_func(21, |x, y| grid[y][x] == 1);

        let mut buf = vec![0; img.size() * img.size() / 8 + 1];
        for y in 0..img.size() {
            for x in 0..img.size() {
                let i = y * img.size() + x;
                if img.bit(y, x) {
                    buf[i >> 3] |= 1 << ((i & 7) as u8);
                }
            }
        }

        let mut vec = Vec::new();
        crate::decode::decode(&img, &mut vec).unwrap();

        assert_eq!(b"rqrr".as_ref(), &vec[..])
    }

    #[test]
    fn test_github() {
        let grid = [
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0,
                1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,
                1,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                0,
            ],
            [
                1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
                0,
            ],
            [
                0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0,
                1,
            ],
            [
                1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1,
                1,
            ],
            [
                1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1,
                0,
            ],
            [
                0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1,
                1,
            ],
            [
                0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0,
                1,
            ],
            [
                1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1,
                1,
            ],
            [
                1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1,
                0,
            ],
            [
                1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1,
                1,
            ],
            [
                0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1,
                1,
            ],
            [
                0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1,
                0,
            ],
            [
                1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0,
                0,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
                1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0,
                0,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1,
                0,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1,
                1,
            ],
        ];

        let img = crate::decode::SimpleGrid::from_func(29, |x, y| grid[y][x] == 1);

        let mut buf = vec![0; img.size() * img.size() / 8 + 1];
        for y in 0..img.size() {
            for x in 0..img.size() {
                let i = y * img.size() + x;
                if img.bit(y, x) {
                    buf[i >> 3] |= 1 << ((i & 7) as u8);
                }
            }
        }

        let mut vec = Vec::new();
        crate::decode::decode(&img, &mut vec).unwrap();

        assert_eq!(b"https://github.com/WanzenBug/rqrr".as_ref(), &vec[..])
    }

    #[test]
    fn test_number() {
        let grid = [
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0,
                1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,
                1,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                0,
            ],
            [
                1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0,
                0,
            ],
            [
                1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0,
                1,
            ],
            [
                0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1,
                0,
            ],
            [
                0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                1,
            ],
            [
                0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1,
                0,
            ],
            [
                0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0,
                1,
            ],
            [
                0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1,
                0,
            ],
            [
                1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
                1,
            ],
            [
                0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1,
                0,
            ],
            [
                1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0,
                1,
            ],
            [
                1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
                0,
            ],
            [
                1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0,
                0,
            ],
            [
                1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                0,
            ],
            [
                0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0,
                1,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1,
                0,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
                1,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0,
                0,
            ],
            [
                1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1,
                0,
            ],
            [
                1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1,
                0,
            ],
            [
                1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0,
                0,
            ],
        ];

        let img = crate::decode::SimpleGrid::from_func(29, |x, y| grid[y][x] == 1);

        let mut buf = vec![0; img.size() * img.size() / 8 + 1];
        for y in 0..img.size() {
            for x in 0..img.size() {
                let i = y * img.size() + x;
                if img.bit(y, x) {
                    buf[i >> 3] |= 1 << ((i & 7) as u8);
                }
            }
        }

        let mut vec = Vec::new();
        crate::decode::decode(&img, &mut vec).unwrap();

        assert_eq!(b"1234567891011121314151617181920".as_ref(), &vec[..])
    }

    /*
    #[test]
    fn test_fuzz() {
        use arbitrary::Arbitrary;
        let data = include_bytes!("../../test_data/crash-0d7a321bf16ab956c7bdbf0167bcc4861a1c61f1");
        let mut unstructured = arbitrary::Unstructured::new(data);
        let qr_code = qr_code::QrCode::arbitrary(&mut unstructured).unwrap();
        dbg!(&qr_code);
        let bmp = qr_code.to_bmp().mul(3).add_white_border(12);
        bmp.write(std::fs::File::create("cc.bmp").unwrap()).unwrap();
        let _ = bmp.decode().unwrap();
    }
     */
}
