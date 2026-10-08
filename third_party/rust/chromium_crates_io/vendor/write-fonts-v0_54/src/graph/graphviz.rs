//! Support for generating graphviz files from our object graph

use std::collections::{BTreeSet, HashSet};

use super::{Graph, ObjectId, OffsetLen};

pub struct GraphVizGraph<'a> {
    graph: &'a Graph,
    nodes: Vec<ObjectId>,
    edges: Vec<GraphVizEdge>,
}

impl<'a> GraphVizGraph<'a> {
    pub(crate) fn from_graph(graph: &'a Graph, prune_non_overflows: bool) -> Self {
        let mut edges = Vec::new();

        // if we are pruning it means that we remove all nodes in spaces
        // that do not include overflows.
        let nodes: BTreeSet<_> = if !prune_non_overflows {
            graph.objects.keys().copied().collect()
        } else {
            let overflows = graph.find_overflows();
            let overflow_spaces = overflows
                .iter()
                .map(|overflow| graph.nodes.get(&overflow.child).unwrap().space)
                .collect::<HashSet<_>>();
            graph
                .nodes
                .iter()
                .filter_map(|(id, node)| overflow_spaces.contains(&node.space).then_some(*id))
                .collect()
        };

        for (parent_id, table) in &graph.objects {
            if !nodes.contains(parent_id) {
                continue;
            }
            let parent = &graph.nodes[parent_id];
            for link in &table.offsets {
                if !nodes.contains(&link.object) {
                    continue;
                }
                let child = &graph.nodes[&link.object];
                let len = child.position - parent.position;
                edges.push(GraphVizEdge {
                    source: *parent_id,
                    target: link.object,
                    len,
                    type_: link.len,
                });
            }
        }

        GraphVizGraph {
            graph,
            edges,
            nodes: nodes.into_iter().collect(),
        }
    }

    /// Write out this graph as a graphviz file to the provided path.
    ///
    /// Overwrites any existing file at this location.
    pub fn write_to_file(&self, path: impl AsRef<std::path::Path>) -> std::io::Result<()> {
        let mut buf = Vec::new();
        dot2::render(self, &mut buf).unwrap();
        std::fs::write(path, &buf)
    }
}

#[derive(Clone, Debug)]
pub struct GraphVizEdge {
    source: ObjectId,
    target: ObjectId,
    len: u32,
    type_: OffsetLen,
}

impl<'a> dot2::GraphWalk<'a> for GraphVizGraph<'a> {
    type Node = ObjectId;
    type Edge = GraphVizEdge;
    type Subgraph = ();

    fn nodes(&'a self) -> dot2::Nodes<'a, Self::Node> {
        self.nodes.as_slice().into()
    }

    fn edges(&'a self) -> dot2::Edges<'a, Self::Edge> {
        self.edges.as_slice().into()
    }

    fn source(&'a self, edge: &Self::Edge) -> Self::Node {
        edge.source
    }

    fn target(&'a self, edge: &Self::Edge) -> Self::Node {
        edge.target
    }
}

impl<'a> dot2::Labeller<'a> for GraphVizGraph<'a> {
    type Node = ObjectId;
    type Edge = GraphVizEdge;
    type Subgraph = ();

    fn graph_id(&'a self) -> dot2::Result<dot2::Id<'a>> {
        dot2::Id::new("TablePacking")
    }

    fn node_id(&'a self, n: &Self::Node) -> dot2::Result<dot2::Id<'a>> {
        dot2::Id::new(format!("N{}", n.0))
    }

    fn node_label<'b>(&'b self, n: &Self::Node) -> dot2::Result<dot2::label::Text<'b>> {
        let obj = &self.graph.objects[n];
        let node = &self.graph.nodes[n];

        let name = if obj.type_.is_mock() {
            // if we have no name (generally because this is a test) then use
            // the object id instead.
            format!("{n:?} ({}B, S{})", obj.bytes.len(), node.space.0)
        } else {
            format!("{} ({}B, S{})", obj.type_, obj.bytes.len(), node.space.0)
        };
        Ok(dot2::label::Text::LabelStr(name.into()))
    }

    fn edge_label(&'a self, e: &Self::Edge) -> dot2::label::Text<'a> {
        dot2::label::Text::LabelStr(e.len.to_string().into())
    }

    fn edge_color(&'a self, e: &Self::Edge) -> Option<dot2::label::Text<'a>> {
        if e.len > e.type_.max_value() {
            return Some(dot2::label::Text::LabelStr("firebrick".into()));
        }
        None
    }

    fn edge_style(&'a self, e: &Self::Edge) -> dot2::Style {
        if e.len > e.type_.max_value() {
            dot2::Style::Bold
        } else {
            dot2::Style::Solid
        }
    }
}
