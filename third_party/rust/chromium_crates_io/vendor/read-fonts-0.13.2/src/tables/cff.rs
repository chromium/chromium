//! The [CFF](https://learn.microsoft.com/en-us/typography/opentype/spec/cff) table

include!("../../generated/generated_cff.rs");

use super::postscript::{Index1, Latin1String, StringId};

/// The [Compact Font Format](https://learn.microsoft.com/en-us/typography/opentype/spec/cff) table.
#[derive(Clone)]
pub struct Cff<'a> {
    header: CffHeader<'a>,
    names: Index1<'a>,
    top_dicts: Index1<'a>,
    strings: Index1<'a>,
    global_subrs: Index1<'a>,
}

impl<'a> Cff<'a> {
    pub fn offset_data(&self) -> FontData<'a> {
        self.header.offset_data()
    }

    pub fn header(&self) -> CffHeader<'a> {
        self.header.clone()
    }

    /// Returns the name index.
    ///
    /// This contains the PostScript names of all fonts in the font set.
    ///
    /// See "Name INDEX" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=13>
    pub fn names(&self) -> Index1<'a> {
        self.names.clone()
    }

    /// Returns the PostScript name for the font in the font set at the
    /// given index.
    pub fn name(&self, index: usize) -> Option<Latin1String<'a>> {
        Some(Latin1String::new(self.names.get(index).ok()?))
    }

    /// Returns the top dict index.
    ///
    /// This contains the top-level DICTs of all fonts in the font set. The
    /// objects here correspond to those in the name index.
    ///
    /// See "Top DICT INDEX" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=14>
    pub fn top_dicts(&self) -> Index1<'a> {
        self.top_dicts.clone()
    }

    /// Returns the string index.
    ///
    /// This contains all of the strings used by fonts within the font set.
    /// They are referenced by string identifiers represented by the
    /// [`StringId`] type.
    ///
    /// See "String INDEX" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=17>
    pub fn strings(&self) -> Index1<'a> {
        self.strings.clone()
    }

    /// Returns the associated string for the given identifier.
    ///
    /// If the identifier does not represent a standard string, the result is
    /// looked up in the string index.
    pub fn string(&self, id: StringId) -> Option<Latin1String<'a>> {
        match id.standard_string() {
            Ok(name) => Some(name),
            Err(ix) => self.strings.get(ix).ok().map(Latin1String::new),
        }
    }

    /// Returns the global subroutine index.
    ///
    /// This contains sub-programs that are referenced by one or more
    /// charstrings in the font set.
    ///
    /// See "Local/Global Subrs INDEXes" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=25>
    pub fn global_subrs(&self) -> Index1<'a> {
        self.global_subrs.clone()
    }
}

impl TopLevelTable for Cff<'_> {
    const TAG: Tag = Tag::new(b"CFF ");
}

impl<'a> FontRead<'a> for Cff<'a> {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        let header = CffHeader::read(data)?;
        let mut data = FontData::new(header.trailing_data());
        let names = Index1::read(data)?;
        data = data
            .split_off(names.size_in_bytes()?)
            .ok_or(ReadError::OutOfBounds)?;
        let top_dicts = Index1::read(data)?;
        data = data
            .split_off(top_dicts.size_in_bytes()?)
            .ok_or(ReadError::OutOfBounds)?;
        let strings = Index1::read(data)?;
        data = data
            .split_off(strings.size_in_bytes()?)
            .ok_or(ReadError::OutOfBounds)?;
        let global_subrs = Index1::read(data)?;
        Ok(Self {
            header,
            names,
            top_dicts,
            strings,
            global_subrs,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{tables::postscript::StringId, FontRef, TableProvider};

    #[test]
    fn read_noto_serif_display_cff() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = font.cff().unwrap();
        assert_eq!(cff.header().major(), 1);
        assert_eq!(cff.header().minor(), 0);
        assert_eq!(cff.top_dicts().count(), 1);
        assert_eq!(cff.names().count(), 1);
        assert_eq!(cff.global_subrs.count(), 17);
        let name = Latin1String::new(cff.names().get(0).unwrap());
        assert_eq!(name, "NotoSerifDisplay-Regular");
        assert_eq!(cff.strings().count(), 5);
        // Version
        assert_eq!(cff.string(StringId::new(391)).unwrap(), "2.9");
        // Notice
        assert_eq!(
            cff.string(StringId::new(392)).unwrap(),
            "Noto is a trademark of Google LLC."
        );
        // Copyright
        assert_eq!(
            cff.string(StringId::new(393)).unwrap(),
            "Copyright 2022 The Noto Project Authors https:github.comnotofontslatin-greek-cyrillic"
        );
        // FullName
        assert_eq!(
            cff.string(StringId::new(394)).unwrap(),
            "Noto Serif Display Regular"
        );
        // FamilyName
        assert_eq!(
            cff.string(StringId::new(395)).unwrap(),
            "Noto Serif Display"
        );
    }
}
