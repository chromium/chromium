//! Outlines from variable composites in `VARC`.
//!
//! A variable composite glyph is a tree of components. Each component is a
//! transformed glyph drawn at its own location in the font's variation space,
//! and may itself be a composite. The leaves are glyphs from the font's base
//! outline source, `glyf` or CFF. They are drawn by the [`OutlineGlyphs`]
//! that produced the composite.
//!
//! The working memory a [`VarcGlyph`] needs depends on its nesting depth and
//! its largest leaf. The tree is walked to measure them the first time the
//! size is needed, and the result is kept. Drawing splits that memory into
//! typed slices, as the `glyf` loader does, so a composite needs no memory
//! beyond that buffer.

use super::{
    glyf::Precision, glyphs::OutlineGlyphs, DrawOptions, DrawnMetrics, PathSink, TransformSink,
};
use crate::model::Error;
use crate::{
    limits::{MAX_COMPOSITE_EDGES, MAX_RECURSION_DEPTH},
    mem::take_slice,
    tables::{
        varc::{
            Condition, CoverageTable, DecomposedTransform, MultiItemVariationStore,
            SparseVariationRegionList, Varc, VarcComponent, VarcFlags,
        },
        variations::NO_VARIATION_INDEX,
    },
    types::{F2Dot14, GlyphId, Matrix},
    ReadError,
};
use core::{
    mem::{align_of, size_of},
    sync::atomic::{AtomicU32, AtomicUsize, Ordering},
};
use yoke::Yokeable;

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

/// Provides the data that [`Outlines::draw`] needs from outside the table.
///
/// This includes the font's location and the glyphs at the leaves of a
/// composite.
pub(crate) trait VarcContext {
    /// Returns the normalized variation coordinates of the font's location.
    ///
    /// A component that resets unspecified axes uses these values.
    fn coords(&self) -> &[F2Dot14];

    /// Draws `glyph` into `sink` at the normalized coordinates `coords`.
    ///
    /// The `coords` may differ from [`Self::coords`]. `ppem` is the size of
    /// the composite, or `None` for font units. `buf` is working memory of
    /// at least [`Self::glyph_buffer_size`] bytes for the glyph.
    fn draw_glyph(
        &self,
        glyph: GlyphId,
        coords: &[F2Dot14],
        ppem: Option<f32>,
        buf: &mut [u8],
        sink: &mut dyn PathSink,
    ) -> Result<(), Error>;

    /// Returns the size in bytes of the working memory needed to draw
    /// `glyph`.
    fn glyph_buffer_size(&self, glyph: GlyphId) -> usize;
}

/// A glyph in the `VARC` table, with its working memory size computed.
/// A composite glyph, ready to draw.
///
/// The memory a draw needs depends on the whole tree, so the tree is
/// measured the first time that is asked for, not when the outline is made.
#[derive(Debug)]
pub(crate) struct Outline {
    glyph: GlyphId,
    coverage_index: u32,
    measured: Measured,
}

impl Outline {
    /// Returns the glyph identifier.
    pub(crate) fn glyph(&self) -> GlyphId {
        self.glyph
    }
}

impl Clone for Outline {
    fn clone(&self) -> Self {
        Self {
            glyph: self.glyph,
            coverage_index: self.coverage_index,
            measured: self.measured.clone(),
        }
    }
}

/// What measuring a composite's tree found, kept in atomics so an outline
/// stays cheap to clone and safe to share.
///
/// Two threads that measure at once store the same values.
#[derive(Debug, Default)]
struct Measured {
    /// [`Measured::UNMEASURED`] until the tree is measured. Then one of the
    /// other states in the low two bits, with the nesting depth above them.
    state: AtomicU32,
    /// The size in bytes of the largest leaf, once `state` says so.
    leaf: AtomicUsize,
}

impl Measured {
    const UNMEASURED: u32 = 0;
    const MEASURED: u32 = 1;
    const MALFORMED: u32 = 2;
    const OVER_LIMIT: u32 = 3;
    const STATE_BITS: u32 = 2;
}

impl Clone for Measured {
    fn clone(&self) -> Self {
        Self {
            state: AtomicU32::new(self.state.load(Ordering::Acquire)),
            leaf: AtomicUsize::new(self.leaf.load(Ordering::Relaxed)),
        }
    }
}

/// Provides access to the variable composite glyphs in a font.
#[derive(Clone, Yokeable)]
pub(crate) struct Outlines<'a> {
    varc: Varc<'a>,
    coverage: CoverageTable<'a>,
    var_store: Option<MultiItemVariationStore<'a>>,
    regions: Option<SparseVariationRegionList<'a>>,
    units_per_em: u16,
    axis_count: usize,
    /// The length of the longest axis index list in the table.
    ///
    /// A list longer than `axis_count` repeats an axis, and the later value
    /// wins.
    index_count: usize,
}

impl<'a> Outlines<'a> {
    /// Creates a new set of composite glyphs from `varc`.
    ///
    /// `axis_count` is the number of variation axes that components can set.
    /// A font without `fvar` can still have axes in `gvar` or the CFF2
    /// variation store.
    pub(crate) fn new(
        varc: Varc<'a>,
        units_per_em: u16,
        axis_count: usize,
    ) -> Result<Self, ReadError> {
        let coverage = varc.coverage()?;
        let var_store = varc.multi_var_store().transpose()?;
        let regions = var_store
            .as_ref()
            .map(|store| store.region_list())
            .transpose()?;
        let list_count = varc
            .axis_indices_list()
            .transpose()?
            .map_or(0, |lists| lists.count() as usize);
        let index_count = (0..list_count)
            .filter_map(|nth| varc.axis_indices(nth).ok())
            .map(|list| list.count_or_compute())
            .max()
            .unwrap_or(0);
        Ok(Self {
            varc,
            coverage,
            var_store,
            regions,
            units_per_em,
            axis_count,
            index_count,
        })
    }

    /// Returns true if the table covers `glyph`.
    pub(crate) fn covers(&self, glyph: GlyphId) -> bool {
        self.coverage.get(glyph).is_some()
    }

    /// Returns the outline for `glyph`.
    ///
    /// Returns `None` if the table does not cover the glyph. The tree is read
    /// only when the glyph is drawn or its memory size is requested.
    pub(crate) fn outline(&self, glyph: GlyphId) -> Option<Outline> {
        let coverage_index = self.coverage.get(glyph)?;
        Some(Outline {
            glyph,
            coverage_index,
            measured: Measured::default(),
        })
    }

    /// Returns the memory layout for drawing `outline`, measuring its tree
    /// the first time.
    ///
    /// Fails if the glyph data is malformed or the tree passes the recursion
    /// or edge limits. Every component counts, whatever its condition, so the
    /// answer is the same at every location.
    fn layout_of(&self, outline: &Outline, context: &dyn VarcContext) -> Result<Layout, Error> {
        let measured = &outline.measured;
        let mut state = measured.state.load(Ordering::Acquire);
        if state == Measured::UNMEASURED {
            let mut walk = Walk::new();
            let result = self.measure(outline.glyph, outline.coverage_index, context, &mut walk);
            let (code, leaf) = match result {
                Err(_) => (Measured::MALFORMED, 0),
                Ok(_) if walk.truncated => (Measured::OVER_LIMIT, 0),
                Ok(leaf) => (Measured::MEASURED, leaf),
            };
            state = code | (walk.depth as u32) << Measured::STATE_BITS;
            measured.leaf.store(leaf, Ordering::Relaxed);
            measured.state.store(state, Ordering::Release);
        }
        match state & ((1 << Measured::STATE_BITS) - 1) {
            Measured::MEASURED => {
                let depth = (state >> Measured::STATE_BITS) as usize;
                Ok(self.layout(depth, measured.leaf.load(Ordering::Relaxed)))
            }
            Measured::OVER_LIMIT => Err(Error::LimitExceeded),
            _ => Err(Error::Malformed),
        }
    }

    /// Returns the size in bytes of the memory for drawing `outline`, or zero
    /// if it cannot be drawn.
    pub(crate) fn required_memory_size(
        &self,
        outline: &Outline,
        context: &dyn VarcContext,
    ) -> usize {
        self.layout_of(outline, context)
            .ok()
            .and_then(|layout| layout.packed_len())
            .unwrap_or(0)
    }

    /// Returns the memory layout for a composite.
    ///
    /// `depth` is the nesting depth and `leaf` is the size in bytes of the
    /// largest leaf.
    fn layout(&self, depth: usize, leaf: usize) -> Layout {
        Layout {
            depth: depth.max(1),
            axis_count: self.axis_count,
            index_count: self.index_count,
            region_count: self.region_count(),
            leaf,
        }
    }

    /// Returns the number of variation regions, or zero if the table has no
    /// variation store.
    fn region_count(&self) -> usize {
        self.regions
            .as_ref()
            .map_or(0, |regions| regions.region_count() as usize)
    }

