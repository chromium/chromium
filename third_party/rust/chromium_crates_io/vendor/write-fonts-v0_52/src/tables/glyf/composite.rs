//! Composite glyphs (containing other glyphs as components)

use crate::{
    from_obj::{FromObjRef, FromTableRef, ToOwnedTable},
    FontWrite,
};

use read_fonts::{tables::glyf::CompositeGlyphFlags, types::GlyphId16, FontRead, ReadArgs};

use super::Bbox;

pub use read_fonts::tables::glyf::{Anchor, Transform};

/// A glyph consisting of multiple component sub-glyphs
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CompositeGlyph {
    pub bbox: Bbox,
    components: Vec<Component>,
    instructions: Vec<u8>,
}

/// A single component glyph (part of a [`CompositeGlyph`]).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Component {
    pub glyph: GlyphId16,
    pub anchor: Anchor,
    pub flags: ComponentFlags,
    pub transform: Transform,
}

/// Options that can be manually set for a given component.
///
/// This provides an easier interface for setting those flags that are not
/// calculated based on other properties of the glyph. For more information
/// on these flags, see [Component Glyph Flags](flags-spec) in the spec.
///
/// These eventually are combined with calculated flags into the
/// [`CompositeGlyphFlags`] bitset.
///
/// [flags-spec]: https://learn.microsoft.com/en-us/typography/opentype/spec/glyf#compositeGlyphFlags
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct ComponentFlags {
    /// Round xy values to the nearest grid line
    pub round_xy_to_grid: bool,
    /// Use the advance/lsb/rsb values of this component for the whole
    /// composite glyph
    pub use_my_metrics: bool,
    /// The composite should have this component's offset scaled
    pub scaled_component_offset: bool,
    /// The composite should *not* have this component's offset scaled
    pub unscaled_component_offset: bool,
    /// If set, the components of the composite glyph overlap.
    pub overlap_compound: bool,
}

impl FromObjRef<read_fonts::tables::glyf::CompositeGlyph<'_>> for CompositeGlyph {
    fn from_obj_ref(
        from: &read_fonts::tables::glyf::CompositeGlyph,
        _data: read_fonts::FontData,
    ) -> Self {
        let bbox = Bbox {
            x_min: from.x_min(),
            y_min: from.y_min(),
            x_max: from.x_max(),
            y_max: from.y_max(),
        };
        let components = from
            .components()
            .map(|c| Component {
                glyph: c.glyph,
                anchor: c.anchor,
                flags: c.flags.into(),
                transform: c.transform,
            })
            .collect();
        Self {
            bbox,
            components,
            instructions: from
                .instructions()
                .map(|v| v.to_owned())
                .unwrap_or_default(),
        }
    }
}

impl FromTableRef<read_fonts::tables::glyf::CompositeGlyph<'_>> for CompositeGlyph {}

impl ReadArgs for CompositeGlyph {
    type Args = ();
}

impl<'a> FontRead<'a> for CompositeGlyph {
    fn read_with_args(
        data: read_fonts::FontData<'a>,
        _: (),
    ) -> Result<Self, read_fonts::ReadError> {
        read_fonts::tables::glyf::CompositeGlyph::read(data).map(|g| g.to_owned_table())
    }
}

impl Component {
    /// Create a new component.
    pub fn new(
        glyph: GlyphId16,
        anchor: Anchor,
        transform: Transform,
        flags: impl Into<ComponentFlags>,
    ) -> Self {
        Component {
            glyph,
            anchor,
            flags: flags.into(),
            transform,
        }
    }
    /// Compute the flags for this glyph, excepting `MORE_COMPONENTS` and
    /// `WE_HAVE_INSTRUCTIONS`, which must be set manually
    fn compute_flag(&self) -> CompositeGlyphFlags {
        self.anchor.compute_flags() | self.transform.compute_flags() | self.flags.into()
    }

