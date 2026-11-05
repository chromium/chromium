//! The width, style and weight of a font.
//!
//! [`Font::default_attributes`] returns the attributes of the default
//! instance. [`Attributes::from_variations`] and the named instance
//! constructors return the attributes at other locations. They take the
//! same user space values as [`InstanceBuilder`](super::InstanceBuilder).

use super::{variation::Variation, Font, Kind};
use crate::{
    tables::{
        head::{Head, MacStyle},
        os2::{Os2, SelectionFlags},
        post::Post,
    },
    types::Tag,
    TableProvider,
};

/// The width, style and weight of a font at one location.
#[derive(Copy, Clone, PartialEq, Debug, Default)]
pub struct Attributes {
    /// How wide the design is relative to its normal width.
    pub width: Width,
    /// Whether the design is upright, italic or oblique.
    pub style: Style,
    /// How heavy the design is.
    pub weight: Weight,
}

impl Attributes {
    /// Returns the attributes at the location given by `variations`, in user
    /// space.
    ///
    /// Each attribute starts from the font's default attributes. Values for
    /// the `wdth`, `ital`, `slnt` and `wght` axes override them. Variations
    /// are applied the same way as when building an instance. A variation
    /// for an axis the font does not have is ignored, the last value for an
    /// axis is used, and each value is clamped to the range of its axis.
    pub fn from_variations<V>(font: &Font, variations: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        let mut attributes = font.default_attributes();
        let axes = font.axes();
        if axes.is_empty() {
            return attributes;
        }
        let (mut width, mut italic, mut slant, mut weight) = (None, None, None, None);
        for variation in variations {
            let variation = variation.into();
            let slot = match variation.tag {
                WDTH => &mut width,
                ITAL => &mut italic,
                SLNT => &mut slant,
                WGHT => &mut weight,
                _ => continue,
            };
            // As in `Axes::filter`, a value applies to every axis with the
            // tag, each clamped to its own range; the last such axis wins.
            for axis in axes.iter().filter(|axis| axis.tag() == variation.tag) {
                *slot = Some(axis.clamp(variation.value));
            }
        }
        if let Some(width) = width {
            attributes.width = Width::new(width / 100.0);
        }
        attributes.style = style_at(attributes.style, italic, slant);
        if let Some(weight) = weight {
            attributes.weight = Weight::new(weight);
        }
        attributes
    }

    /// Returns the attributes of the named instance at `index`.
    ///
    /// Returns the font's default attributes if there is no such instance.
    pub fn from_named_instance(font: &Font, index: u32) -> Self {
        Self::from_named_instance_with_overrides(font, index, core::iter::empty::<Variation>())
    }

    /// Returns the attributes of the named instance at `index`, with
    /// `overrides` applied after the instance's own values.
    ///
    /// If there is no such instance, only the overrides apply.
    pub fn from_named_instance_with_overrides<V>(font: &Font, index: u32, overrides: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        let instance = font.named_instances().get(index);
        let named = instance.iter().flat_map(|instance| instance.variations());
        Self::from_variations(font, named.chain(overrides.into_iter().map(Into::into)))
    }

    /// Returns the attributes of the default instance of `font`.
    ///
    /// An SFNT font defines them in its tables. A Type 1 or standalone CFF
    /// font has only a weight name and an italic angle, which are read the
    /// same way as FreeType reads them. These fonts define no width.
    pub(crate) fn from_font(font: &Font) -> Self {
        match font.kind() {
            Kind::Sfnt(..) => Self::from_sfnt(&font.tables()),
            Kind::Type1(type1) => {
                Self::from_postscript(type1.weight(), type1.italic_angle() as f32)
            }
            Kind::Cff(cff, _) => cff.metadata().map_or_else(Self::default, |metadata| {
                Self::from_postscript(metadata.weight(), metadata.italic_angle().to_f32())
            }),
        }
    }