    /// Measures the tree under a composite.
    ///
    /// Returns the size in bytes of the largest leaf and records the deepest
    /// nesting in `walk`. Every component is counted regardless of its
    /// condition, so the result is valid for any location.
    fn measure(
        &self,
        glyph: GlyphId,
        coverage_index: u32,
        context: &dyn VarcContext,
        walk: &mut Walk,
    ) -> Result<usize, ReadError> {
        // The caller filters out a glyph already on the stack, so this one
        // starts no cycle. The limits end the walk, and the outline is then
        // refused.
        if walk.stack.len() >= MAX_RECURSION_DEPTH || walk.edges_left == 0 {
            walk.truncated = true;
            return Ok(0);
        }
        walk.edges_left -= 1;
        walk.stack.push(glyph);
        walk.depth = walk.depth.max(walk.stack.len());
        let mut leaf = 0usize;
        let record = self.varc.glyph(coverage_index as usize)?;
        for component in record.components() {
            let component_gid = component?.gid();
            // A covered component nests, unless it is the glyph itself,
            // which drawing takes from the base, or one already being drawn,
            // which closes a cycle and draws nothing.
            let covered = self
                .coverage
                .get(component_gid)
                .filter(|_| component_gid != glyph);
            let component_leaf = match covered {
                Some(coverage_index) if !walk.stack.contains(component_gid) => {
                    self.measure(component_gid, coverage_index, context, walk)?
                }
                // A leaf or a cycle is an edge of the tree too.
                _ if walk.edges_left == 0 => {
                    walk.truncated = true;
                    break;
                }
                Some(_) => {
                    walk.edges_left -= 1;
                    0
                }
                None => {
                    walk.edges_left -= 1;
                    context.glyph_buffer_size(component_gid)
                }
            };
            leaf = leaf.max(component_leaf);
        }
        walk.stack.pop();
        Ok(leaf)
    }

    /// Draws `outline` into `sink` at the location provided by `context`.
    ///
    /// The glyphs at the leaves are drawn by `context`. `ppem` is the size of
    /// the leaves, or `None` for font units. It also scales the component
    /// offsets. `buf` must hold at least
    /// [`required_memory_size`](Self::required_memory_size) bytes.
    pub(crate) fn draw(
        &self,
        outline: &Outline,
        context: &dyn VarcContext,
        ppem: Option<f32>,
        buf: &mut [u8],
        sink: &mut dyn PathSink,
    ) -> Result<(), Error> {
        let layout = self.layout_of(outline, context)?;
        let memory = layout.carve(buf).ok_or(Error::InsufficientMemory)?;
        self.draw_tree(outline, &layout, context, ppem, memory, sink)
    }

    /// Implements [`draw`](Self::draw) with memory that is already split
    /// into slices.
    fn draw_tree(
        &self,
        outline: &Outline,
        layout: &Layout,
        context: &dyn VarcContext,
        ppem: Option<f32>,
        memory: Memory<'_>,
        sink: &mut dyn PathSink,
    ) -> Result<(), Error> {
        let Memory {
            font_coords,
            coords,
            scalars,
            leaf,
            mut scratch,
        } = memory;
        expand_coords(font_coords, context.coords());
        let (root_scalars, level_scalars) =
            split(scalars, layout.region_count).ok_or(Error::InsufficientMemory)?;
        let mut scalar_cache = ScalarCache::new(root_scalars);
        let levels = Levels {
            coords,
            scalars: level_scalars,
            axis_count: layout.axis_count,
            region_count: layout.region_count,
        };
        let draw = DrawContext {
            font_coords,
            scale: linear_scale(ppem, self.units_per_em),
            ppem,
            context,
            store_regions: self.var_store.as_ref().zip(self.regions.as_ref()),
        };
        let mut stack = GlyphStack::new();
        self.draw_glyph(
            outline.glyph,
            outline.coverage_index,
            draw.font_coords,
            Matrix::IDENTITY,
            &draw,
            &mut scalar_cache,
            levels,
            leaf,
            sink,
            &mut stack,
            &mut scratch,
        )
    }

    #[allow(clippy::too_many_arguments)]
    fn draw_glyph(
        &self,
        glyph: GlyphId,
        coverage_index: u32,
        current_coords: &[F2Dot14],
        parent_matrix: Matrix<f32>,
        draw: &DrawContext<'a, '_>,
        scalar_cache: &mut ScalarCache<'_>,
        mut levels: Levels<'_>,
        leaf: &mut [u8],
        sink: &mut dyn PathSink,
        stack: &mut GlyphStack,
        scratch: &mut Scratch<'_>,
    ) -> Result<(), Error> {
        // Past a limit the whole glyph fails rather than leaving out what
        // lies beyond it. The memory is sized by a walk that stops at the
        // same limits, so nothing past them has room to draw.
        if stack.len() >= MAX_RECURSION_DEPTH || scratch.edges_left == 0 {
            return Err(Error::LimitExceeded);
        }
        scratch.edges_left -= 1;
        let record = self.varc.glyph(coverage_index as usize)?;
        // This level's run of coordinates and scalars, for components drawn
        // at another location.
        let mut level = levels.next().ok_or(Error::InsufficientMemory)?;
        stack.push(glyph);
        let store_regions = draw.store_regions;
        for component in record.components() {
            let component = component?;
            if !self.component_condition_met(
                &component,
                current_coords,
                scalar_cache,
                scratch,
                store_regions,
            )? {
                continue;
            }
            let component_gid = component.gid();
            let flags = component.flags();

            let coords_the_same = !flags.contains(VarcFlags::HAVE_AXES)
                && !flags.contains(VarcFlags::RESET_UNSPECIFIED_AXES);

            let component_coords: &[F2Dot14] = if coords_the_same {
                current_coords
            } else {
                self.component_coords(
                    &component,
                    current_coords,
                    level.coords,
                    scalar_cache,
                    scratch,
                    draw.font_coords,
                    store_regions,
                )?;
                level.coords
            };

            let mut transform = *component.transform();
            apply_transform_variations(
                &component,
                current_coords,
                &mut transform,
                scalar_cache,
                scratch,
                store_regions,
            )?;
            let matrix = parent_matrix * scale_matrix(transform.matrix(), draw.scale);
            if component_gid != glyph {
                if let Some(coverage_index) = self.coverage.get(component_gid) {
                    if !stack.contains(component_gid) {
                        if coords_the_same {
                            // The scalars are for these coordinates, so the
                            // child shares them.
                            self.draw_glyph(
                                component_gid,
                                coverage_index,
                                current_coords,
                                matrix,
                                draw,
                                scalar_cache,
                                level.rest.reborrow(),
                                leaf,
                                sink,
                                stack,
                                scratch,
                            )?;
                        } else {
                            let mut child_cache = ScalarCache::new(level.scalars);
                            self.draw_glyph(
                                component_gid,
                                coverage_index,
                                component_coords,
                                matrix,
                                draw,
                                &mut child_cache,
                                level.rest.reborrow(),
                                leaf,
                                sink,
                                stack,
                                scratch,
                            )?;
                        }
                        continue;
                    }
                    // The component leads back to a glyph being drawn. The
                    // cycle draws nothing, but it is an edge of the tree.
                    if scratch.edges_left == 0 {
                        return Err(Error::LimitExceeded);
                    }
                    scratch.edges_left -= 1;
                    continue;
                }
            }
            // A leaf is an edge of the tree too.
            if scratch.edges_left == 0 {
                return Err(Error::LimitExceeded);
            }
            scratch.edges_left -= 1;
            draw.context.draw_glyph(
                component_gid,
                component_coords,
                draw.ppem,
                leaf,
                &mut TransformSink::new(&mut *sink, matrix),
            )?;
        }
        stack.pop();
        Ok(())
    }

    /// Writes the coordinates for drawing `component` to `coords`.
    ///
    /// `coords` holds one value per axis.
    #[allow(clippy::too_many_arguments)]
    fn component_coords(
        &self,
        component: &VarcComponent<'_>,
        current_coords: &[F2Dot14],
        coords: &mut [F2Dot14],
        scalar_cache: &mut ScalarCache<'_>,
        scratch: &mut Scratch<'_>,
        font_coords: &[F2Dot14],
        store_regions: Option<(&MultiItemVariationStore<'a>, &SparseVariationRegionList<'a>)>,
    ) -> Result<(), Error> {
        let flags = component.flags();
        if flags.contains(VarcFlags::RESET_UNSPECIFIED_AXES) {
            expand_coords(coords, font_coords);
        } else {
            expand_coords(coords, current_coords);
        }

        if !flags.contains(VarcFlags::HAVE_AXES) {
            return Ok(());
        }

        let axis_indices_index = component.axis_indices_index().ok_or(Error::Malformed)?;
        let num_axes = self.axis_indices(axis_indices_index as usize, scratch.axis_indices)?;
        let axis_indices = &scratch.axis_indices[..num_axes];
        let values = scratch
            .axis_values
            .get_mut(..num_axes)
            .ok_or(Error::Malformed)?;
        let stated = axis_values(component, values);
        let values = &mut values[..stated];
        if let Some(var_idx) = component.axis_values_var_index() {
            if var_idx != NO_VARIATION_INDEX && !values.is_empty() {
                let (store, regions) = store_regions.ok_or(Error::Malformed)?;
                let deltas = scratch
                    .deltas
                    .get_mut(..values.len())
                    .ok_or(Error::Malformed)?;
                compute_tuple_deltas(
                    store,
                    regions,
                    var_idx,
                    current_coords,
                    scalar_cache,
                    deltas,
                )?;
                for (value, delta) in values.iter_mut().zip(deltas.iter()) {
                    *value += *delta;
                }
            }
        }

        for (axis_index, value) in axis_indices.iter().zip(values.iter().copied()) {
            let Some(slot) = coords.get_mut(*axis_index as usize) else {
                return Err(Error::Malformed);
            };
            let raw = value.round().clamp(i16::MIN as f64, i16::MAX as f64) as i16;
            *slot = F2Dot14::from_bits(raw);
        }
        Ok(())
    }

