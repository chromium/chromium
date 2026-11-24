//! A per-face cache of shape plans.
//!
//! HarfBuzz's `hb_shape` builds its plan through `hb_shape_plan_create_cached`,
//! so repeated calls over the same face, segment properties and features reuse
//! one plan. HarfRust builds a fresh plan per call and offers no cache of its
//! own, so this supplies one, keeping the cost of `hr_shape` in line with what
//! HarfBuzz callers expect.
//!
//! # Why nothing ever needs flushing
//!
//! The cache hangs off the face, and everything the planner reads from a font
//! is fixed for the life of that face: the presence of `GSUB`, `morx` and
//! `mort`, and the script chosen from `GSUB`. The inputs that vary with a
//! font instance are the `GSUB` and `GPOS` feature variation indices and,
//! when lookups vary with the location, the normalized coordinates, and
//! `ShapePlanKey` compares those.
//!
//! So changing a font's variation settings simply misses and builds a new
//! plan, rather than leaving a stale one behind, and the other mutable font
//! properties (scale, point size and callbacks) do not enter into a plan at
//! all. Two fonts over one face share the cache safely for the same reason.

use std::sync::{Arc, OnceLock};

use harfrust::font::Font;
use harfrust::{Direction, Feature, Language, Script, ShapePlan, ShapePlanKey};

/// A face's cache of shape plans.
///
/// A singly linked list whose links are each written once, mirroring the
/// atomic list HarfBuzz keeps on its own faces. No node is ever unlinked or
/// changed after it is published, which is what lets a reader walk the list
/// with plain acquire loads: a hit takes no lock, and hands back a borrow
/// rather than a counted reference, because the nodes live as long as the
/// cache and the cache outlives every shaping call made through the face.
#[derive(Default)]
pub(crate) struct PlanCache {
    head: OnceLock<Box<Node>>,
}

impl Drop for PlanCache {
    fn drop(&mut self) {
        // Iteratively. The chain is boxed, so the derived recursive drop would
        // put one stack frame per plan and overflow on a face that collected a
        // great many. HarfBuzz frees its list with the same explicit loop.
        let mut link = self.head.take();
        while let Some(mut node) = link {
            link = node.next.take();
        }
    }
}

/// One cached plan and the link to the next.
///
/// The plan records what it was built for, so HarfRust's own `ShapePlanKey`
/// decides whether it matches a caller: it ignores where a ranged feature
/// starts and ends, which each shaping call takes from its own features, and
/// compares everything a plan does depend on. Keeping a key of this crate's
/// own here would let the two drift apart.
struct Node {
    /// Counted, because [`hr_shape_plan_create_cached`] hands the same plan
    /// out as an object the caller owns. Shaping only ever borrows it.
    plan: Arc<ShapePlan>,
    next: OnceLock<Box<Node>>,
}

impl PlanCache {
    /// Returns a plan for the given properties, building one if the cache does
    /// not already hold a match.
    pub(crate) fn get(
        &self,
        instance: &Font,
        direction: Direction,
        script: Option<Script>,
        language: Option<&Language>,
        features: &[Feature],
    ) -> &Arc<ShapePlan> {
        let key = ShapePlanKey::new(instance, script, direction)
            .language(language)
            .features(features);

        // A linear walk, as HarfBuzz does over its per-face plan list. The key
        // borrows the caller's own values, so a hit -- the common case once a
        // face is warm -- builds nothing at all.
        let mut link = &self.head;
        while let Some(node) = link.get() {
            if key.matches(&node.plan) {
                return &node.plan;
            }
            link = &node.next;
        }

        let plan = Arc::new(build(instance, direction, script, language, features));
        self.insert(&key, plan)
    }

    /// Publishes a node at the end of the list, or hands back a matching one
    /// that another thread published while this one was building.
    fn insert(&self, key: &ShapePlanKey<'_>, plan: Arc<ShapePlan>) -> &Arc<ShapePlan> {
        let mut node = Box::new(Node {
            plan,
            next: OnceLock::new(),
        });
        let mut link = &self.head;
        loop {
            match link.set(node) {
                Ok(()) => {
                    let published = link.get().expect("just set");
                    return &published.plan;
                }
                Err(unplaced) => {
                    node = unplaced;
                    let occupant = link.get().expect("set failed, so it is occupied");
                    if key.matches(&occupant.plan) {
                        return &occupant.plan;
                    }
                    link = &occupant.next;
                }
            }
        }
    }
}

fn build(
    instance: &Font,
    direction: Direction,
    script: Option<Script>,
    language: Option<&Language>,
    features: &[Feature],
) -> ShapePlan {
    ShapePlan::new(instance, direction, script, language, features)
}

#[cfg(test)]
mod tests {
    use super::*;
    use harfrust::Tag;

    fn font() -> Font {
        let path = concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../harfrust/tests/fonts/rb_custom/LaBelleAurore.ttf"
        );
        let data = std::fs::read(path).expect("failed to read test font");
        Font::new(data, 0).expect("failed to parse test font")
    }

    /// Ranges follow the text, so two styled runs must share a plan, while a
    /// different value or a global feature must not.
    #[test]
    fn ranged_features_share_a_plan_whatever_their_range() {
        let font = font();
        let instance = font.instance_builder().build();
        let cache = PlanCache::default();
        let liga = Tag::new(b"liga");
        let get = |features: &[Feature]| {
            cache
                .get(&instance, Direction::LeftToRight, None, None, features)
                .clone()
        };

        let first = get(&[Feature::new(liga, 0, 0..6)]);
        assert!(Arc::ptr_eq(&first, &get(&[Feature::new(liga, 0, 15..21)])));
        assert!(!Arc::ptr_eq(&first, &get(&[Feature::new(liga, 1, 0..6)])));
        assert!(!Arc::ptr_eq(&first, &get(&[Feature::new(liga, 0, ..)])));
        assert!(Arc::ptr_eq(&first, &get(&[Feature::new(liga, 0, 30..36)])));
    }
}
