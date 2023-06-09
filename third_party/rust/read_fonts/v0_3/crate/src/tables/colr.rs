//! The [COLR](https://docs.microsoft.com/en-us/typography/opentype/spec/colr) table

use super::variations::{DeltaSetIndex, DeltaSetIndexMap, ItemVariationStore};
use types::BoundingBox;

use std::ops::Deref;

include!("../../generated/generated_colr.rs");

/// Identifier used for representing a paint on the recursion blacklist.
pub type PaintId = usize;

impl<'a> Colr<'a> {
    /// Returns the COLRv0 base glyph for the given glyph identifier.
    ///
    /// The return value is a range of layer indices that can be passed to
    /// [`v0_layer`](Self::v0_layer) to retrieve the layer glyph identifiers
    /// and palette color indices.
    pub fn v0_base_glyph(&self, glyph_id: GlyphId) -> Result<Option<Range<usize>>, ReadError> {
        let records = self.base_glyph_records().ok_or(ReadError::NullOffset)??;
        let record = match records.binary_search_by(|rec| rec.glyph_id().cmp(&glyph_id)) {
            Ok(ix) => &records[ix],
            _ => return Ok(None),
        };
        let start = record.first_layer_index() as usize;
        let end = start + record.num_layers() as usize;
        Ok(Some(start..end))
    }

    /// Returns the COLRv0 layer at the given index.
    ///
    /// The layer is represented by a tuple containing the glyph identifier of
    /// the associated outline and the palette color index.
    pub fn v0_layer(&self, index: usize) -> Result<(GlyphId, u16), ReadError> {
        let layers = self.layer_records().ok_or(ReadError::NullOffset)??;
        let layer = layers.get(index).ok_or(ReadError::OutOfBounds)?;
        Ok((layer.glyph_id(), layer.palette_index()))
    }

    /// Returns the COLRv1 base glyph for the given glyph identifier.
    ///
    /// The second value in the tuple is a unique identifier for the paint that
    /// may be used to detect recursion in the paint graph.
    pub fn v1_base_glyph(
        &self,
        glyph_id: GlyphId,
    ) -> Result<Option<(Paint<'a>, PaintId)>, ReadError> {
        let list = self.base_glyph_list().ok_or(ReadError::NullOffset)??;
        let records = list.base_glyph_paint_records();
        let record = match records.binary_search_by(|rec| rec.glyph_id().cmp(&glyph_id)) {
            Ok(ix) => &records[ix],
            _ => return Ok(None),
        };
        let offset_data = list.offset_data();
        // Use the address of the paint as an identifier for the recursion
        // blacklist.
        let id = record.paint_offset().to_u32() as usize + offset_data.as_ref().as_ptr() as usize;
        Ok(Some((record.paint(offset_data)?, id)))
    }

    /// Returns the COLRv1 layer at the given index.
    ///
    /// The second value in the tuple is a unique identifier for the paint that
    /// may be used to detect recursion in the paint graph.
    pub fn v1_layer(&self, index: usize) -> Result<(Paint<'a>, PaintId), ReadError> {
        let list = self.layer_list().ok_or(ReadError::NullOffset)??;
        let offset = list
            .paint_offsets()
            .get(index)
            .ok_or(ReadError::OutOfBounds)?
            .get();
        let offset_data = list.offset_data();
        // Use the address of the paint as an identifier for the recursion
        // blacklist.
        let id = offset.to_u32() as usize + offset_data.as_ref().as_ptr() as usize;
        Ok((offset.resolve(offset_data)?, id))
    }

    /// Returns the COLRv1 clip box for the given glyph identifier.
    pub fn v1_clip_box(&self, glyph_id: GlyphId) -> Result<Option<ClipBox<'a>>, ReadError> {
        use std::cmp::Ordering;
        let list = self.clip_list().ok_or(ReadError::NullOffset)??;
        let clips = list.clips();
        let clip = match clips.binary_search_by(|clip| {
            if glyph_id < clip.start_glyph_id() {
                Ordering::Greater
            } else if glyph_id > clip.end_glyph_id() {
                Ordering::Less
            } else {
                Ordering::Equal
            }
        }) {
            Ok(ix) => &clips[ix],
            _ => return Ok(None),
        };
        Ok(Some(clip.clip_box(list.offset_data())?))
    }
}