    /// like `FontWrite` but lets us pass in the flags that must be determined
    /// externally (WE_HAVE_INSTRUCTIONS and MORE_COMPONENTS)
    fn write_into(&self, writer: &mut crate::TableWriter, extra_flags: CompositeGlyphFlags) {
        let flags = self.compute_flag() | extra_flags;
        flags.bits().write_into(writer);
        self.glyph.write_into(writer);
        self.anchor.write_into(writer);
        self.transform.write_into(writer);
    }
}

/// An error that occurs if a `CompositeGlyph` is constructed with no components.
#[derive(Clone, Copy, Debug)]
#[non_exhaustive]
pub struct NoComponents;

impl std::fmt::Display for NoComponents {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "A composite glyph must contain at least one component")
    }
}

impl std::error::Error for NoComponents {}

impl CompositeGlyph {
    /// Create a new composite glyph, with the provided component.
    ///
    /// The 'bbox' argument is the bounding box of the glyph after the transform
    /// has been applied.
    ///
    /// Additional components can be added with [`add_component`][Self::add_component]
    pub fn new(component: Component, bbox: impl Into<Bbox>) -> Self {
        Self {
            bbox: bbox.into(),
            components: vec![component],
            instructions: Default::default(),
        }
    }

    /// Add a new component to this glyph
    ///
    /// The 'bbox' argument is the bounding box of the glyph after the transform
    /// has been applied.
    pub fn add_component(&mut self, component: Component, bbox: impl Into<Bbox>) {
        self.components.push(component);
        self.bbox = self.bbox.union(bbox.into());
    }

    /// Construct a `CompositeGlyph` from an iterator of `Component` and `Bbox`es.
    ///
    /// This returns an error if the iterator is empty; a CompositeGlyph must always
    /// contain at least one component.
    pub fn try_from_iter(
        source: impl IntoIterator<Item = (Component, Bbox)>,
    ) -> Result<Self, NoComponents> {
        let mut components = Vec::new();
        let mut union_box: Option<Bbox> = None;

        for (component, bbox) in source {
            components.push(component);
            union_box.get_or_insert(bbox).union(bbox);
        }

        if components.is_empty() {
            Err(NoComponents)
        } else {
            Ok(CompositeGlyph {
                bbox: union_box.unwrap(),
                components,
                instructions: Default::default(),
            })
        }
    }

    pub fn components(&self) -> &[Component] {
        &self.components
    }

    pub fn components_mut(&mut self) -> &mut [Component] {
        &mut self.components
    }

    pub fn set_instructions(&mut self, instructions: &[u8]) {
        self.instructions = instructions.to_vec();
    }

    pub fn instructions(&self) -> &[u8] {
        &self.instructions
    }
}

impl FontWrite for CompositeGlyph {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        const N_CONTOURS: i16 = -1;
        N_CONTOURS.write_into(writer);
        self.bbox.write_into(writer);
        let (last, rest) = self
            .components
            .split_last()
            .expect("empty composites checked in validation");
        for comp in rest {
            comp.write_into(writer, CompositeGlyphFlags::MORE_COMPONENTS);
        }
        let last_flags = if self.instructions.is_empty() {
            CompositeGlyphFlags::empty()
        } else {
            CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS
        };
        last.write_into(writer, last_flags);

        if !self.instructions.is_empty() {
            (self.instructions.len() as u16).write_into(writer);
            self.instructions.write_into(writer);
        }
        writer.pad_to_2byte_aligned();
    }
}

impl crate::validate::Validate for CompositeGlyph {
    fn validate_impl(&self, ctx: &mut crate::codegen_prelude::ValidationCtx) {
        if self.components.is_empty() {
            ctx.report("composite glyph must have components");
        }
        if self.instructions.len() > u16::MAX as usize {
            ctx.report("instructions len overflows");
        }
    }
}

impl FontWrite for Anchor {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        let two_bytes = self
            .compute_flags()
            .contains(CompositeGlyphFlags::ARG_1_AND_2_ARE_WORDS);
        match self {
            Anchor::Offset { x, y } if !two_bytes => [*x as i8, *y as i8].write_into(writer),
            Anchor::Offset { x, y } => [*x, *y].write_into(writer),
            Anchor::Point { base, component } if !two_bytes => {
                [*base as u8, *component as u8].write_into(writer)
            }
            Anchor::Point { base, component } => [*base, *component].write_into(writer),
        }
    }
}