    /// Returns the attributes defined by a PostScript font's `Weight` name
    /// and `ItalicAngle`.
    ///
    /// A nonzero italic angle means italic, matching how FreeType sets its
    /// italic flag.
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/type1/t1objs.c#L462>
    fn from_postscript(weight: Option<&str>, italic_angle: f32) -> Self {
        Self {
            width: Width::default(),
            style: if italic_angle != 0.0 {
                Style::Italic
            } else {
                Style::Normal
            },
            weight: weight.map_or_else(Weight::default, Weight::from_name),
        }
    }

    /// Reads the attributes of the default instance from the font's tables.
    ///
    /// These come from OS/2, with the `post` italic angle for an oblique
    /// style. If there is no OS/2 table, they come from `head`.
    fn from_sfnt<'a>(tables: &impl TableProvider<'a>) -> Self {
        if let Ok(os2) = tables.os2() {
            Self::from_os2_post(os2, tables.post().ok())
        } else if let Ok(head) = tables.head() {
            Self::from_head(head)
        } else {
            Self::default()
        }
    }

    fn from_os2_post(os2: Os2, post: Option<Post>) -> Self {
        let width = Width::from_width_class(os2.us_width_class());
        // Bits 0 and 9 of fsSelection mark italic and oblique.
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/os2#fsselection>
        let fs_selection = os2.fs_selection();
        let style = if fs_selection.contains(SelectionFlags::ITALIC) {
            Style::Italic
        } else if fs_selection.contains(SelectionFlags::OBLIQUE) {
            Style::Oblique(post.map(|post| post.italic_angle().to_f32()))
        } else {
            Style::Normal
        };
        // usWeightClass is specified from 1 to 1000, but a variable font can
        // state a value outside it, so it is not clamped.
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/os2#usweightclass>
        let weight = Weight(os2.us_weight_class() as f32);
        Self {
            width,
            style,
            weight,
        }
    }

    fn from_head(head: Head) -> Self {
        let mac_style = head.mac_style();
        let style = if mac_style.contains(MacStyle::ITALIC) {
            Style::Italic
        } else {
            Style::default()
        };
        let weight = if mac_style.contains(MacStyle::BOLD) {
            Weight::BOLD
        } else {
            Weight::default()
        };
        Self {
            width: Width::default(),
            style,
            weight,
        }
    }
}

const WDTH: Tag = Tag::new(b"wdth");
const ITAL: Tag = Tag::new(b"ital");
const SLNT: Tag = Tag::new(b"slnt");
const WGHT: Tag = Tag::new(b"wght");

/// Returns the style at a location with the given `italic` and `slant`
/// values, starting from the font's `default` style.
///
/// An `ital` value of 0.5 or more is italic, and a smaller value is not
/// italic. A nonzero `slnt` value is oblique by that angle unless the style
/// is italic. A `slnt` value of zero is not oblique.
fn style_at(default: Style, italic: Option<f32>, slant: Option<f32>) -> Style {
    let mut style = default;
    match italic {
        Some(italic) if italic >= 0.5 => style = Style::Italic,
        Some(_) if style == Style::Italic => style = Style::Normal,
        _ => {}
    }
    match slant {
        _ if style == Style::Italic => {}
        Some(slant) if slant != 0.0 => style = Style::Oblique(Some(slant)),
        Some(_) if matches!(style, Style::Oblique(_)) => style = Style::Normal,
        _ => {}
    }
    style
}

/// How wide a design is relative to its normal width, typically from 0.5 to
/// 2.0.
///
/// In variable fonts, this can be controlled with the `wdth` axis, which
/// expresses the width as a percentage.
///
/// See <https://fonts.google.com/knowledge/glossary/width>
#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct Width(f32);

impl Width {
    /// Width that is 50% of normal.
    pub const ULTRA_CONDENSED: Self = Self(0.5);

    /// Width that is 62.5% of normal.
    pub const EXTRA_CONDENSED: Self = Self(0.625);

    /// Width that is 75% of normal.
    pub const CONDENSED: Self = Self(0.75);

