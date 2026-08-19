//! IFT Graph
//!
//! This command inspects an IFT font an generates a representation of the extension graph formed by invalidating patches.

use std::{
    collections::{BTreeMap, BTreeSet},
    path::Path,
};

use clap::Parser;
use font_types::Tag;
use incremental_font_transfer::{
    font_patch::IncrementalFontPatchBase,
    patch_group::PatchInfo,
    patchmap::{intersecting_patches, PatchFormat, PatchMapEntry, SubsetDefinition},
};
use read_fonts::{ReadError, TableProvider};
use shared_brotli_patch_decoder::BuiltInBrotliDecoder;
use skrifa::{FontRef, MetadataProvider};

#[derive(Parser, Debug)]
#[command(
    version,
    about = "This command inspects an IFT font (https://w3c.github.io/IFT/Overview.html) an generates a representation of the extension graph formed by invalidating patches."
)]
struct Args {
    /// The input IFT font file.
    #[arg(short, long)]
    font: std::path::PathBuf,

    // In the graph output include the patch path associated with each edge.
    #[arg(long)]
    include_patch_paths: bool,
}

fn standard_features() -> BTreeSet<Tag> {
    // Copied from harfbuzz:
    // https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-subset-input.cc#L82
    BTreeSet::from([
        // common
        Tag::new(b"rvrn"),
        Tag::new(b"ccmp"),
        Tag::new(b"liga"),
        Tag::new(b"locl"),
        Tag::new(b"mark"),
        Tag::new(b"mkmk"),
        Tag::new(b"rlig"),
        //fractions
        Tag::new(b"frac"),
        Tag::new(b"numr"),
        Tag::new(b"dnom"),
        //horizontal
        Tag::new(b"calt"),
        Tag::new(b"clig"),
        Tag::new(b"curs"),
        Tag::new(b"kern"),
        Tag::new(b"rclt"),
        //vertical
        Tag::new(b"valt"),
        Tag::new(b"vert"),
        Tag::new(b"vkrn"),
        Tag::new(b"vpal"),
        Tag::new(b"vrt2"),
        //ltr
        Tag::new(b"ltra"),
        Tag::new(b"ltrm"),
        //rtl
        Tag::new(b"rtla"),
        Tag::new(b"rtlm"),
        //random
        Tag::new(b"rand"),
        //justify
        Tag::new(b"jalt"), // HarfBuzz doesn't use; others might
        //East Asian spacing
        Tag::new(b"chws"),
        Tag::new(b"vchw"),
        Tag::new(b"halt"),
        Tag::new(b"vhal"),
        //private
        Tag::new(b"Harf"),
        Tag::new(b"HARF"),
        Tag::new(b"Buzz"),
        Tag::new(b"BUZZ"),
        //shapers

        //arabic
        Tag::new(b"init"),
        Tag::new(b"medi"),
        Tag::new(b"fina"),
        Tag::new(b"isol"),
        Tag::new(b"med2"),
        Tag::new(b"fin2"),
        Tag::new(b"fin3"),
        Tag::new(b"cswh"),
        Tag::new(b"mset"),
        Tag::new(b"stch"),
        //hangul
        Tag::new(b"ljmo"),
        Tag::new(b"vjmo"),
        Tag::new(b"tjmo"),
        //tibetan
        Tag::new(b"abvs"),
        Tag::new(b"blws"),
        Tag::new(b"abvm"),
        Tag::new(b"blwm"),
        //indic
        Tag::new(b"nukt"),
        Tag::new(b"akhn"),
        Tag::new(b"rphf"),
        Tag::new(b"rkrf"),
        Tag::new(b"pref"),
        Tag::new(b"blwf"),
        Tag::new(b"half"),
        Tag::new(b"abvf"),
        Tag::new(b"pstf"),
        Tag::new(b"cfar"),
        Tag::new(b"vatu"),
        Tag::new(b"cjct"),
        Tag::new(b"init"),
        Tag::new(b"pres"),
        Tag::new(b"abvs"),
        Tag::new(b"blws"),
        Tag::new(b"psts"),
        Tag::new(b"haln"),
        Tag::new(b"dist"),
        Tag::new(b"abvm"),
        Tag::new(b"blwm"),
    ])
}