impl FontWrite for Transform {
    fn write_into(&self, writer: &mut crate::TableWriter) {
        let flags = self.compute_flags();
        if flags.contains(CompositeGlyphFlags::WE_HAVE_A_TWO_BY_TWO) {
            [self.xx, self.yx, self.xy, self.yy].write_into(writer);
        } else if flags.contains(CompositeGlyphFlags::WE_HAVE_AN_X_AND_Y_SCALE) {
            [self.xx, self.yy].write_into(writer);
        } else if flags.contains(CompositeGlyphFlags::WE_HAVE_A_SCALE) {
            self.xx.write_into(writer)
        }
    }
}

impl From<CompositeGlyphFlags> for ComponentFlags {
    fn from(src: CompositeGlyphFlags) -> ComponentFlags {
        ComponentFlags {
            round_xy_to_grid: src.contains(CompositeGlyphFlags::ROUND_XY_TO_GRID),
            use_my_metrics: src.contains(CompositeGlyphFlags::USE_MY_METRICS),
            scaled_component_offset: src.contains(CompositeGlyphFlags::SCALED_COMPONENT_OFFSET),
            unscaled_component_offset: src.contains(CompositeGlyphFlags::UNSCALED_COMPONENT_OFFSET),
            overlap_compound: src.contains(CompositeGlyphFlags::OVERLAP_COMPOUND),
        }
    }
}

impl From<ComponentFlags> for CompositeGlyphFlags {
    fn from(value: ComponentFlags) -> Self {
        (if value.round_xy_to_grid {
            CompositeGlyphFlags::ROUND_XY_TO_GRID
        } else {
            Default::default()
        }) | if value.use_my_metrics {
            CompositeGlyphFlags::USE_MY_METRICS
        } else {
            Default::default()
        } | if value.scaled_component_offset {
            CompositeGlyphFlags::SCALED_COMPONENT_OFFSET
        } else {
            Default::default()
        } | if value.unscaled_component_offset {
            CompositeGlyphFlags::UNSCALED_COMPONENT_OFFSET
        } else {
            Default::default()
        } | if value.overlap_compound {
            CompositeGlyphFlags::OVERLAP_COMPOUND
        } else {
            Default::default()
        }
    }
}

#[cfg(test)]
mod tests {

    use read_fonts::{
        tables::glyf as read_glyf, types::GlyphId, FontData, FontRead, FontRef, TableProvider,
    };

    use super::*;

    #[test]
    fn roundtrip_composite() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let read_glyf::Glyph::Composite(orig) =
            loca.get_glyf(GlyphId::new(2), &glyf).unwrap().unwrap()
        else {
            panic!("not a composite glyph")
        };

        let bbox = Bbox {
            x_min: orig.x_min(),
            y_min: orig.y_min(),
            x_max: orig.x_max(),
            y_max: orig.y_max(),
        };
        let mut iter = orig
            .components()
            .map(|comp| Component::new(comp.glyph, comp.anchor, comp.transform, comp.flags));
        let mut composite = CompositeGlyph::new(iter.next().unwrap(), bbox);
        composite.add_component(iter.next().unwrap(), bbox);
        composite.instructions = orig.instructions().unwrap_or_default().to_vec();
        assert!(iter.next().is_none());
        let bytes = crate::dump_table(&composite).unwrap();
        let ours = read_fonts::tables::glyf::CompositeGlyph::read(FontData::new(&bytes)).unwrap();

        let our_comps = ours.components().collect::<Vec<_>>();
        let orig_comps = orig.components().collect::<Vec<_>>();
        assert_eq!(our_comps.len(), orig_comps.len());
        assert_eq!(our_comps.len(), 2);
        assert_eq!(&our_comps[0], &orig_comps[0]);
        assert_eq!(&our_comps[1], &orig_comps[1]);
        assert_eq!(ours.instructions(), orig.instructions());
        assert_eq!(orig.offset_data().len(), bytes.len());

        assert_eq!(orig.offset_data().as_ref(), bytes);
    }
}
