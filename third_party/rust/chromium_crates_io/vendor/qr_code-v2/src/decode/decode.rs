//! QR decode, taken from https://raw.githubusercontent.com/WanzenBug/rqrr/505c281db1fe4c7d30e3be595ec5a48b2cedebde/src/decode.rs

use std::io::Write;
use std::mem;

use g2p::{g2p, GaloisField};

use crate::decode::version_db::{RSParameters, VERSION_DATA_BASE};
use crate::decode::{BitGrid, DeQRError, DeQRResult};

g2p!(GF16, 4, modulus: 0b1_0011);
g2p!(GF256, 8, modulus: 0b1_0001_1101);

const MAX_PAYLOAD_SIZE: usize = 8896;

/// Version of a QR Code which determines its size
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct Version(pub usize);

impl Version {
    /// Given the grid size, determine the likely grid size
    pub fn from_size(b: usize) -> DeQRResult<Self> {
        let computed_version = b.saturating_sub(17) / 4;

        if computed_version > 0 && computed_version <= 40 {
            Ok(Version(computed_version))
        } else {
            Err(DeQRError::InvalidVersion)
        }
    }

    /// Return the size of a grid of the given version
    pub fn to_size(&self) -> usize {
        self.0 as usize * 4 + 17
    }
}

/// MetaData for a QR grid
///
/// Stores information about the size/version of given grid. Also contains information about the
/// error correction level and bit mask used.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct MetaData {
    /// The version/size of the grid
    pub version: Version,
    /// the error correction leven, between 0 and 3
    pub ecc_level: u16,
    /// The mask that was used, value between 0 and 7
    pub mask: u16,
}

#[derive(Clone)]
pub struct RawData {
    data: [u8; MAX_PAYLOAD_SIZE],
    len: usize,
}

impl RawData {
    pub fn new() -> Self {
        RawData {
            data: [0; MAX_PAYLOAD_SIZE],
            len: 0,
        }
    }

    pub fn push(&mut self, bit: bool) {
        assert!((self.len >> 8) < MAX_PAYLOAD_SIZE);
        let bitpos = (self.len & 7) as u8;
        let bytepos = self.len >> 3;

        if bit {
            self.data[bytepos] |= 0x80_u8 >> bitpos;
        }
        self.len += 1;
    }
}

#[derive(Clone)]
pub struct CorrectedDataStream {
    data: [u8; MAX_PAYLOAD_SIZE],
    ptr: usize,
    bit_len: usize,
}

impl CorrectedDataStream {
    pub fn bits_remaining(&self) -> usize {
        assert!(self.bit_len >= self.ptr);
        self.bit_len - self.ptr
    }

    pub fn take_bits(&mut self, nbits: usize) -> usize {
        let mut ret = 0;
        let max_len = ::std::cmp::min(self.bits_remaining(), nbits);
        assert!(max_len <= mem::size_of::<usize>() * 8);
        for _ in 0..max_len {
            let b = self.data[self.ptr >> 3];
            let bitpos = self.ptr & 7;
            ret <<= 1;
            if 0 != (b << bitpos) & 0x80 {
                ret |= 1
            }
            self.ptr += 1;
        }
        ret
    }
}

/* ***********************************************************************
 * Decoder algorithm
 */
#[derive(Copy, Clone)]
pub struct DataStream {
    pub raw: [u8; MAX_PAYLOAD_SIZE],
    pub data_bits: usize,
    pub ptr: usize,
    pub data: [u8; MAX_PAYLOAD_SIZE],
}

/// Given a grid try to decode and write it to the output writer
///
/// This tries to read the bit patterns from a [Grid](trait.Grid.html), correct errors
/// and/or missing bits and write the result to the output. If successful also returns
/// [MetaData](struct.MetaData.html) of the read grid.
pub fn decode<W>(code: &dyn BitGrid, writer: W) -> DeQRResult<MetaData>
where
    W: Write,
{
    let meta = read_format(code)?;
    let raw = read_data(code, &meta);
    let stream = codestream_ecc(&meta, raw)?;
    decode_payload(&meta, stream, writer)?;

    Ok(meta)
}