    /// Reads the `nth` list of axis indices into `out` and returns the
    /// number of indices.
    fn axis_indices(&self, nth: usize, out: &mut [u16]) -> Result<usize, ReadError> {
        let packed = self.varc.axis_indices(nth)?;
        let mut count = 0;
        for value in packed.iter() {
            // `out` holds the longest list in the table.
            let slot = out.get_mut(count).ok_or(ReadError::OutOfBounds)?;
            *slot = u16::try_from(value).map_err(|_| ReadError::OutOfBounds)?;
            count += 1;
        }
        Ok(count)
    }

    fn component_condition_met(
        &self,
        component: &VarcComponent<'_>,
        coords: &[F2Dot14],
        scalar_cache: &mut ScalarCache<'_>,
        scratch: &mut Scratch<'_>,
        store_regions: Option<(&MultiItemVariationStore<'a>, &SparseVariationRegionList<'a>)>,
    ) -> Result<bool, Error> {
        let Some(condition_index) = component.condition_index() else {
            return Ok(true);
        };
        let condition_list = self.varc.condition_list().ok_or(Error::Malformed)??;
        let offset = condition_list
            .condition_offsets()
            .get(condition_index as usize)
            .ok_or(Error::Malformed)?;
        // A null condition is unconditional; it is not a format-0 table.
        if offset.get().is_null() {
            return Ok(true);
        }
        let condition = condition_list.conditions().get(condition_index as usize)?;
        eval_condition(&condition, coords, store_regions, scalar_cache, scratch)
    }
}

/// Data shared by every glyph in one draw.
struct DrawContext<'a, 'b> {
    font_coords: &'b [F2Dot14],
    /// The factor that scales component offsets to the size of the leaves.
    scale: f32,
    ppem: Option<f32>,
    context: &'b dyn VarcContext,
    store_regions: Option<(
        &'b MultiItemVariationStore<'a>,
        &'b SparseVariationRegionList<'a>,
    )>,
}

/// The maximum number of fields that one transform can vary.
///
/// Together with the axis count, this bounds the length of a delta tuple.
const TRANSFORM_FIELD_COUNT: usize = 9;

/// The size of a composite's working memory and how it is split.
///
/// A single buffer from the caller holds every slice that the walk uses.
/// The slices are taken in descending order of alignment so that only the
/// first one can need padding. The memory for drawing leaves comes last.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Layout {
    /// The nesting depth of the composites, at least one.
    depth: usize,
    axis_count: usize,
    /// The length of the longest axis index list, which sizes the axis
    /// indices, their values and their deltas.
    index_count: usize,
    region_count: usize,
    /// The size in bytes of the memory needed by the largest leaf.
    leaf: usize,
}

impl Layout {
    /// Returns the length of the longest delta tuple a component reads.
    fn tuple_len(&self) -> usize {
        self.index_count.max(TRANSFORM_FIELD_COUNT)
    }

    /// Returns the size in bytes of the buffer needed for
    /// [`carve`](Self::carve) to succeed.
    ///
    /// Returns `None` if the table has more regions than memory can hold.
    fn packed_len(&self) -> Option<usize> {
        let doubles = self.tuple_len().checked_add(self.index_count)?;
        let scalars = (self.depth + 1).checked_mul(self.region_count)?;
        // The font's coordinates, one run per level, and the axis indices.
        let halves = (self.depth + 1)
            .checked_mul(self.axis_count)?
            .checked_add(self.index_count)?;
        doubles
            .checked_mul(size_of::<f64>())?
            .checked_add(scalars.checked_mul(size_of::<f32>())?)?
            .checked_add(halves.checked_mul(size_of::<u16>())?)?
            .checked_add(self.leaf)?
            .checked_add(align_of::<f64>())
    }

    /// Splits `buf` into the slices used by a draw, or returns `None` if it
    /// is too short.
    fn carve<'m>(&self, buf: &'m mut [u8]) -> Option<Memory<'m>> {
        let (deltas, rest) = take_slice::<f64>(buf, self.tuple_len())?;
        let (axis_values, rest) = take_slice::<f64>(rest, self.index_count)?;
        let (scalars, rest) = take_slice::<f32>(rest, (self.depth + 1) * self.region_count)?;
        let (font_coords, rest) = take_slice::<F2Dot14>(rest, self.axis_count)?;
        let (coords, rest) = take_slice::<F2Dot14>(rest, self.depth * self.axis_count)?;
        let (axis_indices, leaf) = take_slice::<u16>(rest, self.index_count)?;
        (leaf.len() >= self.leaf).then_some(Memory {
            font_coords,
            coords,
            scalars,
            leaf,
            scratch: Scratch {
                deltas,
                axis_values,
                axis_indices,
                edges_left: MAX_COMPOSITE_EDGES,
            },
        })
    }
}

/// The working memory for one draw, split by a [`Layout`].
struct Memory<'m> {
    /// The font's coordinates, one per axis.
    font_coords: &'m mut [F2Dot14],
    /// One run of coordinates per level of nesting.
    coords: &'m mut [F2Dot14],
    /// One run of region scalars for the root and one per level.
    scalars: &'m mut [f32],
    /// The working memory for drawing leaves.
    leaf: &'m mut [u8],
    scratch: Scratch<'m>,
}

/// Slices that each level uses in turn, and the number of edges left to
/// walk.
struct Scratch<'m> {
    deltas: &'m mut [f64],
    axis_values: &'m mut [f64],
    axis_indices: &'m mut [u16],
    edges_left: usize,
}

/// The unused runs of coordinates and scalars.
///
/// There is one of each per level of nesting below the current one.
struct Levels<'m> {
    coords: &'m mut [F2Dot14],
    scalars: &'m mut [f32],
    axis_count: usize,
    region_count: usize,
}

/// The runs for one level, with the remaining runs for the levels below it.
struct Level<'m> {
    coords: &'m mut [F2Dot14],
    scalars: &'m mut [f32],
    rest: Levels<'m>,
}

impl Levels<'_> {
    /// Reborrows the runs for one child.
    fn reborrow(&mut self) -> Levels<'_> {
        Levels {
            coords: &mut *self.coords,
            scalars: &mut *self.scalars,
            axis_count: self.axis_count,
            region_count: self.region_count,
        }
    }

    /// Takes the runs for one level, or returns `None` if none are left.
    fn next(&mut self) -> Option<Level<'_>> {
        let (coords, rest_coords) = split(&mut *self.coords, self.axis_count)?;
        let (scalars, rest_scalars) = split(&mut *self.scalars, self.region_count)?;
        Some(Level {
            coords,
            scalars,
            rest: Levels {
                coords: rest_coords,
                scalars: rest_scalars,
                axis_count: self.axis_count,
                region_count: self.region_count,
            },
        })
    }
}

/// Splits `len` elements off the front of `slice`, or `None` if it is too
/// short.
fn split<T>(slice: &mut [T], len: usize) -> Option<(&mut [T], &mut [T])> {
    (len <= slice.len()).then(|| slice.split_at_mut(len))
}

/// The composites being walked, outermost first, used to detect cycles.
struct GlyphStack {
    ids: [GlyphId; MAX_RECURSION_DEPTH],
    len: usize,
}

impl GlyphStack {
    fn new() -> Self {
        Self {
            ids: [GlyphId::new(0); MAX_RECURSION_DEPTH],
            len: 0,
        }
    }

    fn len(&self) -> usize {
        self.len
    }

    fn contains(&self, glyph: GlyphId) -> bool {
        self.ids[..self.len].contains(&glyph)
    }

    /// Pushes `glyph`.
    ///
    /// Callers keep the depth below the limit, so there is always room.
    fn push(&mut self, glyph: GlyphId) {
        if let Some(slot) = self.ids.get_mut(self.len) {
            *slot = glyph;
            self.len += 1;
        }
    }

    fn pop(&mut self) {
        self.len = self.len.saturating_sub(1);
    }
}

