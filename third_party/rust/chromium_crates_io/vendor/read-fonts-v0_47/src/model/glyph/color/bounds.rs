//! Color glyph extents computed from what the glyph paints.
//!
//! Provides a [`PaintSink`] that tracks the transform and clip stacks and
//! takes the union of the clips of all fills, as HarfBuzz's
//! `hb-paint-extents` does. A fill clipped to an outline is measured by the
//! control box of that outline.

use super::{Brush, CompositeMode, PaintSink};
use crate::{
    model::glyph::outline::OutlineGlyphs,
    types::{BoundingBox, GlyphId, Matrix},
};
use smallvec::SmallVec;

/// The area covered by a clip or by the paint inside it.
#[derive(Clone, Copy)]
enum Bounds {
    /// The whole plane, because no clip has been pushed.
    Unbounded,
    /// A box with nonzero area.
    Bounded(BoundingBox<f32>),
    /// No area.
    Empty,
}

impl Bounds {
    /// Creates bounds from a box, or `Empty` if the box has no area.
    ///
    /// HarfBuzz treats a box with no width or height as empty.
    fn of(bounds: BoundingBox<f32>) -> Self {
        if bounds.x_min < bounds.x_max && bounds.y_min < bounds.y_max {
            Bounds::Bounded(bounds)
        } else {
            Bounds::Empty
        }
    }

    /// Expands to also cover `other`.
    fn union(&mut self, other: Bounds) {
        match (*self, other) {
            (_, Bounds::Unbounded) => *self = Bounds::Unbounded,
            (Bounds::Empty, Bounds::Bounded(_)) => *self = other,
            (Bounds::Bounded(a), Bounds::Bounded(b)) => {
                *self = Bounds::Bounded(BoundingBox {
                    x_min: a.x_min.min(b.x_min),
                    y_min: a.y_min.min(b.y_min),
                    x_max: a.x_max.max(b.x_max),
                    y_max: a.y_max.max(b.y_max),
                })
            }
            _ => {}
        }
    }

    /// Shrinks to the area also covered by `other`.
    fn intersect(&mut self, other: Bounds) {
        match (*self, other) {
            (_, Bounds::Empty) => *self = Bounds::Empty,
            (Bounds::Unbounded, Bounds::Bounded(_)) => *self = other,
            (Bounds::Bounded(a), Bounds::Bounded(b)) => {
                *self = Bounds::of(BoundingBox {
                    x_min: a.x_min.max(b.x_min),
                    y_min: a.y_min.max(b.y_min),
                    x_max: a.x_max.min(b.x_max),
                    y_max: a.y_max.min(b.y_max),
                });
            }
            _ => {}
        }
    }
}

/// Returns the bounding box of the corners of `bounds` after applying
/// `transform`.
fn transform_bounds(transform: &Matrix<f32>, bounds: BoundingBox<f32>) -> BoundingBox<f32> {
    let corners = [
        transform.transform(bounds.x_min, bounds.y_min),
        transform.transform(bounds.x_max, bounds.y_min),
        transform.transform(bounds.x_min, bounds.y_max),
        transform.transform(bounds.x_max, bounds.y_max),
    ];
    let (first, rest) = corners.split_first().unwrap();
    rest.iter().fold(
        BoundingBox {
            x_min: first.0,
            y_min: first.1,
            x_max: first.0,
            y_max: first.1,
        },
        |bounds, (x, y)| BoundingBox {
            x_min: bounds.x_min.min(*x),
            y_min: bounds.y_min.min(*y),
            x_max: bounds.x_max.max(*x),
            y_max: bounds.y_max.max(*y),
        },
    )
}

/// A sink that paints nothing and measures what would have been painted.
pub(super) struct BoundsSink<'a> {
    outlines: OutlineGlyphs<'a>,
    /// The transformation matrix at each depth. The last one is current.
    transforms: SmallVec<[Matrix<f32>; 8]>,
    /// The clip at each depth, in root coordinates. The last one is current.
    clips: SmallVec<[Bounds; 8]>,
    /// The area painted by each open layer. The last one is current.
    groups: SmallVec<[Bounds; 8]>,
}

impl<'a> BoundsSink<'a> {
    pub(super) fn new(outlines: OutlineGlyphs<'a>) -> Self {
        let mut sink = Self {
            outlines,
            transforms: SmallVec::new(),
            clips: SmallVec::new(),
            groups: SmallVec::new(),
        };
        sink.transforms.push(Matrix::IDENTITY);
        sink.clips.push(Bounds::Unbounded);
        sink.groups.push(Bounds::Empty);
        sink
    }

    /// Returns the bounding box of everything painted, or an empty box at
    /// the origin if nothing was painted.
    pub(super) fn bounds(&self) -> BoundingBox<f32> {
        match self.groups.last() {
            Some(Bounds::Bounded(bounds)) => *bounds,
            _ => BoundingBox::default(),
        }
    }

    fn current_transform(&self) -> &Matrix<f32> {
        self.transforms.last().unwrap_or(&Matrix::IDENTITY)
    }

    fn current_clip(&self) -> Bounds {
        self.clips.last().copied().unwrap_or(Bounds::Unbounded)
    }

    /// Clips to `bounds`, given in the current coordinate space.
    fn push_clip(&mut self, bounds: Bounds) {
        let mut clip = self.current_clip();
        clip.intersect(bounds);
        self.clips.push(clip);
    }