pub(crate) fn decode_payload<W>(
    meta: &MetaData,
    mut ds: CorrectedDataStream,
    mut writer: W,
) -> DeQRResult<()>
where
    W: Write,
{
    while ds.bits_remaining() >= 4 {
        let ty = ds.take_bits(4);
        match ty {
            0 => break,
            1 => decode_numeric(meta, &mut ds, &mut writer),
            2 => decode_alpha(meta, &mut ds, &mut writer),
            3 => decode_structured(meta, &mut ds, &mut writer),
            4 => decode_byte(meta, &mut ds, &mut writer),
            8 => decode_kanji(meta, &mut ds, &mut writer),
            7 => decode_eci(meta, &mut ds, &mut writer),
            _ => Err(DeQRError::UnknownDataType)?,
        }?;
    }
    Ok(())
}

fn decode_eci<W>(_meta: &MetaData, ds: &mut CorrectedDataStream, mut _writer: W) -> DeQRResult<()>
where
    W: Write,
{
    if ds.bits_remaining() < 8 {
        Err(DeQRError::DataUnderflow)?
    }

    let mut _eci = ds.take_bits(8) as u32;
    if _eci & 0xc0 == 0x80 {
        if ds.bits_remaining() < 8 {
            Err(DeQRError::DataUnderflow)?
        }
        _eci = (_eci << 8) | (ds.take_bits(8) as u32)
    } else if _eci & 0xe0 == 0xc0 {
        if ds.bits_remaining() < 16 {
            Err(DeQRError::DataUnderflow)?
        }

        _eci = (_eci << 16) | (ds.take_bits(16) as u32)
    }
    Ok(())
}

fn decode_kanji<W>(meta: &MetaData, ds: &mut CorrectedDataStream, mut writer: W) -> DeQRResult<()>
where
    W: Write,
{
    let nbits = match meta.version {
        Version(0..=9) => 8,
        Version(10..=26) => 10,
        _ => 12,
    };

    let count = ds.take_bits(nbits);
    if ds.bits_remaining() < count * 13 {
        Err(DeQRError::DataUnderflow)?
    }

    for _ in 0..count {
        let d = ds.take_bits(13);
        let ms_b = d / 0xc0;
        let ls_b = d % 0xc0;
        let intermediate = ms_b << 8 | ls_b;
        let sjw = if intermediate + 0x8140 <= 0x9ffc {
            /* bytes are in the range 0x8140 to 0x9FFC */
            (intermediate + 0x8140) as u16
        } else {
            (intermediate + 0xc140) as u16
        };
        writer
            .write_all(&[(sjw >> 8) as u8, (sjw & 0xff) as u8])
            .map_err(|_| DeQRError::IoError)?;
    }
    Ok(())
}

fn decode_structured<W>(meta: &MetaData, ds: &mut CorrectedDataStream, writer: W) -> DeQRResult<()>
where
    W: Write,
{
    let _current = ds.take_bits(4);
    let _total = ds.take_bits(4);
    let _parity = ds.take_bits(8);
    let _mode_bits = ds.take_bits(4);
    //println!("decode_structured {}/{} parity:{} mode_bits:{}", current, total, parity,mode_bits);
    decode_byte(meta, ds, writer)?;
    Ok(())
}

fn decode_byte<W>(meta: &MetaData, ds: &mut CorrectedDataStream, mut writer: W) -> DeQRResult<()>
where
    W: Write,
{
    let nbits = match meta.version {
        Version(0..=9) => 8,
        _ => 16,
    };

    let count = ds.take_bits(nbits);
    //println!("decode_byte version:{:?} count:{} bits_remaining:{}", meta.version, count, ds.bits_remaining());
    if ds.bits_remaining() < count * 8 {
        return Err(DeQRError::DataUnderflow)?;
    }

    for _ in 0..count {
        let buf = &[ds.take_bits(8) as u8];
        writer.write_all(buf).map_err(|_| DeQRError::IoError)?;
    }
    Ok(())
}

fn decode_alpha<W>(meta: &MetaData, ds: &mut CorrectedDataStream, mut writer: W) -> DeQRResult<()>
where
    W: Write,
{
    let nbits = match meta.version {
        Version(0..=9) => 9,
        Version(10..=26) => 11,
        _ => 13,
    };
    let mut count = ds.take_bits(nbits);
    let mut buf = [0; 2];

    while count >= 2 {
        alpha_tuple(&mut buf, ds, 11, 2)?;
        writer.write_all(&buf[..]).map_err(|_| DeQRError::IoError)?;
        count -= 2;
    }

    if count == 1 {
        alpha_tuple(&mut buf, ds, 6, 1)?;
        writer
            .write_all(&buf[..1])
            .map_err(|_| DeQRError::IoError)?;
    }

    Ok(())
}