/// The state of a sizing walk.
struct Walk {
    stack: GlyphStack,
    edges_left: usize,
    /// The deepest nesting seen.
    depth: usize,
    /// True if a limit stopped the walk early.
    truncated: bool,
}

impl Walk {
    fn new() -> Self {
        Self {
            stack: GlyphStack::new(),
            edges_left: MAX_COMPOSITE_EDGES,
            depth: 0,
            truncated: false,
        }
    }
}

/// The scalar for each region at one location, computed on first use.
struct ScalarCache<'m> {
    values: &'m mut [f32],
}

impl<'m> ScalarCache<'m> {
    /// Marks a scalar that has not been computed.
    ///
    /// Scalars are in `[0, 1]`, so this value is never valid.
    const INVALID: f32 = 2.0;

    fn new(values: &'m mut [f32]) -> Self {
        values.fill(Self::INVALID);
        Self { values }
    }

    fn get(&self, index: usize) -> f32 {
        self.values.get(index).copied().unwrap_or(Self::INVALID)
    }

    fn set(&mut self, index: usize, value: f32) {
        if let Some(slot) = self.values.get_mut(index) {
            *slot = value;
        }
    }
}

/// Writes the axis values of `component` to `out`, one per axis index.
///
/// Returns the number of slots written. If the component has axis values,
/// every slot is written and any missing values are set to zero. If it has
/// none, this returns zero.
fn axis_values(component: &VarcComponent<'_>, out: &mut [f64]) -> usize {
    let Some(packed) = component.axis_values() else {
        return 0;
    };
    out.fill(0.0);
    for (slot, value) in out.iter_mut().zip(packed.iter()) {
        *slot = value as f64;
    }
    out.len()
}

fn apply_transform_variations(
    component: &VarcComponent<'_>,
    coords: &[F2Dot14],
    transform: &mut DecomposedTransform,
    scalar_cache: &mut ScalarCache<'_>,
    scratch: &mut Scratch<'_>,
    store_regions: Option<(&MultiItemVariationStore<'_>, &SparseVariationRegionList<'_>)>,
) -> Result<(), Error> {
    let Some(var_idx) = component.transform_var_index() else {
        return Ok(());
    };

    let flags = component.flags();

    const TRANSFORM_MASK: VarcFlags = VarcFlags::from_bits_truncate(
        VarcFlags::HAVE_TRANSLATE_X.bits()
            | VarcFlags::HAVE_TRANSLATE_Y.bits()
            | VarcFlags::HAVE_ROTATION.bits()
            | VarcFlags::HAVE_SCALE_X.bits()
            | VarcFlags::HAVE_SCALE_Y.bits()
            | VarcFlags::HAVE_SKEW_X.bits()
            | VarcFlags::HAVE_SKEW_Y.bits()
            | VarcFlags::HAVE_TCENTER_X.bits()
            | VarcFlags::HAVE_TCENTER_Y.bits(),
    );
    let field_count = (flags.bits() & TRANSFORM_MASK.bits()).count_ones() as usize;
    if field_count == 0 || var_idx == NO_VARIATION_INDEX {
        return Ok(());
    }

    let (store, regions) = store_regions.ok_or(Error::Malformed)?;
    let deltas = scratch
        .deltas
        .get_mut(..field_count)
        .ok_or(Error::Malformed)?;
    compute_tuple_deltas(store, regions, var_idx, coords, scalar_cache, deltas)?;

    let mut delta_iter = deltas.iter().map(|&delta| delta as f32);

    if flags.contains(VarcFlags::HAVE_TRANSLATE_X) {
        let delta = delta_iter.next().unwrap_or(0.0);
        transform.set_translate_x(transform.translate_x() + delta);
    }
    if flags.contains(VarcFlags::HAVE_TRANSLATE_Y) {
        let delta = delta_iter.next().unwrap_or(0.0);
        transform.set_translate_y(transform.translate_y() + delta);
    }
    if flags.contains(VarcFlags::HAVE_ROTATION) {
        let delta = delta_iter.next().unwrap_or(0.0);
        transform.set_rotation(transform.rotation() + delta / 4096.0);
    }
    if flags.contains(VarcFlags::HAVE_SCALE_X) {
        let delta = delta_iter.next().unwrap_or(0.0);
        transform.set_scale_x(transform.scale_x() + delta / 1024.0);
    }
    if flags.contains(VarcFlags::HAVE_SCALE_Y) {
        let delta = delta_iter.next().unwrap_or(0.0);
        transform.set_scale_y(transform.scale_y() + delta / 1024.0);
    }
    const SKEW_OR_CENTER: VarcFlags = VarcFlags::from_bits_truncate(
        VarcFlags::HAVE_SKEW_X.bits()
            | VarcFlags::HAVE_SKEW_Y.bits()
            | VarcFlags::HAVE_TCENTER_X.bits()
            | VarcFlags::HAVE_TCENTER_Y.bits(),
    );
    if flags.intersects(SKEW_OR_CENTER) {
        if flags.contains(VarcFlags::HAVE_SKEW_X) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_skew_x(transform.skew_x() + delta / 4096.0);
        }
        if flags.contains(VarcFlags::HAVE_SKEW_Y) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_skew_y(transform.skew_y() + delta / 4096.0);
        }
        if flags.contains(VarcFlags::HAVE_TCENTER_X) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_center_x(transform.center_x() + delta);
        }
        if flags.contains(VarcFlags::HAVE_TCENTER_Y) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_center_y(transform.center_y() + delta);
        }
    }

    if !flags.contains(VarcFlags::HAVE_SCALE_Y) {
        transform.set_scale_y(transform.scale_x());
    }
    Ok(())
}

fn eval_condition(
    condition: &Condition<'_>,
    coords: &[F2Dot14],
    store_regions: Option<(&MultiItemVariationStore<'_>, &SparseVariationRegionList<'_>)>,
    scalar_cache: &mut ScalarCache<'_>,
    scratch: &mut Scratch<'_>,
) -> Result<bool, Error> {
    condition.evaluate(coords, |var_idx| {
        let (var_store, regions) = store_regions.ok_or(Error::Malformed)?;
        let delta = scratch.deltas.get_mut(..1).ok_or(Error::Malformed)?;
        compute_tuple_deltas(var_store, regions, var_idx, coords, scalar_cache, delta)?;
        Ok(delta[0])
    })
}

/// Returns the factor that scales font units to `ppem`.
fn linear_scale(ppem: Option<f32>, units_per_em: u16) -> f32 {
    match ppem {
        Some(ppem) if units_per_em != 0 => ppem / units_per_em as f32,
        _ => 1.0,
    }
}

/// Copies `coords` into `out`, which holds one value per axis.
///
/// Axes missing from `coords` are set to their default.
fn expand_coords(out: &mut [F2Dot14], coords: &[F2Dot14]) {
    out.fill(F2Dot14::ZERO);
    for (slot, value) in out.iter_mut().zip(coords.iter().copied()) {
        *slot = value;
    }
}

/// Sums the deltas at `coords` for the tuple identified by `var_idx` into
/// `out`.
///
/// The length of `out` is the length of the tuple.
fn compute_tuple_deltas(
    store: &MultiItemVariationStore,
    regions: &SparseVariationRegionList,
    var_idx: u32,
    coords: &[F2Dot14],
    cache: &mut ScalarCache<'_>,
    out: &mut [f64],
) -> Result<(), ReadError> {
    // The slice is reused across calls, so it starts at zero rather than at
    // the previous call's values.
    out.fill(0.0);
    if out.is_empty() || var_idx == NO_VARIATION_INDEX {
        return Ok(());
    }
    let outer = (var_idx >> 16) as usize;
    let inner = (var_idx & 0xFFFF) as usize;
    let data = store.variation_data().get(outer)?;
    let region_indices = data.region_indices();
    let mut deltas = data.delta_set(inner)?.fetcher();
    let regions = regions.regions();

    let mut skip = 0;
    for region_index in region_indices.iter() {
        let region_idx = region_index.get() as usize;
        let mut scalar = cache.get(region_idx);
        if scalar >= 2.0 {
            scalar = regions.get(region_idx)?.compute_scalar_f32(coords);
            cache.set(region_idx, scalar);
        }
        // Regions with a zero scalar are skipped in one seek.
        if scalar == 0.0 {
            skip += out.len();
        } else {
            if skip != 0 {
                deltas.skip(skip)?;
                skip = 0;
            }
            deltas.add_to_f64_scaled(out, scalar as f64)?;
        }
    }

    Ok(())
}

#[inline(always)]
fn scale_matrix(mut m: Matrix<f32>, s: f32) -> Matrix<f32> {
    m.dx *= s;
    m.dy *= s;
    m
}

/// A variable composite glyph from a [`Font`](crate::model::Font).
///
/// This includes the base outlines used to draw its components.
#[derive(Clone)]
pub struct VarcGlyph<'a> {
    pub(super) outlines: &'a Outlines<'a>,
    pub(super) outline: Outline,
    /// Draws the leaves from the base source selected by the source order.
    pub(super) base: OutlineGlyphs<'a>,
}