fn get_feature_tags(font: &FontRef<'_>) -> Result<BTreeSet<Tag>, ReadError> {
    let standard_features = standard_features();

    let mut result: BTreeSet<Tag> = Default::default();

    if let Ok(gsub) = font.gsub() {
        for fr in gsub.feature_list()?.feature_records() {
            if standard_features.contains(&fr.feature_tag()) {
                continue;
            }

            result.insert(fr.feature_tag());
        }
    }

    if let Ok(gpos) = font.gpos() {
        for fr in gpos.feature_list()?.feature_records() {
            if standard_features.contains(&fr.feature_tag()) {
                continue;
            }

            result.insert(fr.feature_tag());
        }
    }

    Ok(result)
}

fn get_node_name(font: &FontRef<'_>) -> Result<String, ReadError> {
    let chars: BTreeSet<char> = font
        .charmap()
        .mappings()
        .map(|(cp, _)| char::from_u32(cp).unwrap())
        .collect();

    let mut name: String = chars.into_iter().collect();

    let features = get_feature_tags(font)?;
    if !features.is_empty() {
        let features: Vec<_> = features.into_iter().map(|t| t.to_string()).collect();
        let features = features.join(",");
        name.push('|');
        name.push_str(&features);
    }

    let axes = font.axes();
    if !axes.is_empty() {
        let axis_strings: Vec<_> = font
            .axes()
            .iter()
            .map(|axis| format!("{}[{}..{}]", axis.tag(), axis.min_value(), axis.max_value()))
            .collect();
        let axis_strings = axis_strings.join(",");
        name.push('|');
        name.push_str(&axis_strings);
    }

    Ok(name)
}

fn to_next_font(base_path: &Path, font: &FontRef<'_>, patch: PatchMapEntry) -> Vec<u8> {
    let path = base_path.join(patch.url().as_ref());
    let patch_bytes = std::fs::read(&path)
        .unwrap_or_else(|e| panic!("Unable to read patch file ({}): {:?}", path.display(), e));

    let patch_info: PatchInfo = patch.into();
    font.apply_table_keyed_patch(&patch_info, &patch_bytes, &BuiltInBrotliDecoder)
        .expect("Patch application failed.")
}

#[derive(Clone, Default, Ord, PartialEq, PartialOrd, Eq)]
struct NodeName(String);

#[derive(Clone, Default, Ord, PartialEq, PartialOrd, Eq)]
struct Edge {
    name: NodeName,
    url: String,
}

fn to_graph(
    base_path: &Path,
    font: FontRef<'_>,
    mut graph: BTreeMap<NodeName, BTreeSet<Edge>>,
) -> BTreeMap<NodeName, BTreeSet<Edge>> {
    let patches =
        intersecting_patches(&font, &SubsetDefinition::all()).expect("patch map parsing failed");

    let node_name = NodeName(get_node_name(&font).unwrap());
    graph.entry(node_name.clone()).or_default();

    for patch in patches {
        if !matches!(patch.format(), PatchFormat::TableKeyed { .. }) {
            // This graph only considers invalidating (that is table keyed patches), so skip all other types.
            continue;
        }

        let url_string = patch.url().as_ref().to_string();
        let next_font = to_next_font(base_path, &font, patch);
        let next_font = FontRef::new(&next_font).expect("Downstream font parsing failed");

        {
            let e = graph.entry(node_name.clone()).or_default();
            let next_node_name = get_node_name(&next_font).unwrap();
            e.insert(Edge {
                name: NodeName(next_node_name),
                url: url_string,
            });
        }

        graph = to_graph(base_path, next_font, graph)
    }

    graph
}

fn main() {
    let args = Args::parse();

    let font_bytes = std::fs::read(&args.font).unwrap_or_else(|e| {
        panic!(
            "Unable to read input font file ({}): {:?}",
            args.font.display(),
            e
        )
    });
    let font = FontRef::new(&font_bytes).expect("Input font parsing failed");
    let mut graph = Default::default();
    graph = to_graph(args.font.parent().unwrap(), font, graph);

    for (key, values) in graph {
        let key = key.0;
        let values: Vec<_> = if !args.include_patch_paths {
            values.into_iter().map(|edge| edge.name.0).collect()
        } else {
            // Add the patch URL to the node name
            values
                .into_iter()
                .map(|edge| format!("{}|{}", edge.name.0, edge.url))
                .collect()
        };
        println!("{key};{}", values.join(";"));
    }
}
