//! Checked encoders for the two CFF binary languages and INDEX objects.

use super::{Error, Result};

#[inline]
pub(super) fn number(data: &[u8], pos: &mut usize, dict: bool) -> Result<Option<f64>> {
    let b = *data.get(*pos).ok_or(Error)?;
    let start = *pos;
    if let 32..=246 = b {
        *pos = start + 1;
        return Ok(Some((b as i32 - 139) as f64));
    }
    let value = match b {
        247..=250 => {
            let byte = *data.get(start + 1).ok_or(Error)?;
            *pos = start + 2;
            ((b as i32 - 247) * 256 + byte as i32 + 108) as f64
        }
        251..=254 => {
            let byte = *data.get(start + 1).ok_or(Error)?;
            *pos = start + 2;
            (-(b as i32 - 251) * 256 - byte as i32 - 108) as f64
        }
        28 => {
            let bytes = data.get(start + 1..start + 3).ok_or(Error)?;
            *pos = start + 3;
            i16::from_be_bytes(bytes.try_into().map_err(|_| Error)?) as f64
        }
        29 if dict => {
            let bytes = data.get(start + 1..start + 5).ok_or(Error)?;
            *pos = start + 5;
            i32::from_be_bytes(bytes.try_into().map_err(|_| Error)?) as f64
        }
        255 if !dict => {
            let bytes = data.get(start + 1..start + 5).ok_or(Error)?;
            *pos = start + 5;
            i32::from_be_bytes(bytes.try_into().map_err(|_| Error)?) as f64 / 65536.
        }
        30 if dict => {
            let mut p = start + 1;
            let mut s = String::new();
            'bcd: loop {
                let byte = *data.get(p).ok_or(Error)?;
                p += 1;
                for nibble in [byte >> 4, byte & 15] {
                    match nibble {
                        0..=9 => s.push(char::from(b'0' + nibble)),
                        10 => s.push('.'),
                        11 => s.push('E'),
                        12 => s.push_str("E-"),
                        14 => s.push('-'),
                        15 => break 'bcd,
                        _ => return Err(Error),
                    }
                }
                if s.len() > 128 {
                    return Err(Error);
                }
            }
            *pos = p;
            s.parse().map_err(|_| Error)?
        }
        _ => return Ok(None),
    };
    if !value.is_finite() {
        return Err(Error);
    }
    Ok(Some(value))
}

pub(super) fn op(data: &[u8], pos: &mut usize) -> Result<u16> {
    let b = *data.get(*pos).ok_or(Error)?;
    *pos += 1;
    Ok(if b == 12 {
        let second = *data.get(*pos).ok_or(Error)?;
        *pos += 1;
        0x100 | second as u16
    } else {
        b as u16
    })
}

pub(super) fn emit_op(out: &mut Vec<u8>, op: u16) {
    if op >= 0x100 {
        out.push(12);
    }
    out.push(op as u8);
}

pub(super) fn integer(out: &mut Vec<u8>, v: i32) {
    let (bytes, len) = integer_bytes(v);
    out.extend_from_slice(&bytes[..len]);
}

pub(super) fn integer_bytes(v: i32) -> ([u8; 5], usize) {
    let mut bytes = [0; 5];
    match v {
        -107..=107 => {
            bytes[0] = (v + 139) as u8;
            (bytes, 1)
        }
        108..=1131 => {
            let n = v - 108;
            bytes[..2].copy_from_slice(&[(247 + (n >> 8)) as u8, n as u8]);
            (bytes, 2)
        }
        -1131..=-108 => {
            let n = -v - 108;
            bytes[..2].copy_from_slice(&[(251 + (n >> 8)) as u8, n as u8]);
            (bytes, 2)
        }
        -32768..=32767 => {
            bytes[0] = 28;
            bytes[1..3].copy_from_slice(&(v as i16).to_be_bytes());
            (bytes, 3)
        }
        _ => {
            bytes[0] = 29;
            bytes[1..].copy_from_slice(&v.to_be_bytes());
            (bytes, 5)
        }
    }
}

pub(super) fn long(out: &mut Vec<u8>, v: i32) {
    out.push(29);
    out.extend(v.to_be_bytes());
}

