//! IFT Extension
//!
//! This command line tool executes the IFT extension algorithm (<https://w3c.github.io/IFT/Overview.html#extend-font-subset>) on an IFT font.

use std::{
    collections::{BTreeSet, HashMap},
    io::Cursor,
    str::FromStr,
};

use clap::Parser;
use font_types::{Fixed, Tag};
use incremental_font_transfer::{
    patch_group::{PatchGroup, UrlStatus},
    patchmap::{DesignSpace, FeatureSet, PatchUrl, SubsetDefinition},
};
use read_fonts::collections::{IntSet, RangeSet};
use read_fonts::tables::ift::{GlyphKeyedPatch, GlyphPatches};
use read_fonts::{FontData, FontRead};
use regex::Regex;
use shared_brotli_patch_decoder::{BuiltInBrotliDecoder, SharedBrotliDecoder};
use skrifa::FontRef;

#[derive(Parser, Debug)]
#[command(
    version,
    about = "Run the IFT extension algorithm (https://w3c.github.io/IFT/Overview.html#extend-font-subset) on an IFT font."
)]
struct Args {
    /// The input IFT font file.
    #[arg(short, long)]
    font: std::path::PathBuf,

    /// The output IFT font file.
    #[arg(short, long)]
    output: std::path::PathBuf,

    /// Text to extend the font to cover.
    #[arg(short, long)]
    text: Option<String>,

    /// Comma separated list of Unicode codepoint values (base 10) to extend the font to cover.
    ///
    /// * indicates to include all Unicode codepoints.
    #[arg(short, long, value_delimiter = ',', num_args = 1..)]
    unicodes: Vec<String>,

    /// Comma separate list of open type layout feature tags to extend the font to cover.
    ///
    /// * indicates to include all features.
    #[arg(long, value_delimiter = ',', num_args = 1..)]
    features: Vec<String>,

    /// Comma separate list of design space ranges of the form tag@point or tag@start:end to extend the font to cover.
    ///
    /// * indicates to include all design spaces.
    ///
    /// For example wght at point 300 and wdth from 50 to 100:
    /// --design_space="wght@300,wdth@50:100"
    #[arg(short, long, value_delimiter = ',', num_args = 1..)]
    design_space: Vec<String>,

    /// The maximum number of simulated network round trips allowed during extension.
    ///
    /// If the count is exceeded this command will fail and return an error.
    #[arg(long)]
    max_round_trips: Option<u32>,

    /// The maximum number of fetches trips allowed during extension.
    ///
    /// If the count is exceeded this command will fail and return an error.
    #[arg(long)]
    max_fetches: Option<u32>,
    // TODO(garretrieger): add feature tags arguments.
}

