//! Borrowed source tables, with one local-subroutine namespace per Font DICT.

use super::{
    dict::{self, Entry},
    Error, Result,
};
use read_fonts::{
    ps::cff::{index::Index, CffFontRef},
    types::GlyphId,
    FontData, FontRead,
};

pub(super) trait CffFlavor {
    const CFF2: bool;
    const STACK_LIMIT: usize;
    fn empty_glyph() -> Vec<u8>;
}
pub(super) struct Cff1;
pub(super) struct Cff2;
impl CffFlavor for Cff1 {
    const CFF2: bool = false;
    const STACK_LIMIT: usize = 48;
    fn empty_glyph() -> Vec<u8> {
        vec![14]
    }
}
impl CffFlavor for Cff2 {
    const CFF2: bool = true;
    const STACK_LIMIT: usize = 513;
    fn empty_glyph() -> Vec<u8> {
        vec![]
    }
}

pub(super) struct FontDict<'a> {
    pub entries: Vec<Entry<'a>>,
    pub private: Vec<Entry<'a>>,
    pub subrs: Index<'a>,
    pub ivs: usize,
}
pub(super) struct Source<'a> {
    pub font: CffFontRef<'a>,
    pub top: Vec<Entry<'a>>,
    pub names: Vec<Vec<u8>>,
    pub strings: Index<'a>,
    pub fds: Vec<FontDict<'a>>,
    pub cid: bool,
}

pub(super) trait CharStringSource {
    fn initial_ivs(&self, fd: usize) -> usize;
    fn program(&self, global: bool, fd: usize, index: usize) -> Result<&[u8]>;
    fn subr_count(&self, global: bool, fd: usize) -> usize;
    fn subr_bias(&self, global: bool, fd: usize) -> i32 {
        super::encoding::bias(self.subr_count(global, fd))
    }
    fn region_count(&self, ivs: usize) -> Result<usize>;
}
impl CharStringSource for Source<'_> {
    fn initial_ivs(&self, fd: usize) -> usize {
        self.fds[fd].ivs
    }
    fn program(&self, global: bool, fd: usize, index: usize) -> Result<&[u8]> {
        let ix = if global {
            self.font.global_subrs()
        } else {
            &self.fds.get(fd).ok_or(Error)?.subrs
        };
        ix.get(index).ok_or(Error)
    }
    fn subr_count(&self, global: bool, fd: usize) -> usize {
        if global {
            self.font.global_subrs().count() as usize
        } else {
            self.fds[fd].subrs.count() as usize
        }
    }
    fn subr_bias(&self, global: bool, fd: usize) -> i32 {
        if global {
            self.font.global_subrs().subr_bias()
        } else {
            self.fds[fd].subrs.subr_bias()
        }
    }
    fn region_count(&self, ivs: usize) -> Result<usize> {
        let store = self.font.var_store().ok_or(Error)?;
        let data = store
            .item_variation_data()
            .get(ivs)
            .ok_or(Error)?
            .map_err(|_| Error)?;
        Ok(data.region_index_count() as usize)
    }
}

impl<'a> Source<'a> {
    pub fn new(data: &'a [u8]) -> Result<Self> {
        let font = CffFontRef::new(data, 0, None).map_err(|_| Error)?;
        let cff2 = font.version() == 2;
        let (top, names, strings) = if cff2 {
            let table =
                read_fonts::tables::cff2::Cff2::read(FontData::new(data)).map_err(|_| Error)?;
            (
                dict::parse(table.top_dict_data())?,
                Vec::new(),
                Index::Empty,
            )
        } else {
            let table =
                read_fonts::tables::cff::Cff::read(FontData::new(data)).map_err(|_| Error)?;
            (
                dict::parse(table.top_dicts().get(0).ok_or(Error)?)?,
                vec![table.name(0).ok_or(Error)?.to_vec()],
                table.strings().into(),
            )
        };
        let cid = font.is_cid();
        let fd_entries = if let Some(offset) = dict::offset(&top, 0x124, 0)? {
            let index = Index::new(data.get(offset..).ok_or(Error)?, cff2).map_err(|_| Error)?;
            (0..index.count() as usize)
                .map(|i| dict::parse(index.get(i).ok_or(Error)?))
                .collect::<Result<Vec<_>>>()?
        } else if cff2 || cid {
            return Err(Error);
        } else {
            vec![Vec::new()]
        };
        let mut fds = Vec::new();
        for entries in fd_entries {
            let parent = if !cff2 && !cid { &top } else { &entries };
            let size = dict::offset(parent, 18, 0)?.unwrap_or(0);
            let offset = dict::offset(parent, 18, 1)?.unwrap_or(0);
            let private = dict::parse_private(
                data.get(offset..offset.checked_add(size).ok_or(Error)?)
                    .ok_or(Error)?,
                |ivs| {
                    let store = font.var_store().ok_or(Error)?;
                    Ok(store
                        .item_variation_data()
                        .get(ivs)
                        .ok_or(Error)?
                        .map_err(|_| Error)?
                        .region_index_count() as usize)
                },
            )?;
            let subrs = if let Some(relative) = dict::offset(&private, 19, 0)? {
                Index::new(
                    data.get(offset.checked_add(relative).ok_or(Error)?..)
                        .ok_or(Error)?,
                    cff2,
                )
                .map_err(|_| Error)?
            } else {
                Index::Empty
            };
            let ivs = dict::offset(&private, 22, 0)?.unwrap_or(0);
            fds.push(FontDict {
                entries,
                private,
                subrs,
                ivs,
            });
        }
        if fds.is_empty() {
            return Err(Error);
        }
        Ok(Self {
            font,
            top,
            names,
            strings,
            fds,
            cid,
        })
    }
    pub fn fd(&self, gid: GlyphId) -> Result<usize> {
        let fd = if gid.to_u32() >= self.font.num_glyphs() {
            0
        } else {
            self.font.subfont_index(gid).ok_or(Error)? as usize
        };
        if fd >= self.fds.len() {
            return Err(Error);
        }
        Ok(fd)
    }
}
