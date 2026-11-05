//! A client side implementation of the Incremental Font Transfer standard <https://w3c.github.io/IFT/Overview.html#glyph-keyed>
//!  
//! More specifically this provides:
//! - Implementation of parsing and reading incremental font patch mappings:
//!   <https://w3c.github.io/IFT/Overview.html#font-format-extensions>
//! - Implementation of parsing and apply incremental font patches:
//!   <https://w3c.github.io/IFT/Overview.html#font-patch-formats>
//!
//! Built on top of the read-fonts crate.

#![cfg_attr(docsrs, feature(doc_cfg))]
#![forbid(unsafe_code)]

pub mod font_patch;
pub mod glyph_keyed;
pub mod patch_group;
pub mod patchmap;
mod short_string;
pub mod table_keyed;
pub mod url_templates;

#[cfg(test)]
mod testdata {
    use std::{collections::HashMap, iter};

    use font_test_data::bebuffer::BeBuffer;
    use skrifa::Tag;
    use write_fonts::{
        tables::{head::Head, loca::Loca, maxp::Maxp},
        FontBuilder,
    };

    pub fn test_font_for_patching_with_loca_mod<F>(
        short_loca: bool,
        loca_mod: F,
        additional_tables: HashMap<Tag, &[u8]>,
    ) -> Vec<u8>
    where
        F: Fn(&mut Vec<u32>),
    {
        let mut font_builder = FontBuilder::new();

        for (tag, data) in additional_tables {
            font_builder.add_raw(tag, data);
        }

        let maxp = Maxp {
            num_glyphs: 15,
            ..Default::default()
        };
        font_builder.add_table(&maxp).unwrap();

        let head = Head {
            index_to_loc_format: if short_loca { 0 } else { 1 },
            ..Default::default()
        };
        font_builder.add_table(&head).unwrap();

        // ## glyf ##
        // glyphs are padded to even number of bytes since loca format will be short.
        let glyf = if !short_loca {
            // Since we want a long loca artificially inflate glyf table to ensure long offsets are needed.
            BeBuffer::new().extend(iter::repeat_n(0, 140000))
        } else {
            BeBuffer::new()
        };

        let glyf = glyf
            .push_with_tag(1u8, "gid_0")
            .extend([2, 3, 4, 5u8, 0u8])
            .push_with_tag(6u8, "gid_1")
            .extend([7, 8u8, 0u8])
            .push_with_tag(9u8, "gid_8")
            .extend([10, 11, 12u8]);

        // ## loca ##
        let gid_0 = glyf.offset_for("gid_0") as u32;
        let gid_1 = glyf.offset_for("gid_1") as u32;
        let gid_8 = glyf.offset_for("gid_8") as u32;
        let end = gid_8 + 4;

        let mut loca = vec![
            gid_0, // gid 0
            gid_1, // gid 1
            gid_8, // gid 2
            gid_8, // gid 3
            gid_8, // gid 4
            gid_8, // gid 5
            gid_8, // gid 6
            gid_8, // gid 7
            gid_8, // gid 8
            end,   // gid 9
            end,   // gid 10
            end,   // gid 11
            end,   // gid 12
            end,   // gid 13
            end,   // gid 14
            end,   // end
        ];

        loca_mod(&mut loca);

        let loca = Loca::new(loca);
        font_builder.add_table(&loca).unwrap();

        let glyf: &[u8] = &glyf;
        font_builder.add_raw(Tag::new(b"glyf"), glyf);

        font_builder.build()
    }

    pub fn test_font_for_patching() -> Vec<u8> {
        test_font_for_patching_with_loca_mod(
            true,
            |_| {},
            HashMap::from([(Tag::new(b"IFT "), vec![0, 0, 0, 0].as_slice())]),
        )
    }
}
