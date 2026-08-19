//! A builder for the 'glyf' and 'loca' tables

use crate::{
    error::Error,
    tables::loca::{Loca, LocaFormat},
    validate::Validate,
    FontWrite, TableWriter,
};

use super::{CompositeGlyph, Glyf, Glyph, SimpleGlyph};

/// A builder for constructing the 'glyf' & 'loca' tables.
///
/// These two tables are tightly coupled, and are necessarily constructed
/// together.
///
/// # Example
///
/// ```
/// use write_fonts::tables::glyf::{Glyph, GlyfLocaBuilder};
/// # fn get_glyphs() -> Vec<(String, Glyph)> { Vec::new() }
///
/// let names_and_glyphs: Vec<(String, Glyph)> = get_glyphs();
/// let mut builder = GlyfLocaBuilder::new();
///
/// for (name, glyph) in names_and_glyphs {
///     // your error handling goes here
///     if let Err(e) = builder.add_glyph(&glyph) {
///         panic!("error compiling glyph '{name}': '{e}'");
///     }
/// }
///
/// let (_glyf, _loca, _loca_format) = builder.build();
/// // store the results somewhere
/// ```
pub struct GlyfLocaBuilder {
    glyph_writer: TableWriter,
    raw_loca: Vec<u32>,
}

/// A trait encompassing [`Glyph`], [`SimpleGlyph`] and [`CompositeGlyph`]
///
/// This is a marker trait to ensure that only glyphs are passed to [`GlyfLocaBuilder`].
pub trait SomeGlyph: Validate + FontWrite {}

impl GlyfLocaBuilder {
    /// Construct a new builder for the 'glyf' and 'loca' tables.
    pub fn new() -> Self {
        Self {
            glyph_writer: TableWriter::default(),
            raw_loca: vec![0],
        }
    }

    /// Add a glyph to the table.
    ///
    /// The argument can be any of [`Glyph`], [`SimpleGlyph`] or [`CompositeGlyph`].
    ///
    /// The glyph is validated and compiled immediately, so that the caller can
    /// associate any errors with a particular glyph.
    pub fn add_glyph(&mut self, glyph: &impl SomeGlyph) -> Result<&mut Self, Error> {
        glyph.validate()?;
        glyph.write_into(&mut self.glyph_writer);
        let pos = self.glyph_writer.current_data().bytes.len();
        self.raw_loca.push(pos as u32);
        Ok(self)
    }

    /// Construct the final glyf and loca tables.
    ///
    /// This method also returns the loca format; the caller is responsible for
    /// setting this field in the ['head'] table.
    ///
    /// [`head`]: crate::tables::head::Head::index_to_loc_format
    #[must_use]
    pub fn build(self) -> (Glyf, Loca, LocaFormat) {
        let glyph_data = self.glyph_writer.into_data().bytes;
        let loca = Loca::new(self.raw_loca);
        let format = loca.format();
        (Glyf(glyph_data), loca, format)
    }
}

impl SomeGlyph for SimpleGlyph {}

impl SomeGlyph for CompositeGlyph {}

impl SomeGlyph for Glyph {}

impl Default for GlyfLocaBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::super::{Anchor, Component, ComponentFlags, Transform};
    use crate::from_obj::FromTableRef;
    use font_types::GlyphId;
    use kurbo::{BezPath, Shape};
    use read_fonts::FontRead;

    use super::*;

    #[test]
    fn build_some_glyphs() {
        fn make_triangle() -> BezPath {
            let mut path = BezPath::new();
            path.move_to((0., 0.));
            path.line_to((0., 40.));
            path.line_to((20., 40.));
            path.line_to((0., 0.));
            path
        }
        let square = kurbo::Rect::from_points((5., 5.), (100., 100.)).into_path(0.1);
        let triangle = make_triangle();

        let glyph0 = Glyph::Empty;
        let glyph1 = SimpleGlyph::from_bezpath(&square).unwrap();
        let glyph2 = SimpleGlyph::from_bezpath(&triangle).unwrap();
        let gid1 = GlyphId::new(1);
        let gid2 = GlyphId::new(2);

        let mut glyph3 = CompositeGlyph::new(
            Component::new(
                gid1.try_into().unwrap(),
                Anchor::Offset { x: 0, y: 0 },
                Transform::default(),
                ComponentFlags::default(),
            ),
            square.bounding_box(),
        );
        glyph3.add_component(
            Component::new(
                gid2.try_into().unwrap(),
                Anchor::Offset { x: 0, y: 0 },
                Transform::default(),
                ComponentFlags::default(),
            ),
            triangle.bounding_box(),
        );

        let len1 = crate::dump_table(&glyph1).unwrap().len() as u32;
        let len2 = crate::dump_table(&glyph2).unwrap().len() as u32;
        let len3 = crate::dump_table(&glyph3).unwrap().len() as u32;

        let mut builder = GlyfLocaBuilder::new();
        builder
            .add_glyph(&glyph0)
            .unwrap()
            .add_glyph(&glyph1)
            .unwrap()
            .add_glyph(&glyph2)
            .unwrap()
            .add_glyph(&glyph3)
            .unwrap();

        let (glyf, loca, format) = builder.build();
        assert_eq!(loca.offsets.len(), 5);
        assert_eq!(loca.offsets, &[0, 0, len1, len1 + len2, len1 + len2 + len3]);

        let rglyf = read_fonts::tables::glyf::Glyf::read(glyf.0.as_slice().into()).unwrap();
        let loca_bytes = crate::dump_table(&loca).unwrap();
        let rloca = read_fonts::tables::loca::Loca::read(
            loca_bytes.as_slice().into(),
            format == LocaFormat::Long,
        )
        .unwrap();

        let rglyph1 = Glyph::from_table_ref(&rloca.get_glyf(gid1, &rglyf).unwrap().unwrap());
        let rglyph2 = Glyph::from_table_ref(&rloca.get_glyf(gid2, &rglyf).unwrap().unwrap());
        let rglyph3 =
            Glyph::from_table_ref(&rloca.get_glyf(GlyphId::new(3), &rglyf).unwrap().unwrap());
        assert_eq!(rglyph1, glyph1.into());
        assert_eq!(rglyph2, glyph2.into());
        assert_eq!(rglyph3, glyph3.into());
    }
}
