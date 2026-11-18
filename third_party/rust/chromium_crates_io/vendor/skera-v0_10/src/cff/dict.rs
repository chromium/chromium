//! DICTs retain their original bytes unless an entry needs rewriting.

use super::{
    encoding::{self, emit_op},
    Error, Result,
};

#[derive(Clone)]
pub(super) struct Entry<'a> {
    pub op: u16,
    pub args: Vec<f64>,
    pub raw: std::borrow::Cow<'a, [u8]>,
}

pub(super) fn parse(data: &[u8]) -> Result<Vec<Entry<'_>>> {
    parse_private(data, |_| Err(Error))
}

pub(super) fn parse_private(
    data: &[u8],
    regions: impl Fn(usize) -> Result<usize>,
) -> Result<Vec<Entry<'_>>> {
    let mut out = Vec::new();
    let mut p = 0;
    let mut start = 0;
    let mut args = Vec::new();
    let mut ivs = 0;
    while p < data.len() {
        if let Some(v) = encoding::number(data, &mut p, true)? {
            args.push(v);
        } else {
            let op = encoding::op(data, &mut p)?;
            // Preserve the complete encoded entry independently of the live
            // operand stack. Each blend replaces its defaults/deltas/count.
            if op == 23 {
                let n = uint(args.pop().ok_or(Error)?)?;
                let k = regions(ivs)?;
                let consumed = n.checked_mul(k.checked_add(1).ok_or(Error)?).ok_or(Error)?;
                let from = args.len().checked_sub(consumed).ok_or(Error)?;
                args.truncate(from.checked_add(n).ok_or(Error)?);
                continue;
            }
            if op == 22 {
                ivs = uint(*args.first().ok_or(Error)?)?;
            }
            out.push(Entry {
                op,
                args: std::mem::take(&mut args),
                raw: data[start..p].into(),
            });
            start = p;
        }
        if args.len() > 513 {
            return Err(Error);
        }
    }
    if start != p {
        return Err(Error);
    }
    Ok(out)
}

pub(super) fn arg(entries: &[Entry], op: u16, index: usize) -> Option<f64> {
    entries
        .iter()
        .rev()
        .find(|e| e.op == op)?
        .args
        .get(index)
        .copied()
}

pub(super) fn uint(v: f64) -> Result<usize> {
    if v < 0. || v > i32::MAX as f64 || v.fract() != 0. {
        return Err(Error);
    }
    Ok(v as usize)
}

pub(super) fn offset(entries: &[Entry], op: u16, index: usize) -> Result<Option<usize>> {
    arg(entries, op, index).map(uint).transpose()
}

pub(super) fn replacement(op: u16, args: &[usize]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    for &arg in args {
        encoding::long(&mut out, i32::try_from(arg).map_err(|_| Error)?);
    }
    emit_op(&mut out, op);
    Ok(out)
}

pub(super) fn is_hint(op: u16) -> bool {
    // Matches HarfBuzz's dict_opset_t::is_hint_op: BlueValues..StdVW,
    // BlueScale..ForceBold, LanguageGroup and ExpansionFactor.
    matches!(op, 6..=11 | 0x109..=0x10e | 0x111 | 0x112)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn private_blends_count_live_operands_and_preserve_bytes() {
        let mut data = Vec::new();
        for _ in 0..14 {
            data.extend(std::iter::repeat_n(139, 41));
            data.extend([140, 23]);
        }
        data.push(6);
        let entries = parse_private(&data, |_| Ok(40)).unwrap();
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0].args, vec![0.; 14]);
        assert_eq!(entries[0].raw.as_ref(), data);
        assert!(parse_private(&[139, 141, 23, 6], |_| Ok(40)).is_err());
        let mut overflow = vec![139; 514];
        overflow.push(6);
        assert!(parse_private(&overflow, |_| Ok(40)).is_err());
    }
}