fn alpha_tuple(
    buf: &mut [u8; 2],
    ds: &mut CorrectedDataStream,
    nbits: usize,
    digits: usize,
) -> DeQRResult<()> {
    if ds.bits_remaining() < nbits {
        Err(DeQRError::DataUnderflow)
    } else {
        let mut tuple = ds.take_bits(nbits);
        for i in (0..digits).rev() {
            const ALPHA_MAP: &[u8; 46] = b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:\x00";
            buf[i] = ALPHA_MAP[tuple % 45];
            tuple /= 45;
        }
        Ok(())
    }
}

fn decode_numeric<W>(meta: &MetaData, ds: &mut CorrectedDataStream, mut writer: W) -> DeQRResult<()>
where
    W: Write,
{
    let nbits = match meta.version {
        Version(0..=9) => 10,
        Version(10..=26) => 12,
        _ => 14,
    };

    let mut count = ds.take_bits(nbits);
    let mut buf = [0; 3];
    while count >= 3 {
        numeric_tuple(&mut buf, ds, 10, 3)?;
        writer.write_all(&buf[..]).map_err(|_| DeQRError::IoError)?;
        count -= 3;
    }

    if count == 2 {
        numeric_tuple(&mut buf, ds, 7, 2)?;
        writer
            .write_all(&buf[..2])
            .map_err(|_| DeQRError::IoError)?;
        count -= 2;
    }
    if count == 1 {
        numeric_tuple(&mut buf, ds, 4, 1)?;
        writer
            .write_all(&buf[..1])
            .map_err(|_| DeQRError::IoError)?;
    }

    Ok(())
}

fn numeric_tuple(
    buf: &mut [u8; 3],
    ds: &mut CorrectedDataStream,
    nbits: usize,
    digits: usize,
) -> DeQRResult<()> {
    if ds.bits_remaining() < nbits {
        Err(DeQRError::DataUnderflow)
    } else {
        let mut tuple = ds.take_bits(nbits);
        for i in (0..digits).rev() {
            buf[i] = (tuple % 10) as u8 + b'0';
            tuple /= 10;
        }
        Ok(())
    }
}

pub(crate) fn codestream_ecc(meta: &MetaData, ds: RawData) -> DeQRResult<CorrectedDataStream> {
    let mut out = CorrectedDataStream {
        data: [0; MAX_PAYLOAD_SIZE],
        ptr: 0,
        bit_len: 0,
    };

    let ver = &VERSION_DATA_BASE[meta.version.0 as usize];
    let sb_ecc = &ver.ecc[meta.ecc_level as usize];
    let lb_ecc = RSParameters {
        bs: sb_ecc.bs + 1,
        dw: sb_ecc.dw + 1,
        ns: sb_ecc.ns,
    };

    let lb_count = (ver.data_bytes - sb_ecc.bs * sb_ecc.ns) / (sb_ecc.bs + 1);
    let bc = lb_count + sb_ecc.ns;
    let ecc_offset = sb_ecc.dw * bc + lb_count;

    let mut dst_offset = 0;
    for i in 0..bc {
        let ecc = if i < sb_ecc.ns { sb_ecc } else { &lb_ecc };
        let dst = &mut out.data[dst_offset..(dst_offset + ecc.bs)];
        let num_ec = ecc.bs - ecc.dw;
        for j in 0..ecc.dw {
            dst[j] = ds.data[j * bc + i];
        }
        for j in 0..num_ec {
            dst[ecc.dw + j] = ds.data[ecc_offset + j * bc + i];
        }
        correct_block(dst, ecc)?;

        dst_offset += ecc.dw;
    }

    out.bit_len = dst_offset * 8;
    Ok(out)
}