fn main() {
    let args = Args::parse();

    let mut codepoints = IntSet::<u32>::empty();
    if let Some(text) = args.text {
        codepoints.extend_unsorted(text.chars().map(|c| c as u32));
    }

    parse_unicodes(args.unicodes, &mut codepoints).expect("unicodes parsing failed.");
    let features = parse_features(args.features).expect("features parsing failed.");
    let design_space = parse_design_space(args.design_space).expect("design space parsing failed.");

    let subset_definition = SubsetDefinition::new(codepoints, features, design_space);

    let mut font_bytes = std::fs::read(&args.font).unwrap_or_else(|e| {
        panic!(
            "Unable to read input font file ({}): {:?}",
            args.font.display(),
            e
        )
    });
    let initial_font_bytes = font_bytes.len() as u64;

    if font_bytes.starts_with(b"wOF2") {
        println!("    Decompressing WOFF2 font to TTF");
        let mut cursor = Cursor::new(&font_bytes);
        font_bytes = woff2_patched::decode::convert_woff2_to_ttf(&mut cursor)
            .expect("Failed to decode WOFF2 font");
    }

    let mut patch_data: HashMap<PatchUrl, UrlStatus> = Default::default();
    let mut it_count = 0;

    // For this a roundtrip is defined as an iteration of the below loop where at least
    // one new patch URL is needed.
    let mut round_trip_count = 0;
    let mut fetch_count = 0;
    let mut glyph_keyed_bytes = 0u64;
    let mut table_keyed_bytes = 0u64;
    let mut loaded_glyphs = IntSet::<u32>::empty();
    loop {
        it_count += 1;
        println!(">> Iteration {}", it_count);
        let font = FontRef::new(&font_bytes).expect("Input font parsing failed");
        let next_patches = PatchGroup::select_next_patches(font, &patch_data, &subset_definition)
            .expect("Patch selection failed");
        if !next_patches.has_urls() {
            println!("    No outstanding patches, all done.");
            break;
        }

        let mut fetched = false;
        for url in next_patches.urls() {
            patch_data.entry(url.clone()).or_insert_with_key(|key| {
                let url_path = args.font.parent().unwrap().join(url.as_ref());
                println!("    Fetching {}", key.as_ref());
                fetched = true;
                fetch_count += 1;
                if let Some(max_fetch_count) = args.max_fetches {
                    if fetch_count > max_fetch_count {
                        panic!(
                            "Maximum number of fetches ({} > {}) exceeded.",
                            fetch_count, max_fetch_count
                        );
                    }
                }

                let patch_bytes = match std::fs::read(url_path.clone()) {
                    Result::Ok(bytes) => bytes,
                    Result::Err(e) => panic!(
                        "Unable to read patch file ({}): {:?}",
                        url_path.display(),
                        e
                    ),
                };

                let tag = &patch_bytes[0..4];
                if tag == b"ifgk" {
                    glyph_keyed_bytes += patch_bytes.len() as u64;
                    let patch = GlyphKeyedPatch::read(FontData::new(&patch_bytes))
                        .expect("Failed to parse GlyphKeyedPatch");
                    let decoder = BuiltInBrotliDecoder;
                    let decompressed = decoder
                        .decode(
                            patch.brotli_stream(),
                            None,
                            patch.max_uncompressed_length() as usize,
                        )
                        .expect("Failed to decompress patch");
                    let glyph_patches =
                        GlyphPatches::read(FontData::new(&decompressed), patch.flags())
                            .expect("Failed to parse GlyphPatches");
                    for id in glyph_patches.glyph_ids().iter() {
                        loaded_glyphs.insert(id.expect("Failed to read glyph ID").get());
                    }
                } else if tag == b"iftk" {
                    table_keyed_bytes += patch_bytes.len() as u64;
                }

                UrlStatus::Pending(patch_bytes)
            });
        }

        if fetched {
            round_trip_count += 1;
            if let Some(max_round_trips) = args.max_round_trips {
                if round_trip_count > max_round_trips {
                    panic!(
                        "Maximum number of round trips ({} > {}) exceeded.",
                        round_trip_count, max_round_trips
                    );
                }
            }
        }

        if let Some(info) = next_patches.next_invalidating_patch() {
            println!("    Applying next invalidating patch {}", info.url());
        } else {
            println!("    Applying non invalidating patches");
        }

        font_bytes = next_patches
            .apply_next_patches(&mut patch_data)
            .expect("Patch application failed.");
    }

    println!(">> Extension finished");
    std::fs::write(&args.output, font_bytes).expect("Writing output font failed.");
    println!("    Wrote patched font to {}", &args.output.display());
    println!("    Total network round trips = {round_trip_count}");
    println!("    Total fetches = {fetch_count}");
    let network_overhead = fetch_count as u64 * 75;
    let total_bytes = initial_font_bytes + glyph_keyed_bytes + table_keyed_bytes + network_overhead;
    println!("    Input font bytes = {initial_font_bytes}");
    println!("    Glyph keyed patch bytes = {glyph_keyed_bytes}");
    println!("    Table keyed patch bytes = {table_keyed_bytes}");
    println!("    Estimated network overhead bytes = {network_overhead}");
    println!("    Total bytes = {total_bytes}");
    println!(
        "    Glyphs added (count = {}): {:?}",
        loaded_glyphs.len(),
        loaded_glyphs
    );
}