/// Combination of a `COLR` table and a location in variation space for
/// resolving paints.
///
/// See [`Paint::resolve`], [`ColorStops::resolve`] and [`ClipBox::resolve`].
pub struct ColrInstance<'a> {
    colr: Colr<'a>,
    index_map: Option<DeltaSetIndexMap<'a>>,
    var_store: Option<ItemVariationStore<'a>>,
    coords: &'a [F2Dot14],
}

impl<'a> ColrInstance<'a> {
    /// Creates a new instance for the given `COLR` table and normalized variation
    /// coordinates.
    pub fn new(colr: Colr<'a>, coords: &'a [F2Dot14]) -> Self {
        let index_map = colr.var_index_map().and_then(|res| res.ok());
        let var_store = colr.item_variation_store().and_then(|res| res.ok());
        Self {
            colr,
            coords,
            index_map,
            var_store,
        }
    }

    /// Computes a sequence of N variation deltas starting at the given
    /// `var_base` index.
    fn var_deltas<const N: usize>(&self, var_index_base: u32) -> [i32; N] {
        // Magic value that indicates deltas should not be applied.
        const NO_VARIATION_DELTAS: u32 = 0xFFFFFFFF;
        // Note: FreeType never returns an error for these lookups, so
        // we do the same and just `unwrap_or_default` on var store
        // errors.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/fc01e7dd/src/sfnt/ttcolr.c#L574>
        let mut deltas = [0; N];
        if self.coords.is_empty()
            || self.var_store.is_none()
            || var_index_base == NO_VARIATION_DELTAS
        {
            return deltas;
        }
        let var_store = self.var_store.as_ref().unwrap();
        if let Some(index_map) = self.index_map.as_ref() {
            for (i, delta) in deltas.iter_mut().enumerate() {
                let var_index = var_index_base + i as u32;
                *delta = index_map
                    .get(var_index)
                    .and_then(|delta_index| var_store.compute_delta(delta_index, self.coords))
                    .unwrap_or_default();
            }
        } else {
            for (i, delta) in deltas.iter_mut().enumerate() {
                let var_index = var_index_base + i as u32;
                // If we don't have a var index map, use our index as the inner
                // component and set the outer to 0.
                let delta_index = DeltaSetIndex {
                    outer: 0,
                    inner: var_index as u16,
                };
                *delta = var_store
                    .compute_delta(delta_index, self.coords)
                    .unwrap_or_default();
            }
        }
        deltas
    }
}

impl<'a> Deref for ColrInstance<'a> {
    type Target = Colr<'a>;

    fn deref(&self) -> &Self::Target {
        &self.colr
    }
}

impl<'a> ClipBox<'a> {
    /// Resolves a clip box, applying variation deltas using the given
    /// instance.
    pub fn resolve(&self, instance: &ColrInstance<'a>) -> BoundingBox<Fixed> {
        match self {
            Self::Format1(cbox) => BoundingBox {
                x_min: cbox.x_min().to_fixed(),
                y_min: cbox.y_min().to_fixed(),
                x_max: cbox.x_max().to_fixed(),
                y_max: cbox.y_max().to_fixed(),
            },
            Self::Format2(cbox) => {
                let deltas = instance.var_deltas::<4>(cbox.var_index_base());
                BoundingBox {
                    x_min: cbox.x_min().apply_delta(deltas[0]),
                    y_min: cbox.y_min().apply_delta(deltas[1]),
                    x_max: cbox.x_max().apply_delta(deltas[2]),
                    y_max: cbox.y_max().apply_delta(deltas[3]),
                }
            }
        }
    }
}

/// Simplified version of a [`ColorStop`] or [`VarColorStop`] with applied
/// variation deltas.
#[derive(Clone, Debug)]
pub struct ResolvedColorStop {
    pub offset: Fixed,
    pub palette_index: u16,
    pub alpha: Fixed,
}