    /// Width that is 87.5% of normal.
    pub const SEMI_CONDENSED: Self = Self(0.875);

    /// Width that is 100% of normal.
    pub const NORMAL: Self = Self(1.0);

    /// Width that is 112.5% of normal.
    pub const SEMI_EXPANDED: Self = Self(1.125);

    /// Width that is 125% of normal.
    pub const EXPANDED: Self = Self(1.25);

    /// Width that is 150% of normal.
    pub const EXTRA_EXPANDED: Self = Self(1.5);

    /// Width that is 200% of normal.
    pub const ULTRA_EXPANDED: Self = Self(2.0);

    /// Creates a width from its ratio to the normal width.
    pub const fn new(ratio: f32) -> Self {
        Self(ratio)
    }

    /// Creates a width from the
    /// [usWidthClass](https://learn.microsoft.com/en-us/typography/opentype/spec/os2#uswidthclass)
    /// field of the OS/2 table.
    fn from_width_class(width_class: u16) -> Self {
        // The specified range is 1 to 9, and Skia clamps values outside it.
        // <https://skia.googlesource.com/skia/+/21b7538fe0757d8cda31598bc9e5a6d0b4b54629/include/core/SkFontStyle.h#52>
        match width_class {
            0..=1 => Self::ULTRA_CONDENSED,
            2 => Self::EXTRA_CONDENSED,
            3 => Self::CONDENSED,
            4 => Self::SEMI_CONDENSED,
            5 => Self::NORMAL,
            6 => Self::SEMI_EXPANDED,
            7 => Self::EXPANDED,
            8 => Self::EXTRA_EXPANDED,
            _ => Self::ULTRA_EXPANDED,
        }
    }

    /// Returns the width as a ratio, where 1.0 is the normal width.
    pub const fn ratio(self) -> f32 {
        self.0
    }

    /// Returns the width as a percentage, which is the value used by the
    /// `wdth` axis.
    pub fn percentage(self) -> f32 {
        self.0 * 100.0
    }
}

impl Default for Width {
    fn default() -> Self {
        Self::NORMAL
    }
}

/// Whether a design is upright, italic or oblique.
///
/// In variable fonts, this can be controlled with the `ital` and `slnt`
/// axes for italic and oblique styles.
///
/// See <https://fonts.google.com/knowledge/glossary/style>
#[derive(Copy, Clone, PartialEq, Default, Debug)]
pub enum Style {
    /// An upright or "roman" style.
    #[default]
    Normal,
    /// Generally a slanted style, originally based on semi-cursive forms.
    /// This often has a different structure from the normal style.
    Italic,
    /// Oblique (or slanted) style with an optional angle in degrees,
    /// counter-clockwise from the vertical.
    Oblique(Option<f32>),
}

/// How heavy a design is, typically from 1.0 to 1000.0.
///
/// In variable fonts, this can be controlled with the `wght` axis.
///
/// See <https://fonts.google.com/knowledge/glossary/weight>
#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct Weight(f32);

impl Weight {
    /// Weight value of 100.
    pub const THIN: Self = Self(100.0);

    /// Weight value of 200.
    pub const EXTRA_LIGHT: Self = Self(200.0);

    /// Weight value of 300.
    pub const LIGHT: Self = Self(300.0);

    /// Weight value of 350.
    pub const SEMI_LIGHT: Self = Self(350.0);

    /// Weight value of 400.
    pub const NORMAL: Self = Self(400.0);

    /// Weight value of 500.
    pub const MEDIUM: Self = Self(500.0);

    /// Weight value of 600.
    pub const SEMI_BOLD: Self = Self(600.0);

    /// Weight value of 700.
    pub const BOLD: Self = Self(700.0);

    /// Weight value of 800.
    pub const EXTRA_BOLD: Self = Self(800.0);

    /// Weight value of 900.
    pub const BLACK: Self = Self(900.0);

    /// Weight value of 950.
    pub const EXTRA_BLACK: Self = Self(950.0);

