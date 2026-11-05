//! Benchmark that simulates loading tables required for outline extraction.

use core::hint::black_box;
use criterion::{criterion_group, criterion_main, Criterion};
use font_types::BigEndian;
use read_fonts::{
    model::Font,
    tables::{
        cff::Cff, cff2::Cff2, cvar::Cvar, glyf::Glyf, gvar::Gvar, hdmx::Hdmx, hmtx::Hmtx,
        hvar::Hvar, loca::Loca, maxp::Maxp,
    },
    FontRef, TableProvider,
};

const TEST_FONTS: &[(&str, &[u8])] = &[
    ("glyf", font_test_data::TINOS_SUBSET),
    ("glyf+gvar", font_test_data::AMSTELVAR_AVAR2_A),
    ("cff", font_test_data::NOTO_SERIF_DISPLAY_TRIMMED),
    ("cff2", font_test_data::CANTARELL_VF_TRIMMED),
];

pub fn font_ref_outlines_tables(c: &mut Criterion) {
    for (name, font_data) in TEST_FONTS {
        let tables = FontRef::from_index(font_data, 0).unwrap();
        let name = "FontRef ".to_string() + name + " outline tables";
        c.bench_function(&name, |b| {
            b.iter(|| black_box(OutlineTables::new(&tables).unwrap()))
        });
    }
}

pub fn font_model_outlines_tables(c: &mut Criterion) {
    for (name, font_data) in TEST_FONTS {
        let font = Font::new(*font_data, 0).unwrap();
        let tables = font.tables();
        let name = "Font ".to_string() + name + " outline tables";
        c.bench_function(&name, |b| {
            b.iter(|| black_box(OutlineTables::new(&tables).unwrap()))
        });
    }
}

#[expect(unused)]
struct GlyfTables<'a> {
    loca: Loca<'a>,
    glyf: Glyf<'a>,
    gvar: Option<Gvar<'a>>,
    cvt: Option<&'a [BigEndian<i16>]>,
    cvar: Option<Cvar<'a>>,
    hdmx: Option<Hdmx<'a>>,
}

#[expect(unused)]
enum OutlineFormatTables<'a> {
    None,
    Glyf(GlyfTables<'a>),
    Cff(Cff<'a>),
    Cff2(Cff2<'a>),
}

#[expect(unused)]
struct OutlineTables<'a> {
    maxp: Maxp<'a>,
    hmtx: Hmtx<'a>,
    hvar: Option<Hvar<'a>>,
    format: OutlineFormatTables<'a>,
}

impl<'a> OutlineTables<'a> {
    #[inline(never)]
    fn new(tables: &impl TableProvider<'a>) -> Option<Self> {
        let maxp = tables.maxp().ok()?;
        let hmtx = tables.hmtx().ok()?;
        let hvar = tables.hvar().ok();
        let format = if let Ok(glyf) = tables.glyf() {
            let loca = tables.loca(None).ok()?;
            let gvar = tables.gvar().ok();
            let cvt = tables.cvt().ok();
            let cvar = tables.cvar().ok();
            let hdmx = tables.hdmx().ok();
            OutlineFormatTables::Glyf(GlyfTables {
                loca,
                glyf,
                gvar,
                cvt,
                cvar,
                hdmx,
            })
        } else if let Ok(cff2) = tables.cff2() {
            OutlineFormatTables::Cff2(cff2)
        } else if let Ok(cff) = tables.cff() {
            OutlineFormatTables::Cff(cff)
        } else {
            OutlineFormatTables::None
        };
        Some(Self {
            maxp,
            hmtx,
            hvar,
            format,
        })
    }
}

criterion_group!(
    benches,
    font_ref_outlines_tables,
    font_model_outlines_tables,
);
criterion_main!(benches);