fn correct_block(block: &mut [u8], ecc: &RSParameters) -> DeQRResult<()> {
    assert!(ecc.bs > ecc.dw);

    let npar = ecc.bs - ecc.dw;
    let mut sigma_deriv = [GF256::ZERO; 64];

    // Calculate syndromes. If all 0 there is nothing to do.
    let s = match block_syndromes(&block[..ecc.bs], npar) {
        Ok(_) => return Ok(()),
        Err(s) => s,
    };

    let sigma = berlekamp_massey(&s, npar);
    /* Compute derivative of sigma */
    for i in (1..64).step_by(2) {
        sigma_deriv[i - 1] = sigma[i];
    }

    /* Compute error evaluator polynomial */
    let omega = eloc_poly(&s, &sigma, npar - 1);

    /* Find error locations and magnitudes */
    for i in 0..ecc.bs {
        let xinv = GF256::GENERATOR.pow(255 - i);
        if poly_eval(&sigma, xinv) == GF256::ZERO {
            let sd_x = poly_eval(&sigma_deriv, xinv);
            let omega_x = poly_eval(&omega, xinv);
            let error = omega_x / sd_x;
            block[ecc.bs - i - 1] = (GF256(block[ecc.bs - i - 1]) + error).0;
        }
    }

    match block_syndromes(&block[..ecc.bs], npar) {
        Ok(_) => Ok(()),
        Err(_) => Err(DeQRError::DataEcc),
    }
}
/* ***********************************************************************
 * Code stream error correction
 *
 * Generator polynomial for GF(2^8) is x^8 + x^4 + x^3 + x^2 + 1
 */
fn block_syndromes(block: &[u8], npar: usize) -> Result<[GF256; 64], [GF256; 64]> {
    let mut nonzero: bool = false;
    let mut s = [GF256::ZERO; 64];

    for i in 0..npar {
        for j in 0..block.len() {
            let c = GF256(block[block.len() - 1 - j]);
            s[i] += c * GF256::GENERATOR.pow(i * j);
        }
        if s[i] != GF256::ZERO {
            nonzero = true;
        }
    }
    if nonzero {
        Err(s)
    } else {
        Ok(s)
    }
}

fn poly_eval<G>(s: &[G; 64], x: G) -> G
where
    G: GaloisField,
{
    let mut sum = G::ZERO;
    let mut x_pow = G::ONE;

    for i in 0..64 {
        sum += s[i] * x_pow;
        x_pow *= x;
    }
    sum
}

fn eloc_poly(s: &[GF256; 64], sigma: &[GF256; 64], npar: usize) -> [GF256; 64] {
    let mut omega = [GF256::ZERO; 64];
    for i in 0..npar {
        let a = sigma[i];
        for j in 0..(npar - i) {
            let b = s[j + 1];
            omega[i + j] += a * b;
        }
    }
    omega
}
/* ***********************************************************************
 * Berlekamp-Massey algorithm for finding error locator polynomials.
 */
fn berlekamp_massey<G>(s: &[G; 64], n: usize) -> [G; 64]
where
    G: GaloisField,
{
    let mut ts: [G; 64] = [G::ZERO; 64];
    let mut cs: [G; 64] = [G::ZERO; 64];
    let mut bs: [G; 64] = [G::ZERO; 64];
    let mut l: usize = 0;
    let mut m: usize = 1;
    let mut b = G::ONE;
    bs[0] = G::ONE;
    cs[0] = G::ONE;

    for n in 0..n {
        let mut d = s[n];

        // Calculate in GF(p):
        // d = s[n] + \Sum_{i=1}^{l} c[i] * s[n - i]
        for i in 1..=l {
            d += cs[i] * s[n - i];
        }
        // Pre-calculate d * b^-1 in GF(p)
        let mult = d / b;

        if d == G::ZERO {
            m += 1
        } else if l * 2 <= n {
            ts.copy_from_slice(&cs);
            poly_add(&mut cs, &bs, mult, m);
            bs.copy_from_slice(&ts);
            l = n + 1 - l;
            b = d;
            m = 1
        } else {
            poly_add(&mut cs, &bs, mult, m);
            m += 1
        }
    }
    cs
}
/* ***********************************************************************
 * Polynomial operations
 */
fn poly_add<G>(dst: &mut [G; 64], src: &[G; 64], c: G, shift: usize) -> ()
where
    G: GaloisField,
{
    if c == G::ZERO {
        return;
    }

    for i in 0..64 {
        let p = i + shift;
        if p >= 64 {
            break;
        }
        let v = src[i];
        dst[p] += v * c;
    }
}

pub(crate) fn read_data(code: &dyn BitGrid, meta: &MetaData) -> RawData {
    let mut ds = RawData::new();

    let mut y = code.size() - 1;
    let mut x = code.size() - 1;
    let mut neg_dir = true;

    while x > 0 {
        if x == 6 {
            x -= 1;
        }
        if !reserved_cell(meta.version, y, x) {
            ds.push(read_bit(code, meta, y, x));
        }
        if !reserved_cell(meta.version, y, x - 1) {
            ds.push(read_bit(code, meta, y, x - 1));
        }

        let (new_y, new_neg_dir) = match (y, neg_dir) {
            (0, true) => {
                x = x.saturating_sub(2);
                (0, false)
            }
            (y, false) if y == code.size() - 1 => {
                x = x.saturating_sub(2);
                (code.size() - 1, true)
            }
            (y, true) => (y - 1, true),
            (y, false) => (y + 1, false),
        };

        y = new_y;
        neg_dir = new_neg_dir;
    }

    ds
}