/// Collection of [`ColorStop`] or [`VarColorStop`].
// Note: only one of these fields is used at any given time, but this structure
// was chosen over the obvious enum approach for simplicity in generating a
// single concrete type for the `impl Iterator` return type of the `resolve`
// method.
pub struct ColorStops<'a> {
    stops: &'a [ColorStop],
    var_stops: &'a [VarColorStop],
}

impl<'a> From<ColorLine<'a>> for ColorStops<'a> {
    fn from(value: ColorLine<'a>) -> Self {
        Self {
            stops: value.color_stops(),
            var_stops: &[],
        }
    }
}

impl<'a> From<VarColorLine<'a>> for ColorStops<'a> {
    fn from(value: VarColorLine<'a>) -> Self {
        Self {
            stops: &[],
            var_stops: value.color_stops(),
        }
    }
}

impl<'a> ColorStops<'a> {
    /// Returns an iterator yielding resolved color stops with variation deltas
    /// applied.
    pub fn resolve(
        &self,
        instance: &'a ColrInstance<'a>,
    ) -> impl Iterator<Item = ResolvedColorStop> + 'a {
        self.stops
            .iter()
            .map(|stop| ResolvedColorStop {
                offset: stop.stop_offset().to_fixed(),
                palette_index: stop.palette_index(),
                alpha: stop.alpha().to_fixed(),
            })
            .chain(self.var_stops.iter().map(|stop| {
                let deltas = instance.var_deltas::<2>(stop.var_index_base());
                ResolvedColorStop {
                    offset: stop.stop_offset().apply_delta(deltas[0]),
                    palette_index: stop.palette_index(),
                    alpha: stop.alpha().apply_delta(deltas[1]),
                }
            }))
    }
}