    /// Records a fill of the current clip.
    fn paint(&mut self) {
        let clip = self.current_clip();
        if let Some(group) = self.groups.last_mut() {
            group.union(clip);
        }
    }
}

impl PaintSink for BoundsSink<'_> {
    fn push_transform(&mut self, transform: Matrix<f32>) {
        let transform = *self.current_transform() * transform;
        self.transforms.push(transform);
    }

    fn pop_transform(&mut self) {
        if self.transforms.len() > 1 {
            self.transforms.pop();
        }
    }

    fn push_clip_glyph(&mut self, glyph: GlyphId) {
        // The outline's control box, as a drawn glyph is measured.
        let bounds = match self
            .outlines
            .get(glyph)
            .and_then(|glyph| glyph.control_box().ok())
        {
            Some(Some(bounds)) => Bounds::of(transform_bounds(self.current_transform(), bounds)),
            _ => Bounds::Empty,
        };
        self.push_clip(bounds);
    }

    fn push_clip_box(&mut self, clip_box: BoundingBox<f32>) {
        let bounds = Bounds::of(transform_bounds(self.current_transform(), clip_box));
        self.push_clip(bounds);
    }

    fn pop_clip(&mut self) {
        if self.clips.len() > 1 {
            self.clips.pop();
        }
    }

    fn fill(&mut self, _brush: Brush<'_>) {
        self.paint();
    }

    fn push_layer(&mut self, _composite_mode: CompositeMode) {
        self.groups.push(Bounds::Empty);
    }

    fn pop_layer_with_mode(&mut self, composite_mode: CompositeMode) {
        if self.groups.len() < 2 {
            return;
        }
        let source = self.groups.pop().unwrap_or(Bounds::Empty);
        let Some(backdrop) = self.groups.last_mut() else {
            return;
        };
        match composite_mode {
            CompositeMode::Clear => *backdrop = Bounds::Empty,
            CompositeMode::Src | CompositeMode::SrcOut => *backdrop = source,
            CompositeMode::Dest | CompositeMode::DestOut => {}
            CompositeMode::SrcIn | CompositeMode::DestIn => backdrop.intersect(source),
            _ => backdrop.union(source),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn boxed(x_min: f32, y_min: f32, x_max: f32, y_max: f32) -> Bounds {
        Bounds::Bounded(BoundingBox {
            x_min,
            y_min,
            x_max,
            y_max,
        })
    }

    #[test]
    fn union_and_intersection() {
        let mut a = boxed(0.0, 0.0, 10.0, 10.0);
        a.union(boxed(5.0, -5.0, 20.0, 5.0));
        assert!(
            matches!(a, Bounds::Bounded(b) if b == BoundingBox { x_min: 0.0, y_min: -5.0, x_max: 20.0, y_max: 10.0 })
        );
        a.intersect(boxed(15.0, 0.0, 30.0, 30.0));
        assert!(
            matches!(a, Bounds::Bounded(b) if b == BoundingBox { x_min: 15.0, y_min: 0.0, x_max: 20.0, y_max: 10.0 })
        );
        a.intersect(boxed(100.0, 100.0, 200.0, 200.0));
        assert!(matches!(a, Bounds::Empty));
        let mut unbounded = Bounds::Unbounded;
        unbounded.intersect(boxed(1.0, 2.0, 3.0, 4.0));
        assert!(matches!(unbounded, Bounds::Bounded(_)));
        let mut empty = Bounds::Empty;
        empty.union(Bounds::Unbounded);
        assert!(matches!(empty, Bounds::Unbounded));
    }

    // A box with no width or height covers nothing, as in HarfBuzz.
    #[test]
    fn a_box_without_area_is_empty() {
        let line = BoundingBox {
            x_min: 5.0,
            y_min: 0.0,
            x_max: 5.0,
            y_max: 10.0,
        };
        assert!(matches!(Bounds::of(line), Bounds::Empty));
        // Boxes that only touch share no area.
        let mut a = boxed(0.0, 0.0, 5.0, 10.0);
        a.intersect(boxed(5.0, 0.0, 10.0, 10.0));
        assert!(matches!(a, Bounds::Empty));
        // A fill inside a clip without area adds nothing.
        let font = crate::model::Font::new(font_test_data::COLRV0V1_VARIABLE, 0).unwrap();
        let mut sink = BoundsSink::new(font.outline_glyphs());
        sink.push_clip_box(line);
        sink.fill(Brush::Solid {
            palette_index: 0,
            alpha: 1.0,
        });
        sink.pop_clip();
        assert_eq!(sink.bounds(), BoundingBox::default());
    }

    #[test]
    fn a_transformed_box_is_the_box_around_its_corners() {
        // Rotate a quarter turn: x becomes y.
        let rotate = Matrix {
            xx: 0.0,
            yx: 1.0,
            xy: -1.0,
            yy: 0.0,
            dx: 0.0,
            dy: 0.0,
        };
        let bounds = transform_bounds(
            &rotate,
            BoundingBox {
                x_min: 1.0,
                y_min: 2.0,
                x_max: 3.0,
                y_max: 4.0,
            },
        );
        assert_eq!(
            bounds,
            BoundingBox {
                x_min: -4.0,
                y_min: 1.0,
                x_max: -2.0,
                y_max: 3.0,
            }
        );
    }
}
