use read_fonts::{
    ps::cff::charset::Charset,
    tables::{cff::Cff, post::Post},
    TableProvider,
};

use crate::hb::face::FontKind;

#[derive(Clone)]
pub enum GlyphNames<'a> {
    None,
    Cff(Cff<'a>, Charset<'a>),
    Post(Post<'a>),
}

impl<'a> GlyphNames<'a> {
    pub fn new(font: &FontKind<'a>) -> Self {
        match font {
            FontKind::FontRef(font) => Self::from_tables(&font.font),
            FontKind::FontInstance(instance, _) => Self::from_tables(&instance.tables()),
        }
    }

    pub(crate) fn from_tables(font: &impl TableProvider<'a>) -> Self {
        if let Some((cff, charset)) = font
            .cff()
            .ok()
            .and_then(|cff| Some((cff.clone(), cff.charset(0).ok()??)))
        {
            Self::Cff(cff, charset)
        } else if let Ok(post) = font.post() {
            Self::Post(post)
        } else {
            Self::None
        }
    }

    pub fn get(&self, glyph_id: u32) -> Option<&str> {
        let name = match self {
            Self::Cff(cff, charset) => {
                let sid = charset.string_id(glyph_id.into()).ok()?;
                core::str::from_utf8(cff.string(sid)?).ok()
            }
            Self::Post(post) => {
                let gid: u16 = glyph_id.try_into().ok()?;
                post.glyph_name(gid.into())
            }
            Self::None => None,
        }?;
        (!name.is_empty()).then_some(name)
    }
}