fn parse_unicodes(args: Vec<String>, codepoints: &mut IntSet<u32>) -> Result<(), ParsingError> {
    for unicode_string in args {
        if unicode_string.is_empty() {
            continue;
        }

        if unicode_string == "*" {
            let all = IntSet::<u32>::all();
            codepoints.union(&all);
            return Ok(());
        }
        let Ok(unicode) = unicode_string.parse() else {
            return Err(ParsingError::UnicodeCodepointParsingFailed(unicode_string));
        };
        codepoints.insert(unicode);
    }
    Ok(())
}

fn parse_features(args: Vec<String>) -> Result<FeatureSet, ParsingError> {
    let mut tags = BTreeSet::<Tag>::new();
    for tag_string in args {
        let tag = match tag_string.as_str() {
            "" => continue,
            "*" => return Ok(FeatureSet::All),
            tag => Tag::new_checked(tag.as_bytes())
                .map_err(|_| ParsingError::FeatureTagParsingFailed(tag_string))?,
        };

        tags.insert(tag);
    }

    Ok(FeatureSet::Set(tags))
}

fn parse_fixed(value: &str, flag_value: &str) -> Result<Fixed, ParsingError> {
    f64::from_str(value)
        .map_err(|_| ParsingError::DesignSpaceParsingFailed {
            flag_value: flag_value.to_string(),
            message: "Bad axis position value".to_string(),
        })
        .map(Fixed::from_f64)
}

fn parse_design_space(args: Vec<String>) -> Result<DesignSpace, ParsingError> {
    let re = Regex::new(r"^([a-zA-Z][a-zA-Z0-9 ]{3})@([0-9.]+)(:[0-9.]+)?$").unwrap();

    let mut result = HashMap::<Tag, RangeSet<Fixed>>::default();
    for arg in args {
        if arg.is_empty() {
            continue;
        }

        if arg == "*" {
            return Ok(DesignSpace::All);
        }

        let Some(captures) = re.captures(&arg) else {
            return Err(ParsingError::DesignSpaceParsingFailed {
                flag_value: arg,
                message: "Invalid syntax. Must be tag@value or tag@value:value.".to_string(),
            });
        };

        let tag = captures.get(1).unwrap();
        let Ok(tag) = Tag::new_checked(tag.as_str().as_bytes()) else {
            return Err(ParsingError::DesignSpaceParsingFailed {
                flag_value: arg.clone(),
                message: format!("Bad tag value: {}", tag.as_str()),
            });
        };

        let value_1 = parse_fixed(captures.get(2).unwrap().as_str(), &arg)?;

        let range = if let Some(value_2) = captures.get(3) {
            let value_2 = parse_fixed(&value_2.as_str()[1..], &arg)?;
            value_1..=value_2
        } else {
            value_1..=value_1
        };

        result.entry(tag).or_default().insert(range);
    }

    Ok(DesignSpace::Ranges(result))
}

#[derive(Debug, Clone, PartialEq)]
pub enum ParsingError {
    DesignSpaceParsingFailed { flag_value: String, message: String },
    UnicodeCodepointParsingFailed(String),
    FeatureTagParsingFailed(String),
}

impl std::fmt::Display for ParsingError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            ParsingError::DesignSpaceParsingFailed {
                flag_value,
                message,
            } => {
                write!(
                    f,
                    "Failed parsing design_space flag ({}): {}",
                    flag_value, message
                )
            }
            ParsingError::UnicodeCodepointParsingFailed(value) => {
                write!(f, "Invalid unicode codepoint value: {}", value,)
            }
            ParsingError::FeatureTagParsingFailed(value) => {
                write!(f, "Invalid feature tag value: {value}")
            }
        }
    }
}

impl std::error::Error for ParsingError {}