/// Simplified version of `Paint` with applied variation deltas.
///
/// These are constructed with [`Paint::resolve`] method. See the documentation
/// on that method for further detail.
///
/// This is roughly equivalent to FreeType's
/// [`FT_COLR_Paint`](https://freetype.org/freetype2/docs/reference/ft2-layer_management.html#ft_colr_paint)
/// type.
pub enum ResolvedPaint<'a> {
    ColrLayers {
        range: Range<usize>,
    },
    Solid {
        palette_index: u16,
        alpha: Fixed,
    },
    LinearGradient {
        x0: Fixed,
        y0: Fixed,
        x1: Fixed,
        y1: Fixed,
        x2: Fixed,
        y2: Fixed,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    RadialGradient {
        x0: Fixed,
        y0: Fixed,
        radius0: Fixed,
        x1: Fixed,
        y1: Fixed,
        radius1: Fixed,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    SweepGradient {
        center_x: Fixed,
        center_y: Fixed,
        start_angle: Fixed,
        end_angle: Fixed,
        color_stops: ColorStops<'a>,
        extend: Extend,
    },
    Glyph {
        glyph_id: GlyphId,
        paint: Paint<'a>,
    },
    ColrGlyph {
        glyph_id: GlyphId,
    },
    Transform {
        xx: Fixed,
        yx: Fixed,
        xy: Fixed,
        yy: Fixed,
        dx: Fixed,
        dy: Fixed,
        paint: Paint<'a>,
    },
    Translate {
        dx: Fixed,
        dy: Fixed,
        paint: Paint<'a>,
    },
    Scale {
        scale_x: Fixed,
        scale_y: Fixed,
        around_center: Option<Point<Fixed>>,
        paint: Paint<'a>,
    },
    Rotate {
        angle: Fixed,
        around_center: Option<Point<Fixed>>,
        paint: Paint<'a>,
    },
    Skew {
        x_skew_angle: Fixed,
        y_skew_angle: Fixed,
        around_center: Option<Point<Fixed>>,
        paint: Paint<'a>,
    },
    Composite {
        source_paint: Paint<'a>,
        mode: CompositeMode,
        backdrop_paint: Paint<'a>,
    },
}

impl<'a> Paint<'a> {
    /// Resolves this paint with the given instance.
    ///
    /// Resolving means that all numeric values are converted to 16.16 fixed
    /// point, variation deltas are applied, and the various transform
    /// paints are collapsed into a single value for their category (transform,
    /// translate, scale, rotate and skew).
    ///
    /// This provides a simpler type for consumers that are more interested
    /// in extracting the semantics of the graph rather than working with the
    /// raw encoded structures.
    pub fn resolve(&self, instance: &ColrInstance<'a>) -> Result<ResolvedPaint<'a>, ReadError> {
        Ok(match self {
            Self::ColrLayers(layers) => {
                let start = layers.first_layer_index() as usize;
                ResolvedPaint::ColrLayers {
                    range: start..start + layers.num_layers() as usize,
                }
            }
            Self::Solid(solid) => ResolvedPaint::Solid {
                palette_index: solid.palette_index(),
                alpha: solid.alpha().to_fixed(),
            },
            Self::VarSolid(solid) => {
                let deltas = instance.var_deltas::<1>(solid.var_index_base());
                ResolvedPaint::Solid {
                    palette_index: solid.palette_index(),
                    alpha: solid.alpha().apply_delta(deltas[0]),
                }
            }
            Self::LinearGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                ResolvedPaint::LinearGradient {
                    x0: gradient.x0().to_fixed(),
                    y0: gradient.y0().to_fixed(),
                    x1: gradient.x1().to_fixed(),
                    y1: gradient.y1().to_fixed(),
                    x2: gradient.x2().to_fixed(),
                    y2: gradient.y2().to_fixed(),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::VarLinearGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                let deltas = instance.var_deltas::<6>(gradient.var_index_base());
                ResolvedPaint::LinearGradient {
                    x0: gradient.x0().apply_delta(deltas[0]),
                    y0: gradient.y0().apply_delta(deltas[1]),
                    x1: gradient.x1().apply_delta(deltas[2]),
                    y1: gradient.y1().apply_delta(deltas[3]),
                    x2: gradient.x2().apply_delta(deltas[4]),
                    y2: gradient.y2().apply_delta(deltas[5]),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::RadialGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                ResolvedPaint::RadialGradient {
                    x0: gradient.x0().to_fixed(),
                    y0: gradient.y0().to_fixed(),
                    radius0: gradient.radius0().to_fixed(),
                    x1: gradient.x1().to_fixed(),
                    y1: gradient.y1().to_fixed(),
                    radius1: gradient.radius1().to_fixed(),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::VarRadialGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                let deltas = instance.var_deltas::<6>(gradient.var_index_base());
                ResolvedPaint::RadialGradient {
                    x0: gradient.x0().apply_delta(deltas[0]),
                    y0: gradient.y0().apply_delta(deltas[1]),
                    radius0: gradient.radius0().apply_delta(deltas[2]),
                    x1: gradient.x1().apply_delta(deltas[3]),
                    y1: gradient.y1().apply_delta(deltas[4]),
                    radius1: gradient.radius1().apply_delta(deltas[5]),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::SweepGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                ResolvedPaint::SweepGradient {
                    center_x: gradient.center_x().to_fixed(),
                    center_y: gradient.center_y().to_fixed(),
                    start_angle: gradient.start_angle().to_fixed(),
                    end_angle: gradient.end_angle().to_fixed(),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::VarSweepGradient(gradient) => {
                let color_line = gradient.color_line()?;
                let extend = color_line.extend();
                let deltas = instance.var_deltas::<4>(gradient.var_index_base());
                ResolvedPaint::SweepGradient {
                    center_x: gradient.center_x().apply_delta(deltas[0]),
                    center_y: gradient.center_y().apply_delta(deltas[1]),
                    start_angle: gradient.start_angle().apply_delta(deltas[2]),
                    end_angle: gradient.end_angle().apply_delta(deltas[3]),
                    color_stops: color_line.into(),
                    extend,
                }
            }
            Self::Glyph(glyph) => ResolvedPaint::Glyph {
                glyph_id: glyph.glyph_id(),
                paint: glyph.paint()?,
            },
            Self::ColrGlyph(glyph) => ResolvedPaint::ColrGlyph {
                glyph_id: glyph.glyph_id(),
            },
            Self::Transform(transform) => {
                let affine = transform.transform()?;
                let paint = transform.paint()?;
                ResolvedPaint::Transform {
                    xx: affine.xx(),
                    yx: affine.yx(),
                    xy: affine.xy(),
                    yy: affine.yy(),
                    dx: affine.dx(),
                    dy: affine.dy(),
                    paint,
                }
            }
            Self::VarTransform(transform) => {
                let affine = transform.transform()?;
                let paint = transform.paint()?;
                let deltas = instance.var_deltas::<6>(affine.var_index_base());
                ResolvedPaint::Transform {
                    xx: affine.xx().apply_delta(deltas[0]),
                    yx: affine.yx().apply_delta(deltas[1]),
                    xy: affine.xy().apply_delta(deltas[2]),
                    yy: affine.yy().apply_delta(deltas[3]),
                    dx: affine.dx().apply_delta(deltas[4]),
                    dy: affine.dy().apply_delta(deltas[5]),
                    paint,
                }
            }
            Self::Translate(transform) => ResolvedPaint::Translate {
                dx: transform.dx().to_fixed(),
                dy: transform.dy().to_fixed(),
                paint: transform.paint()?,
            },
            Self::VarTranslate(transform) => {
                let deltas = instance.var_deltas::<2>(transform.var_index_base());
                ResolvedPaint::Translate {
                    dx: transform.dx().apply_delta(deltas[0]),
                    dy: transform.dy().apply_delta(deltas[1]),
                    paint: transform.paint()?,
                }
            }
            Self::Scale(transform) => ResolvedPaint::Scale {
                scale_x: transform.scale_x().to_fixed(),
                scale_y: transform.scale_y().to_fixed(),
                around_center: None,
                paint: transform.paint()?,
            },
            Self::VarScale(transform) => {
                let deltas = instance.var_deltas::<2>(transform.var_index_base());
                ResolvedPaint::Scale {
                    scale_x: transform.scale_x().apply_delta(deltas[0]),
                    scale_y: transform.scale_y().apply_delta(deltas[1]),
                    around_center: None,
                    paint: transform.paint()?,
                }
            }
            Self::ScaleAroundCenter(transform) => ResolvedPaint::Scale {
                scale_x: transform.scale_x().to_fixed(),
                scale_y: transform.scale_y().to_fixed(),
                around_center: Some(Point::new(
                    transform.center_x().to_fixed(),
                    transform.center_y().to_fixed(),
                )),
                paint: transform.paint()?,
            },
            Self::VarScaleAroundCenter(transform) => {
                let deltas = instance.var_deltas::<4>(transform.var_index_base());
                ResolvedPaint::Scale {
                    scale_x: transform.scale_x().apply_delta(deltas[0]),
                    scale_y: transform.scale_y().apply_delta(deltas[1]),
                    around_center: Some(Point::new(
                        transform.center_x().apply_delta(deltas[2]),
                        transform.center_y().apply_delta(deltas[3]),
                    )),
                    paint: transform.paint()?,
                }
            }
            Self::ScaleUniform(transform) => {
                let scale = transform.scale().to_fixed();
                ResolvedPaint::Scale {
                    scale_x: scale,
                    scale_y: scale,
                    around_center: None,
                    paint: transform.paint()?,
                }
            }
            Self::VarScaleUniform(transform) => {
                let deltas = instance.var_deltas::<1>(transform.var_index_base());
                let scale = transform.scale().apply_delta(deltas[0]);
                ResolvedPaint::Scale {
                    scale_x: scale,
                    scale_y: scale,
                    around_center: None,
                    paint: transform.paint()?,
                }
            }
            Self::ScaleUniformAroundCenter(transform) => {
                let scale = transform.scale().to_fixed();
                ResolvedPaint::Scale {
                    scale_x: scale,
                    scale_y: scale,
                    around_center: Some(Point::new(
                        transform.center_x().to_fixed(),
                        transform.center_y().to_fixed(),
                    )),
                    paint: transform.paint()?,
                }
            }
            Self::VarScaleUniformAroundCenter(transform) => {
                let deltas = instance.var_deltas::<3>(transform.var_index_base());
                let scale = transform.scale().apply_delta(deltas[0]);
                ResolvedPaint::Scale {
                    scale_x: scale,
                    scale_y: scale,
                    around_center: Some(Point::new(
                        transform.center_x().apply_delta(deltas[1]),
                        transform.center_y().apply_delta(deltas[2]),
                    )),
                    paint: transform.paint()?,
                }
            }
            Self::Rotate(transform) => ResolvedPaint::Rotate {
                angle: transform.angle().to_fixed(),
                around_center: None,
                paint: transform.paint()?,
            },
            Self::VarRotate(transform) => {
                let deltas = instance.var_deltas::<1>(transform.var_index_base());
                ResolvedPaint::Rotate {
                    angle: transform.angle().apply_delta(deltas[0]),
                    around_center: None,
                    paint: transform.paint()?,
                }
            }
            Self::RotateAroundCenter(transform) => ResolvedPaint::Rotate {
                angle: transform.angle().to_fixed(),
                around_center: Some(Point::new(
                    transform.center_x().to_fixed(),
                    transform.center_y().to_fixed(),
                )),
                paint: transform.paint()?,
            },
            Self::VarRotateAroundCenter(transform) => {
                let deltas = instance.var_deltas::<3>(transform.var_index_base());
                ResolvedPaint::Rotate {
                    angle: transform.angle().apply_delta(deltas[0]),
                    around_center: Some(Point::new(
                        transform.center_x().apply_delta(deltas[1]),
                        transform.center_y().apply_delta(deltas[2]),
                    )),
                    paint: transform.paint()?,
                }
            }
            Self::Skew(transform) => ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().to_fixed(),
                y_skew_angle: transform.y_skew_angle().to_fixed(),
                around_center: None,
                paint: transform.paint()?,
            },
            Self::VarSkew(transform) => {
                let deltas = instance.var_deltas::<2>(transform.var_index_base());
                ResolvedPaint::Skew {
                    x_skew_angle: transform.x_skew_angle().apply_delta(deltas[0]),
                    y_skew_angle: transform.y_skew_angle().apply_delta(deltas[1]),
                    around_center: None,
                    paint: transform.paint()?,
                }
            }
            Self::SkewAroundCenter(transform) => ResolvedPaint::Skew {
                x_skew_angle: transform.x_skew_angle().to_fixed(),
                y_skew_angle: transform.y_skew_angle().to_fixed(),
                around_center: Some(Point::new(
                    transform.center_x().to_fixed(),
                    transform.center_y().to_fixed(),
                )),
                paint: transform.paint()?,
            },
            Self::VarSkewAroundCenter(transform) => {
                let deltas = instance.var_deltas::<4>(transform.var_index_base());
                ResolvedPaint::Skew {
                    x_skew_angle: transform.x_skew_angle().apply_delta(deltas[0]),
                    y_skew_angle: transform.y_skew_angle().apply_delta(deltas[1]),
                    around_center: Some(Point::new(
                        transform.center_x().apply_delta(deltas[2]),
                        transform.center_y().apply_delta(deltas[3]),
                    )),
                    paint: transform.paint()?,
                }
            }
            Self::Composite(composite) => ResolvedPaint::Composite {
                source_paint: composite.source_paint()?,
                mode: composite.composite_mode(),
                backdrop_paint: composite.backdrop_paint()?,
            },
        })
    }
}

/// Trait to augment all types in the `COLR` table with an appropriate
/// `apply_delta` method that both adds the delta and converts to 16.16
/// fixed point.
///
/// It might be worth moving this to `font-types` at some point, but
/// the conversion to `Fixed` may not be generally useful.
trait ApplyDelta {
    fn apply_delta(self, delta: i32) -> Fixed;
}

impl ApplyDelta for Fixed {
    fn apply_delta(self, delta: i32) -> Fixed {
        self + Fixed::from_bits(delta)
    }
}

impl ApplyDelta for F2Dot14 {
    fn apply_delta(self, delta: i32) -> Fixed {
        self.to_fixed() + F2Dot14::from_bits(delta as i16).to_fixed()
    }
}

impl ApplyDelta for FWord {
    fn apply_delta(self, delta: i32) -> Fixed {
        self.to_fixed() + Fixed::from_i32(delta)
    }
}

impl ApplyDelta for UfWord {
    fn apply_delta(self, delta: i32) -> Fixed {
        self.to_fixed() + Fixed::from_i32(delta)
    }
}