impl<'a> VarcGlyph<'a> {
    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        self.outline.glyph()
    }

    /// Returns the size in bytes of the working memory needed for drawing.
    ///
    /// The size depends on the nesting depth, the largest leaf, and the
    /// table's axes and regions. It is computed the first time it is needed
    /// and kept. Returns zero for a glyph that cannot be drawn, which a draw
    /// reports as an error.
    pub fn required_memory_size(&self) -> usize {
        let leaves = Leaves::new(self.base, DrawOptions::default(), Precision::Fixed);
        self.outlines.required_memory_size(&self.outline, &leaves)
    }

    pub(super) fn draw(
        &self,
        options: &DrawOptions,
        precision: Precision,
        buf: &mut [u8],
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        let leaves = Leaves::new(self.base, *options, precision);
        self.outlines
            .draw(&self.outline, &leaves, options.ppem, buf, sink)?;
        Ok(DrawnMetrics::default())
    }
}

/// Draws the leaves of a composite from the base source.
///
/// The leaves use the draw options of the composite.
pub(super) struct Leaves<'a> {
    base: OutlineGlyphs<'a>,
    options: DrawOptions,
    precision: Precision,
}

impl<'a> Leaves<'a> {
    pub(super) fn new(base: OutlineGlyphs<'a>, options: DrawOptions, precision: Precision) -> Self {
        Self {
            base,
            options,
            precision,
        }
    }
}