fn read_bit(code: &dyn BitGrid, meta: &MetaData, y: usize, x: usize) -> bool {
    let mut v = code.bit(y, x) as u8;
    if mask_bit(meta.mask, y, x) {
        v ^= 1
    }
    v != 0
}

fn mask_bit(mask: u16, y: usize, x: usize) -> bool {
    match mask {
        0 => 0 == (y + x) % 2,
        1 => 0 == y % 2,
        2 => 0 == x % 3,
        3 => 0 == (y + x) % 3,
        4 => 0 == ((y / 2) + (x / 3)) % 2,
        5 => 0 == ((y * x) % 2 + (y * x) % 3),
        6 => 0 == ((y * x) % 2 + (y * x) % 3) % 2,
        7 => 0 == ((y * x) % 3 + (y + x) % 2) % 2,
        _ => panic!("Unknown mask value"),
    }
}

fn reserved_cell(version: Version, i: usize, j: usize) -> bool {
    let ver = &VERSION_DATA_BASE[version.0];
    let size = version.0 * 4 + 17;

    /* Finder + format: top left */
    if i < 9 && j < 9 {
        return true;
    }

    /* Finder + format: bottom left */
    if i + 8 >= size && j < 9 {
        return true;
    }

    /* Finder + format: top right */
    if i < 9 && j + 8 >= size {
        return true;
    }

    /* Exclude timing patterns */
    if i == 6 || j == 6 {
        return true;
    }

    /* Exclude version info, if it exists. Version info sits adjacent to
     * the top-right and bottom-left finders in three rows, bounded by
     * the timing pattern.
     */
    if version.0 >= 7 {
        if i < 6 && j + 11 >= size {
            return true;
        } else if i + 11 >= size && j < 6 {
            return true;
        }
    }

    /* Exclude alignment patterns */
    let mut ai = None;
    let mut aj = None;

    fn abs_diff(x: usize, y: usize) -> usize {
        if x < y {
            y - x
        } else {
            x - y
        }
    }

    let mut len = 0;
    for (a, &pattern) in ver.apat.iter().take_while(|&&x| x != 0).enumerate() {
        len = a;
        if abs_diff(pattern, i) < 3 {
            ai = Some(a)
        }
        if abs_diff(pattern, j) < 3 {
            aj = Some(a)
        }
    }

    match (ai, aj) {
        (Some(x), Some(y)) if x == len && y == len => true,
        (Some(x), Some(_)) if 0 < x && x < len => true,
        (Some(_), Some(x)) if 0 < x && x < len => true,
        _ => false,
    }
}

fn correct_format(mut word: u16) -> DeQRResult<u16> {
    /* Evaluate U (received codeword) at each of alpha_1 .. alpha_6
     * to get S_1 .. S_6 (but we index them from 0).
     */
    if let Err(mut s) = format_syndromes(word) {
        let sigma = berlekamp_massey(&mut s, 6);

        /* Now, find the roots of the polynomial */
        for i in 0..15 {
            if poly_eval(&sigma, GF16::GENERATOR.pow(15 - i)) == GF16::ZERO {
                word ^= 1 << i;
            }
        }

        // Double CHECK syndromes
        format_syndromes(word).map_err(|_| DeQRError::FormatEcc)?;
    }
    Ok(word)
}

pub(crate) fn read_format(code: &dyn BitGrid) -> DeQRResult<MetaData> {
    let mut format = 0;

    // Try first location
    const XS: [usize; 15] = [8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0];
    const YS: [usize; 15] = [0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8];
    for i in (0..15).rev() {
        format = (format << 1) | code.bit(YS[i], XS[i]) as u16;
    }
    format ^= 0x5412;

    // Check format, try other location if needed
    let verified_format = correct_format(format).or_else(|_| {
        let mut format = 0;
        for i in 0..7 {
            format = (format << 1) | code.bit(code.size() - 1 - i, 8) as u16;
        }
        for i in 0..8 {
            format = (format << 1) | code.bit(8, code.size() - 8 + i) as u16;
        }
        format ^= 0x5412;
        correct_format(format)
    })?;

    let fdata = verified_format >> 10;
    let ecc_level = fdata >> 3;
    let mask = fdata & 7;
    let version = Version::from_size(code.size())?;

    Ok(MetaData {
        version,
        ecc_level,
        mask,
    })
}
/* ***********************************************************************
 * Format value error correction
 *
 * Generator polynomial for GF(2^4) is x^4 + x + 1
 */