    /// Creates a weight with the given value.
    pub const fn new(weight: f32) -> Self {
        Self(weight)
    }

    /// Returns the weight for a PostScript `Weight` name, such as "Bold" or
    /// "SemiBold".
    ///
    /// Names are matched without regard to case, spaces, hyphens or
    /// underscores. An unknown name returns the normal weight.
    fn from_name(name: &str) -> Self {
        const NAMES: &[(&str, Weight)] = &[
            ("thin", Weight::THIN),
            ("hairline", Weight::THIN),
            ("extralight", Weight::EXTRA_LIGHT),
            ("ultralight", Weight::EXTRA_LIGHT),
            ("light", Weight::LIGHT),
            ("semilight", Weight::SEMI_LIGHT),
            ("demilight", Weight::SEMI_LIGHT),
            ("book", Weight::NORMAL),
            ("regular", Weight::NORMAL),
            ("normal", Weight::NORMAL),
            ("roman", Weight::NORMAL),
            ("medium", Weight::MEDIUM),
            ("semibold", Weight::SEMI_BOLD),
            ("demibold", Weight::SEMI_BOLD),
            ("demi", Weight::SEMI_BOLD),
            ("bold", Weight::BOLD),
            ("extrabold", Weight::EXTRA_BOLD),
            ("ultrabold", Weight::EXTRA_BOLD),
            ("black", Weight::BLACK),
            ("heavy", Weight::BLACK),
            ("extrablack", Weight::EXTRA_BLACK),
            ("ultrablack", Weight::EXTRA_BLACK),
        ];
        let letters = name
            .bytes()
            .filter(|byte| !matches!(byte, b' ' | b'-' | b'_'))
            .map(|byte| byte.to_ascii_lowercase());
        NAMES
            .iter()
            .find(|(known, _)| letters.clone().eq(known.bytes()))
            .map_or(Self::NORMAL, |(_, weight)| *weight)
    }

    /// Returns the weight's value.
    pub const fn value(self) -> f32 {
        self.0
    }
}