pub(super) fn encode(out: &mut Vec<u8>, value: f64, dict: bool) -> Result<()> {
    if !value.is_finite() {
        return Err(Error);
    }
    if value.fract() == 0. && value >= i32::MIN as f64 && value <= i32::MAX as f64 {
        if !dict && !(-32768. ..=32767.).contains(&value) {
            return Err(Error);
        }
        integer(out, value as i32);
    } else if dict {
        out.push(30);
        let mut nibbles = Vec::new();
        for b in format!("{value:.8e}").bytes() {
            match b {
                b'0'..=b'9' => nibbles.push(b - b'0'),
                b'.' => nibbles.push(10),
                b'e' | b'E' => nibbles.push(11),
                b'-' if nibbles.last() == Some(&11) => *nibbles.last_mut().ok_or(Error)? = 12,
                b'-' => nibbles.push(14),
                b'+' => (),
                _ => return Err(Error),
            }
        }
        nibbles.push(15);
        if nibbles.len() % 2 != 0 {
            nibbles.push(15);
        }
        out.extend(nibbles.chunks_exact(2).map(|n| n[0] << 4 | n[1]));
    } else {
        let fixed = (value * 65536.).round();
        if fixed < i32::MIN as f64 || fixed > i32::MAX as f64 {
            return Err(Error);
        }
        out.push(255);
        out.extend((fixed as i32).to_be_bytes());
    }
    Ok(())
}

// INDEX layout is computed without building an intermediate byte buffer.
pub(super) struct IndexLayout {
    count: u32,
    width: usize,
    pub size: usize,
    cff2: bool,
}
impl IndexLayout {
    pub fn new(items: &[Vec<u8>], cff2: bool) -> Result<Self> {
        let count = u32::try_from(items.len()).map_err(|_| Error)?;
        if !cff2 {
            u16::try_from(count).map_err(|_| Error)?;
        }
        let header = if cff2 { 4usize } else { 2usize };
        let len = items
            .iter()
            .try_fold(1u32, |sum, s| sum.checked_add(u32::try_from(s.len()).ok()?))
            .ok_or(Error)?;
        let width = if len <= 255 {
            1
        } else if len <= 65535 {
            2
        } else if len <= 0xffffff {
            3
        } else {
            4
        };
        let size = if count == 0 {
            header
        } else {
            items
                .len()
                .checked_add(1)
                .and_then(|n| n.checked_mul(width))
                .and_then(|n| n.checked_add(header + 1))
                .and_then(|n| n.checked_add((len - 1) as usize))
                .ok_or(Error)?
        };
        Ok(Self {
            count,
            width,
            size,
            cff2,
        })
    }
    pub fn write(&self, out: &mut Vec<u8>, items: &[Vec<u8>]) {
        if self.cff2 {
            out.extend(self.count.to_be_bytes());
        } else {
            out.extend((self.count as u16).to_be_bytes());
        }
        if self.count == 0 {
            return;
        }
        out.push(self.width as u8);
        let mut offset = 1u32;
        for s in items {
            out.extend(&offset.to_be_bytes()[4 - self.width..]);
            offset += s.len() as u32;
        }
        out.extend(&offset.to_be_bytes()[4 - self.width..]);
        for s in items {
            out.extend(s);
        }
    }
}

pub(super) fn index(items: &[Vec<u8>], cff2: bool) -> Result<Vec<u8>> {
    let layout = IndexLayout::new(items, cff2)?;
    let mut out = Vec::with_capacity(layout.size);
    layout.write(&mut out, items);
    Ok(out)
}

pub(super) fn bias(count: usize) -> i32 {
    if count < 1240 {
        107
    } else if count < 33900 {
        1131
    } else {
        32768
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::ps::cff::index::Index;
    #[test]
    fn numeric_boundaries() {
        for dict in [false, true] {
            for v in [
                -32768., -1132., -1131., -108., -107., -0.25, 0., 107., 108., 1131., 1132., 32767.,
            ] {
                let mut out = Vec::new();
                encode(&mut out, v, dict).unwrap();
                assert_eq!(number(&out, &mut 0, dict).unwrap(), Some(v));
            }
        }
        assert!(encode(&mut Vec::new(), 32768., false).is_err());
    }
    #[test]
    fn empty_and_variable_width_indexes() {
        for cff2 in [false, true] {
            for size in [0, 1, 254, 255, 65535] {
                let items = vec![vec![1; size], vec![], vec![2; 8]];
                let data = index(&items, cff2).unwrap();
                let read = Index::new(&data, cff2).unwrap();
                for (i, s) in items.iter().enumerate() {
                    assert_eq!(read.get(i), Some(s.as_slice()));
                }
            }
            assert_eq!(index(&[], cff2).unwrap().len(), if cff2 { 4 } else { 2 });
        }
    }
}
