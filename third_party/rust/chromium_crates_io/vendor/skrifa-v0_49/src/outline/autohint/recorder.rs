use crate::outline::autohint::topo::{BlueProvenance, Dimension};
use alloc::vec::Vec;

/// Hinting action for a point.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum PointAction {
    IpBefore,
    IpAfter,
    IpOn,
    IpBetween,
}

/// Hinting action for an edge.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum EdgeAction {
    Blue,
    BlueAnchor,
    Anchor,
    Adjust,
    Link,
    Stem,
    Serif,
    SerifAnchor,
    SerifLink1,
    SerifLink2,
    Bound,
}

/// Hinting information for a point.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct PointHint {
    pub action: PointAction,
    pub dimension: Dimension,
    pub point_index: u16,
    pub edge_index: Option<u16>,
    pub edge2_index: Option<u16>,
}

/// Hinting information for an edge.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct EdgeHint {
    pub action: EdgeAction,
    pub dimension: Dimension,
    pub edge_index: u16,
    pub edge2_index: Option<u16>,
    pub edge3_index: Option<u16>,
    pub lower_bound_index: Option<u16>,
    pub upper_bound_index: Option<u16>,
    pub blue: Option<BlueProvenance>,
}

/// Hinting action for a point or an edge.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum HintAction {
    Point(PointHint),
    Edge(EdgeHint),
}

#[derive(Clone, Default, PartialEq, Eq, Debug)]
pub struct HintsRecorder {
    pub actions: Vec<HintAction>,
}

impl HintsRecorder {
    pub fn record_ip_before(&mut self, dim: Dimension, point_ix: usize) {
        self.actions.push(HintAction::Point(PointHint {
            action: PointAction::IpBefore,
            dimension: dim,
            point_index: narrow(point_ix),
            edge_index: None,
            edge2_index: None,
        }));
    }

    pub fn record_ip_after(&mut self, dim: Dimension, point_ix: usize) {
        self.actions.push(HintAction::Point(PointHint {
            action: PointAction::IpAfter,
            dimension: dim,
            point_index: narrow(point_ix),
            edge_index: None,
            edge2_index: None,
        }));
    }

    pub fn record_ip_on(&mut self, dim: Dimension, point_ix: usize, edge_ix: usize) {
        self.actions.push(HintAction::Point(PointHint {
            action: PointAction::IpOn,
            dimension: dim,
            point_index: narrow(point_ix),
            edge_index: Some(narrow(edge_ix)),
            edge2_index: None,
        }));
    }

    pub fn record_ip_between(
        &mut self,
        dim: Dimension,
        point_ix: usize,
        before_edge_ix: usize,
        after_edge_ix: usize,
    ) {
        self.actions.push(HintAction::Point(PointHint {
            action: PointAction::IpBetween,
            dimension: dim,
            point_index: narrow(point_ix),
            edge_index: Some(narrow(before_edge_ix)),
            edge2_index: Some(narrow(after_edge_ix)),
        }));
    }

    #[allow(clippy::too_many_arguments)]
    pub fn record_edge(
        &mut self,
        dim: Dimension,
        action: EdgeAction,
        edge_ix: usize,
        edge2_ix: Option<usize>,
        edge3_ix: Option<usize>,
        lower_bound_ix: Option<usize>,
        upper_bound_ix: Option<usize>,
        blue: Option<BlueProvenance>,
    ) {
        self.actions.push(HintAction::Edge(EdgeHint {
            action,
            dimension: dim,
            edge_index: narrow(edge_ix),
            edge2_index: edge2_ix.map(narrow),
            edge3_index: edge3_ix.map(narrow),
            lower_bound_index: lower_bound_ix.map(narrow),
            upper_bound_index: upper_bound_ix.map(narrow),
            blue,
        }));
    }
}

fn narrow(ix: usize) -> u16 {
    debug_assert!(ix <= u16::MAX as usize);
    ix as u16
}
