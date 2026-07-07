//! Font representation.

mod blob;
mod format;
mod instance;
mod source;
mod tables;

pub use blob::FontBlob;
pub use format::FontFormat;
pub use instance::{
    FontFeatureVariations, FontInstance, FontInstanceBuilder, FontVariation, NormalizedCoord,
};
pub use source::FontSource;
pub use tables::{FontTableFunction, FontTables};

// Do our best to not expose this to users through docs or rust-analyzer.
#[doc(hidden)]
#[rust_analyzer::completions(hidden_from_completion)]
pub mod interop;

use super::once::Once;
use crate::{ps::type1::Type1Font, ReadError};
use alloc::{boxed::Box, sync::Arc};
use core::any::Any;

/// An OpenType or PostScript font.
///
/// This type is internally reference counted, cheaply cloneable and thread
/// safe.
#[derive(Clone)]
pub struct Font(Arc<FontRepr>);

impl Font {
    /// Creates a new font from the given source and font index.
    ///
    /// The index parameter specifies the desired font in a font collection
    /// (ttc or otc) file. It is ignored if the data source is not a blob.
    pub fn new(source: impl Into<FontSource>, index: u32) -> Result<Self, ReadError> {
        let source = source.into();
        let kind = if let Ok(tables) = FontTables::new(source.clone(), index) {
            Some(FontKindRepr::Sfnt(tables, index))
        } else if let FontSource::Blob(blob) = &source {
            match FontFormat::new(blob) {
                Some(FontFormat::Type1) => Type1Font::new(blob).ok().map(FontKindRepr::Type1),
                // TODO: pure CFF fonts
                _ => None,
            }
        } else {
            None
        };
        let kind = kind.ok_or(ReadError::MalformedData("Data isn't a font"))?;
        let repr = FontRepr {
            source,
            kind,
            shaping_data: Once::new(),
        };
        Ok(Self(Arc::new(repr)))
    }

    /// Returns the underlying source of font data.
    pub fn source(&self) -> &FontSource {
        &self.0.source
    }

    /// Returns the underlying kind of the font.
    pub fn kind(&self) -> FontKind<'_> {
        match &self.0.kind {
            FontKindRepr::Sfnt(tables, index) => FontKind::Sfnt(tables, *index),
            FontKindRepr::Type1(font) => FontKind::Type1(font),
        }
    }

    /// Returns an object that provides access to individual font tables.
    ///
    /// For non-SFNT fonts, this will return an empty set of tables.
    pub fn tables(&self) -> &FontTables {
        if let FontKindRepr::Sfnt(tables, _) = &self.0.kind {
            tables
        } else {
            &tables::EMPTY_FONT_TABLES
        }
    }
}

struct FontRepr {
    source: FontSource,
    kind: FontKindRepr,
    // Storage cell for lazily loaded HarfRust shaping data.
    shaping_data: Once<Box<dyn Any + Send + Sync>>,
}

/// The underlying type of a font.
#[derive(Clone)]
pub enum FontKind<'a> {
    /// An SFNT-based font represented by a set of tables and an index.
    Sfnt(&'a FontTables, u32),
    /// An Adobe Type1 font.
    Type1(&'a Type1Font),
}

/// The underlying type of a font.
#[expect(clippy::large_enum_variant)]
enum FontKindRepr {
    Sfnt(FontTables, u32),
    Type1(Type1Font),
}