impl VarcContext for Leaves<'_> {
    fn coords(&self) -> &[F2Dot14] {
        self.base.font().normalized_coords()
    }

    fn draw_glyph(
        &self,
        glyph: GlyphId,
        coords: &[F2Dot14],
        ppem: Option<f32>,
        buf: &mut [u8],
        sink: &mut dyn PathSink,
    ) -> Result<(), Error> {
        let options = DrawOptions {
            ppem,
            ..self.options
        };
        self.base
            .draw_base(glyph, coords, &options, self.precision, buf, sink)
    }

    fn glyph_buffer_size(&self, glyph: GlyphId) -> usize {
        self.base.base_buffer_size(glyph)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{model::glyph::outline::PathElement, FontRef, TableProvider};

    fn coord(value: f32) -> F2Dot14 {
        F2Dot14::from_f32(value)
    }

    fn test_outlines<'a>(font: &FontRef<'a>) -> Outlines<'a> {
        let axis_count = font
            .fvar()
            .map(|fvar| fvar.axis_count() as usize)
            .unwrap_or_default();
        let upem = font.head().unwrap().units_per_em();
        Outlines::new(font.varc().unwrap(), upem, axis_count).unwrap()
    }

    /// Returns working memory for the helpers.
    ///
    /// The size is computed for `outlines` with no leaves and at least eight
    /// axes, regardless of the fixture.
    fn test_memory(outlines: &Outlines<'_>) -> alloc::vec::Vec<u8> {
        alloc::vec![0u8; test_layout(outlines).packed_len().unwrap()]
    }

    fn test_layout(outlines: &Outlines<'_>) -> Layout {
        Layout {
            depth: 1,
            axis_count: outlines.axis_count.max(8),
            index_count: outlines.index_count.max(8),
            region_count: outlines.region_count(),
            leaf: 0,
        }
    }

    /// A context that draws nothing and needs no memory.
    struct NullContext;

    impl VarcContext for NullContext {
        fn coords(&self) -> &[F2Dot14] {
            &[]
        }

        fn draw_glyph(
            &self,
            _glyph: GlyphId,
            _coords: &[F2Dot14],
            _ppem: Option<f32>,
            _buf: &mut [u8],
            _pen: &mut dyn PathSink,
        ) -> Result<(), Error> {
            Ok(())
        }

        fn glyph_buffer_size(&self, _glyph: GlyphId) -> usize {
            0
        }
    }

    fn assert_close(actual: f32, expected: f32) {
        let diff = (actual - expected).abs();
        assert!(
            diff <= 1e-6,
            "expected {expected}, got {actual}, diff {diff}"
        );
    }

    #[test]
    fn expand_coords_pads_and_truncates() {
        let mut out = [F2Dot14::ZERO; 4];
        expand_coords(&mut out, &[coord(0.25), coord(-0.5)]);
        assert_eq!(
            out,
            [coord(0.25), coord(-0.5), F2Dot14::ZERO, F2Dot14::ZERO]
        );

        let mut out = [F2Dot14::ZERO; 1];
        expand_coords(&mut out, &[coord(0.25), coord(-0.5)]);
        assert_eq!(out, [coord(0.25)]);
    }

    #[test]
    fn scale_matrix_only_scales_translation() {
        let matrix = Matrix::from_elements([1.0, 2.0, 3.0, 4.0, 5.0, -6.0]);
        assert_eq!(
            scale_matrix(matrix, 10.0).elements(),
            [1.0, 2.0, 3.0, 4.0, 50.0, -60.0]
        );
    }

    #[test]
    fn compute_tuple_deltas_no_variation_index_zeroes_the_tuple() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let mut values = alloc::vec![0.0f32; regions.region_count() as usize];
        let mut cache = ScalarCache::new(&mut values);
        let mut out = [42.0f64; 3];

        compute_tuple_deltas(
            &store,
            &regions,
            NO_VARIATION_INDEX,
            &[coord(0.0)],
            &mut cache,
            &mut out,
        )
        .unwrap();
        assert_eq!(out, [0.0, 0.0, 0.0]);

        compute_tuple_deltas(
            &store,
            &regions,
            NO_VARIATION_INDEX,
            &[coord(0.0)],
            &mut cache,
            &mut [],
        )
        .unwrap();
    }

    #[test]
    fn compute_tuple_deltas_invalid_outer_index_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let mut values = alloc::vec![0.0f32; regions.region_count() as usize];
        let mut cache = ScalarCache::new(&mut values);
        let mut out = [0.0f64; 1];

        // an outer index past the end of the variation data is refused,
        // rather than silently contributing zero deltas
        assert!(
            compute_tuple_deltas(&store, &regions, 0xFFFF_0000, &[], &mut cache, &mut out).is_err()
        );
    }

    #[test]
    fn compute_tuple_deltas_matches_manual_decode() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let region_list = regions.regions();
        let coords = [coord(0.5); 8];

        let mut tried = 0usize;
        for (outer, data) in store.variation_data().iter().enumerate() {
            let data = data.unwrap();
            let region_count = data.region_indices().len();
            if region_count == 0 {
                continue;
            }
            let delta_set_count = data.delta_sets().unwrap().count() as usize;
            for inner in 0..delta_set_count.min(3) {
                let decoded = data
                    .delta_set(inner)
                    .unwrap()
                    .iter()
                    .collect::<alloc::vec::Vec<_>>();
                if decoded.is_empty() || decoded.len() % region_count != 0 {
                    continue;
                }
                let tuple_len = decoded.len() / region_count;
                let var_idx = ((outer as u32) << 16) | inner as u32;

                let mut values = alloc::vec![0.0f32; regions.region_count() as usize];
                let mut cache = ScalarCache::new(&mut values);
                let mut actual = alloc::vec![0.0f64; tuple_len];
                compute_tuple_deltas(&store, &regions, var_idx, &coords, &mut cache, &mut actual)
                    .unwrap();
                let first = actual.clone();

                // Same cache after population should not alter results.
                compute_tuple_deltas(&store, &regions, var_idx, &coords, &mut cache, &mut actual)
                    .unwrap();
                assert_eq!(actual, first);

                let mut expected = alloc::vec![0.0f64; tuple_len];
                for (region_order, region_idx) in data.region_indices().iter().enumerate() {
                    let scalar = region_list
                        .get(region_idx.get() as usize)
                        .unwrap()
                        .compute_scalar_f32(&coords);
                    if scalar == 0.0 {
                        continue;
                    }
                    let base = region_order * tuple_len;
                    for (i, slot) in expected.iter_mut().enumerate() {
                        *slot += decoded[base + i] as f64 * scalar as f64;
                    }
                }
                assert_eq!(actual.len(), expected.len());
                for (a, e) in actual.iter().zip(expected.iter()) {
                    assert_eq!(*a, *e);
                }
                tried += 1;
            }
        }
        assert!(tried > 0, "expected at least one tuple to be exercised");
    }

    fn apply_transform_variations_reference(
        component: &VarcComponent<'_>,
        coords: &[F2Dot14],
        transform: &mut DecomposedTransform,
        var_store: Option<&MultiItemVariationStore<'_>>,
        regions: Option<&SparseVariationRegionList<'_>>,
        scalar_cache: &mut ScalarCache<'_>,
        deltas: &mut [f64],
    ) -> Result<(), Error> {
        let Some(var_idx) = component.transform_var_index() else {
            return Ok(());
        };
        let flags = component.flags();
        const TRANSFORM_MASK: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_TRANSLATE_X.bits()
                | VarcFlags::HAVE_TRANSLATE_Y.bits()
                | VarcFlags::HAVE_ROTATION.bits()
                | VarcFlags::HAVE_SCALE_X.bits()
                | VarcFlags::HAVE_SCALE_Y.bits()
                | VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        let field_count = (flags.bits() & TRANSFORM_MASK.bits()).count_ones() as usize;
        if field_count == 0 {
            return Ok(());
        }

        let store = var_store.ok_or(Error::Malformed)?;
        let regions = regions.ok_or(Error::Malformed)?;
        let deltas = &mut deltas[..field_count];
        compute_tuple_deltas(store, regions, var_idx, coords, scalar_cache, deltas)?;

        let mut delta_iter = deltas.iter().map(|&delta| delta as f32);
        if flags.contains(VarcFlags::HAVE_TRANSLATE_X) {
            transform.set_translate_x(transform.translate_x() + delta_iter.next().unwrap_or(0.0));
        }
        if flags.contains(VarcFlags::HAVE_TRANSLATE_Y) {
            transform.set_translate_y(transform.translate_y() + delta_iter.next().unwrap_or(0.0));
        }
        if flags.contains(VarcFlags::HAVE_ROTATION) {
            transform
                .set_rotation(transform.rotation() + delta_iter.next().unwrap_or(0.0) / 4096.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_X) {
            transform.set_scale_x(transform.scale_x() + delta_iter.next().unwrap_or(0.0) / 1024.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_Y) {
            transform.set_scale_y(transform.scale_y() + delta_iter.next().unwrap_or(0.0) / 1024.0);
        }
        const SKEW_OR_CENTER: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        if flags.intersects(SKEW_OR_CENTER) {
            if flags.contains(VarcFlags::HAVE_SKEW_X) {
                transform
                    .set_skew_x(transform.skew_x() + delta_iter.next().unwrap_or(0.0) / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_SKEW_Y) {
                transform
                    .set_skew_y(transform.skew_y() + delta_iter.next().unwrap_or(0.0) / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_X) {
                transform.set_center_x(transform.center_x() + delta_iter.next().unwrap_or(0.0));
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_Y) {
                transform.set_center_y(transform.center_y() + delta_iter.next().unwrap_or(0.0));
            }
        }
        if !flags.contains(VarcFlags::HAVE_SCALE_Y) {
            transform.set_scale_y(transform.scale_x());
        }
        Ok(())
    }

    #[test]
    fn apply_transform_variations_matches_reference_path() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let coverage = outlines.varc.coverage().unwrap();
        let var_store = outlines.varc.multi_var_store().transpose().unwrap();
        let regions = var_store
            .as_ref()
            .map(|s| s.region_list())
            .transpose()
            .unwrap();
        let region_count = regions
            .as_ref()
            .map(|r| r.region_count() as usize)
            .unwrap_or(0);

        let mut coords = alloc::vec![F2Dot14::ZERO; outlines.axis_count];
        for (i, c) in coords.iter_mut().enumerate() {
            *c = match i % 4 {
                0 => coord(0.5),
                1 => coord(-0.5),
                2 => coord(0.25),
                _ => coord(-0.25),
            };
        }

        let mut tested = 0usize;
        for gid in coverage.iter() {
            let coverage_index = coverage.get(gid).unwrap() as usize;
            let glyph = outlines.varc.glyph(coverage_index).unwrap();
            for component in glyph.components() {
                let component = component.unwrap();
                if component.transform_var_index().is_none() {
                    continue;
                }

                let mut transform_new = *component.transform();
                let mut transform_ref = *component.transform();
                let mut values_new = alloc::vec![0.0f32; region_count];
                let mut values_ref = alloc::vec![0.0f32; region_count];
                let mut cache_new = ScalarCache::new(&mut values_new);
                let mut cache_ref = ScalarCache::new(&mut values_ref);
                let mut deltas_ref = [0.0f64; TRANSFORM_FIELD_COUNT];
                let mut buf = test_memory(&outlines);
                let mut scratch = test_layout(&outlines).carve(&mut buf).unwrap().scratch;
                let store_regions = var_store.as_ref().zip(regions.as_ref());

                apply_transform_variations(
                    &component,
                    &coords,
                    &mut transform_new,
                    &mut cache_new,
                    &mut scratch,
                    store_regions,
                )
                .unwrap();
                apply_transform_variations_reference(
                    &component,
                    &coords,
                    &mut transform_ref,
                    var_store.as_ref(),
                    regions.as_ref(),
                    &mut cache_ref,
                    &mut deltas_ref,
                )
                .unwrap();

                assert_close(transform_new.translate_x(), transform_ref.translate_x());
                assert_close(transform_new.translate_y(), transform_ref.translate_y());
                assert_close(transform_new.rotation(), transform_ref.rotation());
                assert_close(transform_new.scale_x(), transform_ref.scale_x());
                assert_close(transform_new.scale_y(), transform_ref.scale_y());
                assert_close(transform_new.skew_x(), transform_ref.skew_x());
                assert_close(transform_new.skew_y(), transform_ref.skew_y());
                assert_close(transform_new.center_x(), transform_ref.center_x());
                assert_close(transform_new.center_y(), transform_ref.center_y());
                tested += 1;
                if tested >= 32 {
                    break;
                }
            }
            if tested >= 32 {
                break;
            }
        }
        assert!(tested > 0, "expected at least one transformed component");
    }

    // Build the store/regions needed by `eval_condition`'s signature. The
    // conditions exercised below never touch the variation store, so any valid
    // VARC store works here.
    fn condition_eval_env() -> (
        FontRef<'static>,
        MultiItemVariationStore<'static>,
        SparseVariationRegionList<'static>,
    ) {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        (font, store, regions)
    }

    #[test]
    fn component_conditions_without_variations_do_not_require_a_store() {
        use crate::{FontData, FontRead};

        let leaf = alloc::vec![0, 1, 0, 0, 0xE0, 0, 0x20, 0]; // axis 0: [-0.5, 0.5]
        let mut cases = alloc::vec![(leaf.clone(), false)];
        for default in [-1i16, 0, 1] {
            let mut bytes = alloc::vec![0, 2];
            bytes.extend(default.to_be_bytes());
            bytes.extend(NO_VARIATION_INDEX.to_be_bytes());
            cases.push((bytes, default > 0));
        }
        for format in [3, 4] {
            let mut bytes = alloc::vec![0, format, 1, 0, 0, 6];
            bytes.extend_from_slice(&leaf);
            cases.push((bytes, false));
        }
        let mut negate = alloc::vec![0, 5, 0, 0, 5];
        negate.extend_from_slice(&leaf);
        cases.push((negate, true));

        for (condition, expected) in cases {
            let mut bytes = alloc::vec![0u8; 24]; // VARC header
            bytes[..4].copy_from_slice(&0x00010000u32.to_be_bytes());
            bytes[12..16].copy_from_slice(&24u32.to_be_bytes()); // condition list
            bytes.extend(1u32.to_be_bytes()); // one condition
            bytes.extend(8u32.to_be_bytes()); // condition offset
            bytes.extend_from_slice(&condition);
            let glyphs_offset = bytes.len() as u32;
            bytes[20..24].copy_from_slice(&glyphs_offset.to_be_bytes());
            // One CFF2 INDEX entry: component with condition 0, gid 1.
            bytes.extend([0, 0, 0, 1, 1, 1, 6, 0x80, 0x80, 0, 1, 0]);
            let varc = Varc::read(FontData::new(&bytes)).unwrap();
            let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
            let mut outlines = test_outlines(&font);
            outlines.varc = varc;
            let glyph = outlines.varc.glyph(0).unwrap();
            let component = glyph.components().next().unwrap().unwrap();
            assert_eq!(component.condition_index(), Some(0));
            let mut buf = test_memory(&outlines);
            let mut scratch = test_layout(&outlines).carve(&mut buf).unwrap().scratch;
            let result = outlines.component_condition_met(
                &component,
                &[coord(0.75)],
                &mut ScalarCache::new(&mut []),
                &mut scratch,
                None,
            );
            assert_eq!(result.unwrap(), expected, "condition bytes {condition:?}");
        }
    }

    #[test]
    fn component_zero_deltas_do_not_require_a_store() {
        use crate::{FontData, FontRead};

        for (component_bytes, axis_indices, expected_coord, expected_translation) in [
            // One explicit axis value with NO_VARIATION_INDEX.
            (
                alloc::vec![6, 0, 1, 0, 0x40, 0x10, 0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF],
                alloc::vec![0, 0], // packed axis index 0
                0.25,
                0.0,
            ),
            // An empty axis tuple consumes no deltas, even with a real index.
            (alloc::vec![6, 0, 1, 0, 0], alloc::vec![], 0.5, 0.0),
            // A translated component with NO_VARIATION_INDEX.
            (
                alloc::vec![0x18, 0, 1, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 7],
                alloc::vec![],
                0.5,
                7.0,
            ),
        ] {
            let mut bytes = alloc::vec![0u8; 24];
            bytes[..4].copy_from_slice(&0x00010000u32.to_be_bytes());
            bytes[16..20].copy_from_slice(&24u32.to_be_bytes());
            bytes.extend([0, 0, 0, 1, 1, 1, axis_indices.len() as u8 + 1]);
            bytes.extend_from_slice(&axis_indices);
            let glyphs_offset = bytes.len() as u32;
            bytes[20..24].copy_from_slice(&glyphs_offset.to_be_bytes());
            bytes.extend([0, 0, 0, 1, 1, 1, component_bytes.len() as u8 + 1]);
            bytes.extend_from_slice(&component_bytes);
            let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
            let mut outlines = test_outlines(&font);
            outlines.varc = Varc::read(FontData::new(&bytes)).unwrap();
            let glyph = outlines.varc.glyph(0).unwrap();
            let component = glyph.components().next().unwrap().unwrap();
            let mut coords = [F2Dot14::ZERO; 1];
            let mut cache = ScalarCache::new(&mut []);
            let mut buf = test_memory(&outlines);
            let mut scratch = test_layout(&outlines).carve(&mut buf).unwrap().scratch;
            // A stale scratch delta must not leak into the zero-delta case.
            scratch.deltas.fill(100.0);
            outlines
                .component_coords(
                    &component,
                    &[coord(0.5)],
                    &mut coords,
                    &mut cache,
                    &mut scratch,
                    &[coord(0.5)],
                    None,
                )
                .unwrap();
            assert_eq!(coords, [coord(expected_coord)]);
            let mut transform = *component.transform();
            apply_transform_variations(
                &component,
                &[coord(0.5)],
                &mut transform,
                &mut cache,
                &mut scratch,
                None,
            )
            .unwrap();
            assert_eq!(transform.translate_x(), expected_translation);
        }
    }

    #[test]
    fn null_condition_offsets_are_unconditional() {
        use crate::{FontData, FontRead};

        let leaf = [0, 1, 0, 0, 0xE0, 0, 0x20, 0]; // false at 0.75
        let mut cases = alloc::vec![
            (None, true),
            (Some(alloc::vec![0, 3, 1, 0, 0, 0]), true),
            (Some(alloc::vec![0, 4, 1, 0, 0, 0]), true),
            (Some(alloc::vec![0, 5, 0, 0, 0]), false),
        ];
        for format in [3, 4] {
            for null_first in [false, true] {
                let mut bytes = alloc::vec![0, format, 2];
                bytes.extend_from_slice(if null_first {
                    &[0, 0, 0, 0, 0, 9]
                } else {
                    &[0, 0, 9, 0, 0, 0]
                });
                bytes.extend_from_slice(&leaf);
                cases.push((Some(bytes), format == 4));
            }
        }
        for (condition, expected) in cases {
            let mut bytes = alloc::vec![0u8; 24];
            bytes[..4].copy_from_slice(&0x00010000u32.to_be_bytes());
            bytes[12..16].copy_from_slice(&24u32.to_be_bytes());
            bytes.extend(1u32.to_be_bytes());
            bytes.extend(if condition.is_some() { 8u32 } else { 0 }.to_be_bytes());
            if let Some(condition) = condition.as_ref() {
                bytes.extend_from_slice(condition);
            }
            let glyphs_offset = bytes.len() as u32;
            bytes[20..24].copy_from_slice(&glyphs_offset.to_be_bytes());
            bytes.extend([0, 0, 0, 1, 1, 1, 6, 0x80, 0x80, 0, 1, 0]);
            let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
            let mut outlines = test_outlines(&font);
            outlines.varc = Varc::read(FontData::new(&bytes)).unwrap();
            let glyph = outlines.varc.glyph(0).unwrap();
            let component = glyph.components().next().unwrap().unwrap();
            let mut buf = test_memory(&outlines);
            let mut scratch = test_layout(&outlines).carve(&mut buf).unwrap().scratch;
            let result = outlines.component_condition_met(
                &component,
                &[coord(0.75)],
                &mut ScalarCache::new(&mut []),
                &mut scratch,
                None,
            );
            assert_eq!(result.unwrap(), expected, "condition {condition:?}");
        }
    }

    // A deeply nested condition tree must return an error instead of overflowing
    // the stack. Regression test for the unbounded recursion in `eval_condition`.
    #[test]
    fn eval_condition_bounds_recursion_depth() {
        use crate::{FontData, FontRead};
        // Linear chain of Format5Negate tables: each is `format(u16 BE = 5)` +
        // `condition_offset(Offset24 BE = 5)`, so every table points 5 bytes
        // forward to the next -> an arbitrarily deep condition tree. `CHAIN_LEN`
        // is far larger than the limit, proving evaluation bails out early rather
        // than walking (and recursing into) the whole chain.
        const CHAIN_LEN: usize = 4096;
        let mut bytes = alloc::vec::Vec::with_capacity(CHAIN_LEN * 5);
        for _ in 0..CHAIN_LEN {
            bytes.extend_from_slice(&[0x00, 0x05, 0x00, 0x00, 0x05]);
        }
        let cond = Condition::read(FontData::new(&bytes)).unwrap();

        let (_font, store, regions) = condition_eval_env();
        let mut values = alloc::vec![0.0f32; regions.region_count() as usize];
        let mut cache = ScalarCache::new(&mut values);
        let mut deltas = [0.0f64; 1];
        let mut scratch = Scratch {
            deltas: &mut deltas,
            axis_values: &mut [],
            axis_indices: &mut [],
            edges_left: MAX_COMPOSITE_EDGES,
        };

        let result = eval_condition(
            &cond,
            &[],
            Some((&store, &regions)),
            &mut cache,
            &mut scratch,
        );
        assert!(
            matches!(result, Err(Error::LimitExceeded)),
            "expected RecursionLimitExceeded, got {result:?}"
        );
    }

    // A normal (shallow) condition tree must still evaluate correctly.
    #[test]
    fn eval_condition_shallow_is_correct() {
        use crate::{FontData, FontRead};
        let (_font, store, regions) = condition_eval_env();
        let mut values = alloc::vec![0.0f32; regions.region_count() as usize];
        let mut cache = ScalarCache::new(&mut values);
        let mut deltas = [0.0f64; 1];
        let mut scratch = Scratch {
            deltas: &mut deltas,
            axis_values: &mut [],
            axis_indices: &mut [],
            edges_left: MAX_COMPOSITE_EDGES,
        };

        // ConditionFormat1: axis 0, filter range [-1.0, 1.0].
        let leaf: [u8; 8] = [0x00, 0x01, 0x00, 0x00, 0xC0, 0x00, 0x40, 0x00];
        let cond = Condition::read(FontData::new(&leaf)).unwrap();
        assert!(eval_condition(
            &cond,
            &[coord(0.5)],
            Some((&store, &regions)),
            &mut cache,
            &mut scratch,
        )
        .unwrap());
        assert!(!eval_condition(
            &cond,
            &[coord(1.5)],
            Some((&store, &regions)),
            &mut cache,
            &mut scratch,
        )
        .unwrap());

        // Format5Negate wrapping the same leaf must negate the result, proving a
        // shallow nested tree still evaluates through the recursion guard.
        let mut negate = alloc::vec![0x00, 0x05, 0x00, 0x00, 0x05];
        negate.extend_from_slice(&leaf);
        let cond = Condition::read(FontData::new(&negate)).unwrap();
        assert!(!eval_condition(
            &cond,
            &[coord(0.5)],
            Some((&store, &regions)),
            &mut cache,
            &mut scratch,
        )
        .unwrap());
    }

    fn first_nested_varc_edge(outlines: &Outlines<'_>) -> Option<(GlyphId, u32)> {
        for gid in outlines.coverage.iter() {
            let coverage_index = outlines.coverage.get(gid)?;
            let glyph = outlines.varc.glyph(coverage_index as usize).ok()?;
            for component in glyph.components() {
                let component = component.ok()?;
                // Pick an unconditional child edge so the traversal always attempts
                // to recurse for this component.
                if component.condition_index().is_none()
                    && component.gid() != gid
                    && outlines.coverage.get(component.gid()).is_some()
                {
                    return Some((gid, coverage_index));
                }
            }
        }
        None
    }

    #[test]
    fn draw_glyph_respects_total_edge_budget() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let (root_gid, _) = first_nested_varc_edge(&outlines)
            .expect("expected at least one VARC->VARC component edge in fixture font");
        let outline = outlines.outline(root_gid).unwrap();
        let layout = outlines.layout_of(&outline, &NullContext).unwrap();
        let mut buf = alloc::vec![0u8; layout.packed_len().unwrap()];
        let mut memory = layout.carve(&mut buf).unwrap();
        // Budget of one edge allows entering the root glyph only; any recursive
        // component edge must be rejected as over budget.
        memory.scratch.edges_left = 1;
        let mut pen = alloc::vec::Vec::<PathElement>::new();
        let result = outlines.draw_tree(&outline, &layout, &NullContext, None, memory, &mut pen);
        assert!(
            matches!(result, Err(Error::LimitExceeded)),
            "expected RecursionLimitExceeded when edge budget is exhausted, got {result:?}"
        );
    }

    #[test]
    fn measuring_respects_total_edge_budget() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let (root_gid, root_cov_idx) = first_nested_varc_edge(&outlines)
            .expect("expected at least one VARC->VARC component edge in fixture font");
        let mut walk = Walk::new();
        walk.edges_left = 0;
        let leaf = outlines
            .measure(root_gid, root_cov_idx, &NullContext, &mut walk)
            .unwrap();
        assert_eq!(leaf, 0);
        assert_eq!(walk.edges_left, 0);
        assert!(walk.truncated);
    }

    // The index scratch holds the longest list in the table, which may be
    // longer than the axis count when a list repeats an axis.
    #[test]
    fn axis_index_scratch_holds_the_longest_list() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let varc = font.varc().unwrap();
        let lists = varc.axis_indices_list().unwrap().unwrap().count() as usize;
        let longest = (0..lists)
            .map(|nth| varc.axis_indices(nth).unwrap().iter().count())
            .max()
            .unwrap();
        assert_eq!(outlines.index_count, longest);
        // A layout whose lists are longer than its axes carves slices of the
        // list length for indices and values, and of the axis count for
        // coordinates.
        let layout = Layout {
            depth: 2,
            axis_count: 2,
            index_count: 5,
            region_count: 3,
            leaf: 0,
        };
        let mut buf = alloc::vec![0u8; layout.packed_len().unwrap()];
        let memory = layout.carve(&mut buf).unwrap();
        assert_eq!(memory.scratch.axis_indices.len(), 5);
        assert_eq!(memory.scratch.axis_values.len(), 5);
        assert_eq!(memory.scratch.deltas.len(), TRANSFORM_FIELD_COUNT);
        assert_eq!(memory.font_coords.len(), 2);
        assert_eq!(memory.coords.len(), 4);
    }

    // Making an outline reads only the coverage. The tree is measured the
    // first time its memory is needed, once, and a clone keeps the result.
    #[test]
    fn the_tree_is_measured_on_first_use() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let (root_gid, _) = first_nested_varc_edge(&outlines).unwrap();
        let outline = outlines.outline(root_gid).unwrap();
        assert_eq!(
            outline.measured.state.load(Ordering::Relaxed),
            Measured::UNMEASURED
        );
        let size = outlines.required_memory_size(&outline, &NullContext);
        assert!(size > 0);
        let state = outline.measured.state.load(Ordering::Relaxed);
        assert_eq!(state & 3, Measured::MEASURED);
        let copy = outline.clone();
        assert_eq!(copy.measured.state.load(Ordering::Relaxed), state);
        assert_eq!(outlines.required_memory_size(&copy, &NullContext), size);
        // A glyph the table does not cover has no outline.
        let uncovered = (0..font.maxp().unwrap().num_glyphs() as u32)
            .map(GlyphId::new)
            .find(|gid| !outlines.covers(*gid))
            .unwrap();
        assert!(outlines.outline(uncovered).is_none());
    }

    #[test]
    fn the_layout_counts_the_nesting_and_the_largest_leaf() {
        /// Gives each leaf a size in bytes equal to its glyph id, so the
        /// largest is known.
        struct LeafSizes;

        impl VarcContext for LeafSizes {
            fn coords(&self) -> &[F2Dot14] {
                &[]
            }

            fn draw_glyph(
                &self,
                _glyph: GlyphId,
                _coords: &[F2Dot14],
                _ppem: Option<f32>,
                _buf: &mut [u8],
                _pen: &mut dyn PathSink,
            ) -> Result<(), Error> {
                Ok(())
            }

            fn glyph_buffer_size(&self, glyph: GlyphId) -> usize {
                glyph.to_u32() as usize
            }
        }

        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = test_outlines(&font);
        let (root_gid, _) = first_nested_varc_edge(&outlines).unwrap();
        let outline = outlines.outline(root_gid).unwrap();
        let layout = outlines.layout_of(&outline, &LeafSizes).unwrap();
        assert!(layout.depth >= 2, "a nested composite is two levels deep");
        assert_eq!(layout.axis_count, outlines.axis_count);
        assert_eq!(layout.index_count, outlines.index_count);
        assert_eq!(layout.region_count, outlines.region_count());
        assert!(layout.leaf > 0);
        // The block carves at exactly its stated size and not one byte short
        // of the leaf.
        let packed_len = layout.packed_len().unwrap();
        let mut buf = alloc::vec![0u8; packed_len];
        assert!(layout.carve(&mut buf).is_some());
        let mut short = alloc::vec![0u8; packed_len - layout.leaf - 1];
        assert!(layout.carve(&mut short).is_none());
        // More regions than memory can hold is refused rather than wrapped.
        let huge = Layout {
            region_count: usize::MAX / 2,
            ..layout
        };
        assert!(huge.packed_len().is_none());
    }

    // Every component reference counts against the edge limit, leaves
    // included, so a flat composite with too many components is refused
    // before anything is drawn.
    #[test]
    fn leaf_components_count_against_the_edge_limit() {
        use crate::model::{
            glyph::outline::{DrawMemory, DrawOptions, OutlineSource},
            Blob, Font,
        };
        use alloc::{sync::Arc, vec, vec::Vec};
        use types::Tag;
        let count = MAX_COMPOSITE_EDGES + 1;
        let mut varc = vec![0u8; 24];
        varc[..4].copy_from_slice(&0x00010000u32.to_be_bytes());
        varc[4..8].copy_from_slice(&24u32.to_be_bytes());
        // Coverage of glyph 0 alone.
        varc.extend_from_slice(&[0, 1, 0, 1, 0, 0]);
        let offset = varc.len() as u32;
        varc[20..24].copy_from_slice(&offset.to_be_bytes());
        // One CFF2 INDEX entry with 32 bit offsets.
        varc.extend_from_slice(&1u32.to_be_bytes());
        varc.push(4);
        varc.extend_from_slice(&1u32.to_be_bytes());
        varc.extend_from_slice(&((count * 3 + 1) as u32).to_be_bytes());
        for _ in 0..count {
            // No flags, glyph 1.
            varc.extend_from_slice(&[0, 0, 1]);
        }
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"VARC") {
                return Some(Blob::from(varc.clone()));
            }
            let base = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).ok()?;
            base.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let outline = font.outline_glyphs().get(GlyphId::new(0)).unwrap();
        assert_eq!(outline.source(), OutlineSource::Varc);
        assert_eq!(outline.required_memory_size(), 0);
        let mut path = Vec::<PathElement>::new();
        let result = outline.draw(&DrawOptions::default(), DrawMemory::Scratch, &mut path);
        assert_eq!(result.err(), Some(Error::LimitExceeded));
        assert!(path.is_empty());
    }

    // A component that leads back to a glyph being drawn closes a cycle and
    // draws nothing; the glyph's other components still draw.
    #[test]
    fn a_cycle_draws_nothing() {
        use crate::model::{
            glyph::outline::{DrawMemory, DrawOptions},
            Blob, Font,
        };
        use alloc::{sync::Arc, vec, vec::Vec};
        use types::Tag;
        const LEAF: u8 = 2;
        let mut varc = vec![0u8; 24];
        varc[..4].copy_from_slice(&0x00010000u32.to_be_bytes());
        varc[4..8].copy_from_slice(&24u32.to_be_bytes());
        // Coverage of glyphs 1 and 3, which have outlines of their own.
        varc.extend_from_slice(&[0, 1, 0, 2, 0, 1, 0, 3]);
        let offset = varc.len() as u32;
        varc[20..24].copy_from_slice(&offset.to_be_bytes());
        // Glyph 1 uses glyph 3, which uses glyph 1 and the leaf. Each
        // component has no flags and a 16 bit glyph.
        let records: [&[u8]; 2] = [&[0, 0, 3], &[0, 0, 1, 0, 0, LEAF]];
        varc.extend_from_slice(&2u32.to_be_bytes());
        varc.push(4);
        let mut end = 1u32;
        varc.extend_from_slice(&end.to_be_bytes());
        for record in records {
            end += record.len() as u32;
            varc.extend_from_slice(&end.to_be_bytes());
        }
        records
            .iter()
            .for_each(|record| varc.extend_from_slice(record));
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"VARC") {
                return Some(Blob::from(varc.clone()));
            }
            let base = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).ok()?;
            base.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let draw = |font: &Font, glyph: u8| {
            let mut path = Vec::<PathElement>::new();
            let outline = font
                .outline_glyphs()
                .get(GlyphId::new(glyph as u32))
                .unwrap();
            outline
                .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut path)
                .unwrap();
            path
        };
        let base = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let leaf = draw(&base, LEAF);
        assert!(!leaf.is_empty());
        assert!(!draw(&base, 1).is_empty());
        assert_eq!(draw(&Font::new(source, 0).unwrap(), 1), leaf);
    }
}
