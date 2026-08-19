//! The [cpal](https://learn.microsoft.com/en-us/typography/opentype/spec/cpal) table

include!("../../generated/generated_cpal.rs");

#[cfg(test)]
mod tests {

    use super::*;

    fn make_simple_cpal(palettes: Vec<Vec<(u8, u8, u8, u8)>>) -> Cpal {
        let mut cpal = Cpal {
            num_palettes: palettes.len() as u16,
            num_palette_entries: palettes[0].len() as u16,
            num_color_records: palettes.iter().map(Vec::len).sum::<usize>() as u16,
            ..Default::default()
        };
        cpal.color_records_array.set(
            palettes
                .iter()
                .flat_map(|p| p.iter())
                .copied()
                .map(|(r, g, b, a)| ColorRecord {
                    red: r,
                    green: g,
                    blue: b,
                    alpha: a,
                })
                .collect::<Vec<_>>(),
        );
        cpal
    }

    #[test]
    fn write_read_simple_cpal() {
        let cpal = make_simple_cpal(vec![
            vec![(0, 0, 255, 255), (255, 0, 0, 255)],
            vec![(0, 255, 0, 255), (255, 0, 255, 255)],
            vec![(0, 0, 0, 255), (128, 0, 128, 255)],
        ]);
        let bytes = crate::write::dump_table(&cpal).unwrap();
        let loaded = read_fonts::tables::cpal::Cpal::read(FontData::new(&bytes)).unwrap();
        assert_eq!(
            (2, 3, 6),
            (
                loaded.num_palette_entries(),
                loaded.num_palettes(),
                loaded.num_color_records()
            )
        );

        let loaded_colors = loaded.color_records_array().unwrap().unwrap();
        assert_eq!(
            vec![
                read_fonts::tables::cpal::ColorRecord {
                    red: 0,
                    green: 255,
                    blue: 0,
                    alpha: 255,
                },
                read_fonts::tables::cpal::ColorRecord {
                    red: 128,
                    green: 0,
                    blue: 128,
                    alpha: 255,
                },
            ],
            vec![loaded_colors[2], *loaded_colors.last().unwrap(),],
        );
    }
}