fn format_syndromes(u: u16) -> Result<[GF16; 64], [GF16; 64]> {
    let mut result = [GF16(0); 64];
    let mut nonzero = false;

    for i in 0..6 {
        for j in 0..15 {
            if u & (1 << j) != 0 {
                result[i] += GF16::GENERATOR.pow((i + 1) * j);
            }
        }
        if result[i].0 != 0 {
            nonzero = true;
        }
    }

    if nonzero {
        Err(result)
    } else {
        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mask_0() {
        let test = [
            [1, 0, 1, 0, 1, 0, 1],
            [0, 1, 0, 1, 0, 1, 0],
            [1, 0, 1, 0, 1, 0, 1],
            [0, 1, 0, 1, 0, 1, 0],
            [1, 0, 1, 0, 1, 0, 1],
            [0, 1, 0, 1, 0, 1, 0],
            [1, 0, 1, 0, 1, 0, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(0, y, x));
            }
        }
    }

    #[test]
    fn test_mask_1() {
        let test = [
            [1, 1, 1, 1, 1, 1, 1],
            [0, 0, 0, 0, 0, 0, 0],
            [1, 1, 1, 1, 1, 1, 1],
            [0, 0, 0, 0, 0, 0, 0],
            [1, 1, 1, 1, 1, 1, 1],
            [0, 0, 0, 0, 0, 0, 0],
            [1, 1, 1, 1, 1, 1, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(1, y, x));
            }
        }
    }

    #[test]
    fn test_mask_2() {
        let test = [
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(2, y, x));
            }
        }
    }

    #[test]
    fn test_mask_3() {
        let test = [
            [1, 0, 0, 1, 0, 0, 1],
            [0, 0, 1, 0, 0, 1, 0],
            [0, 1, 0, 0, 1, 0, 0],
            [1, 0, 0, 1, 0, 0, 1],
            [0, 0, 1, 0, 0, 1, 0],
            [0, 1, 0, 0, 1, 0, 0],
            [1, 0, 0, 1, 0, 0, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(3, y, x));
            }
        }
    }

    #[test]
    fn test_mask_4() {
        let test = [
            [1, 1, 1, 0, 0, 0, 1],
            [1, 1, 1, 0, 0, 0, 1],
            [0, 0, 0, 1, 1, 1, 0],
            [0, 0, 0, 1, 1, 1, 0],
            [1, 1, 1, 0, 0, 0, 1],
            [1, 1, 1, 0, 0, 0, 1],
            [0, 0, 0, 1, 1, 1, 0],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(4, y, x));
            }
        }
    }

    #[test]
    fn test_mask_5() {
        let test = [
            [1, 1, 1, 1, 1, 1, 1],
            [1, 0, 0, 0, 0, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 1, 0, 1, 0, 1],
            [1, 0, 0, 1, 0, 0, 1],
            [1, 0, 0, 0, 0, 0, 1],
            [1, 1, 1, 1, 1, 1, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(5, y, x));
            }
        }
    }

    #[test]
    fn test_mask_6() {
        let test = [
            [1, 1, 1, 1, 1, 1, 1],
            [1, 1, 1, 0, 0, 0, 1],
            [1, 1, 0, 1, 1, 0, 1],
            [1, 0, 1, 0, 1, 0, 1],
            [1, 0, 1, 1, 0, 1, 1],
            [1, 0, 0, 0, 1, 1, 1],
            [1, 1, 1, 1, 1, 1, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(6, y, x));
            }
        }
    }

    #[test]
    fn test_mask_7() {
        let test = [
            [1, 0, 1, 0, 1, 0, 1],
            [0, 0, 0, 1, 1, 1, 0],
            [1, 0, 0, 0, 1, 1, 1],
            [0, 1, 0, 1, 0, 1, 0],
            [1, 1, 1, 0, 0, 0, 1],
            [0, 1, 1, 1, 0, 0, 0],
            [1, 0, 1, 0, 1, 0, 1],
        ];

        for x in 0..7 {
            for y in 0..7 {
                assert_eq!(test[y][x] != 0, mask_bit(7, y, x));
            }
        }
    }
}