impl Default for Weight {
    fn default() -> Self {
        Self::NORMAL
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::VAZIRMATN_VAR;

    #[test]
    fn missing_os2() {
        let font = Font::new(font_test_data::CMAP12_FONT1, 0).unwrap();
        let attributes = font.default_attributes();
        assert_eq!(attributes.width, Width::NORMAL);
        assert_eq!(attributes.style, Style::Italic);
        assert_eq!(attributes.weight, Weight::BOLD);
    }

    #[test]
    fn so_stylish() {
        let font = Font::new(font_test_data::CMAP14_FONT1, 0).unwrap();
        let attributes = font.default_attributes();
        assert_eq!(attributes.width, Width::SEMI_CONDENSED);
        assert_eq!(attributes.style, Style::Oblique(Some(-14.0)));
        assert_eq!(attributes.weight, Weight::EXTRA_BOLD);
    }

    #[test]
    fn variations_override_the_axes_the_font_has() {
        let font = Font::new(VAZIRMATN_VAR, 0).unwrap();
        let default = font.default_attributes();
        // No variations: the default attributes.
        assert_eq!(
            Attributes::from_variations(&font, core::iter::empty::<Variation>()),
            default
        );
        // wght is an axis of the font, clamped to it, and the last value wins.
        let at = |variations: &[(&str, f32)]| Attributes::from_variations(&font, variations);
        assert_eq!(at(&[("wght", 650.0)]).weight, Weight::new(650.0));
        assert_eq!(at(&[("wght", 2000.0)]).weight, Weight::new(900.0));
        assert_eq!(
            at(&[("wght", 300.0), ("wght", 500.0)]).weight,
            Weight::MEDIUM
        );
        // The font has no wdth, ital or slnt axis, so those are ignored.
        let ignored = at(&[("wdth", 75.0), ("ital", 1.0), ("slnt", -10.0)]);
        assert_eq!(ignored, default);
        // A static font has no axes at all.
        let tinos = Font::new(font_test_data::TINOS_SUBSET, 0).unwrap();
        assert_eq!(
            Attributes::from_variations(&tinos, [("wght", 700.0)]),
            tinos.default_attributes()
        );
    }

    #[test]
    fn named_instances_give_their_own_attributes() {
        let font = Font::new(VAZIRMATN_VAR, 0).unwrap();
        // Instance 6 is Bold.
        assert_eq!(
            Attributes::from_named_instance(&font, 6).weight,
            Weight::BOLD
        );
        assert_eq!(
            Attributes::from_named_instance_with_overrides(&font, 6, [("wght", 800.0)]).weight,
            Weight::EXTRA_BOLD
        );
        // No such instance: the default attributes, then the overrides.
        let missing = font.named_instances().len();
        assert_eq!(
            Attributes::from_named_instance(&font, missing),
            font.default_attributes()
        );
        assert_eq!(
            Attributes::from_named_instance_with_overrides(&font, missing, [("wght", 300.0)])
                .weight,
            Weight::LIGHT
        );
    }

    // A Type 1 font states its weight by name and its style by angle, as
    // FreeType reads them; it states no width.
    #[test]
    fn a_type1_font_states_its_weight_name_and_angle() {
        let regular = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap();
        assert_eq!(regular.default_attributes(), Attributes::default());
        let header = core::str::from_utf8(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA)
            .unwrap()
            .replace("/Weight (Book)", "/Weight (Bold)")
            .replace("/ItalicAngle 0 def", "/ItalicAngle -12 def");
        let bold_italic = Font::new(header.into_bytes(), 0).unwrap();
        let attributes = bold_italic.default_attributes();
        assert_eq!(attributes.weight, Weight::BOLD);
        assert_eq!(attributes.style, Style::Italic);
        assert_eq!(attributes.width, Width::NORMAL);
        // A standalone CFF font that names no weight is normal.
        let sfnt = crate::FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = sfnt.cff().unwrap().offset_data().as_bytes().to_vec();
        let cff = Font::new(cff, 0).unwrap();
        assert!(matches!(cff.kind(), Kind::Cff(..)));
        assert_eq!(cff.default_attributes(), Attributes::default());
    }

    #[test]
    fn weight_names() {
        for (name, weight) in [
            ("Bold", Weight::BOLD),
            ("bold", Weight::BOLD),
            ("Black", Weight::BLACK),
            ("Semi Bold", Weight::SEMI_BOLD),
            ("Demi-Bold", Weight::SEMI_BOLD),
            ("ExtraLight", Weight::EXTRA_LIGHT),
            ("Book", Weight::NORMAL),
            ("Roman", Weight::NORMAL),
            ("Medium", Weight::MEDIUM),
            ("Heavy", Weight::BLACK),
            ("Unheard Of", Weight::NORMAL),
            ("", Weight::NORMAL),
        ] {
            assert_eq!(Weight::from_name(name), weight, "{name:?}");
        }
    }

    #[test]
    fn italic_and_slant_axes_choose_the_style() {
        let oblique = Style::Oblique(Some(-12.0));
        // Nothing given keeps the default style.
        assert_eq!(style_at(oblique, None, None), oblique);
        // ital from one half up is italic, and it wins over slnt.
        assert_eq!(
            style_at(Style::Normal, Some(1.0), Some(-10.0)),
            Style::Italic
        );
        assert_eq!(style_at(Style::Normal, Some(0.5), None), Style::Italic);
        assert_eq!(style_at(Style::Italic, Some(0.0), None), Style::Normal);
        // A nonzero slnt is oblique by that angle, and zero is upright.
        assert_eq!(
            style_at(Style::Normal, None, Some(-10.0)),
            Style::Oblique(Some(-10.0))
        );
        assert_eq!(style_at(oblique, None, Some(0.0)), Style::Normal);
        // A default italic stays italic whatever the slant.
        assert_eq!(style_at(Style::Italic, None, Some(-10.0)), Style::Italic);
    }
}
