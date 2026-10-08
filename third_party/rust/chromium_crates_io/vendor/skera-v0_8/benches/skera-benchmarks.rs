use criterion::{criterion_group, criterion_main, Criterion};
use skera::{subset_font, Plan, SubsetFlags, DEFAULT_DROP_TABLES, DEFAULT_LAYOUT_FEATURES};
use std::{path::Path, time::Duration};
use write_fonts::{
    read::{collections::IntSet, FontRef},
    types::NameId,
};

fn read_test_font(file_name: &str) -> Vec<u8> {
    let path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("test-data/fonts")
        .join(file_name);
    std::fs::read(&path).expect("failed to read test font file")
}

/// Creates a subsetting plan using the same default options as running the src/main.rs with
/// --unicodes.
fn create_plan(font: &FontRef, unicodes: &IntSet<u32>) -> Plan {
    Plan::new(
        &IntSet::empty(), // gids (empty by default when using --unicodes)
        unicodes,
        font,
        SubsetFlags::default(),
        &DEFAULT_DROP_TABLES.iter().copied().collect(),
        &IntSet::all(), // layout_scripts (default in CLI is all scripts)
        &DEFAULT_LAYOUT_FEATURES.iter().copied().collect(),
        // Keep name IDs 0 to 6 (Copyright through PostScript Name)
        &(0..=6).map(NameId::from).collect(),
        // Keep English (US) locale (0x0409) names
        &[0x0409].into_iter().collect(),
    )
}

fn benchmark_subset(c: &mut Criterion) {
    c.benchmark_group("subset")
        .bench_function("Amiri-Regular-all", |b| {
            let font_bytes = read_test_font("Amiri-Regular.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("Mplus1p-Regular-all", |b| {
            let font_bytes = read_test_font("Mplus1p-Regular.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("MPLUS1-Variable-all", |b| {
            let font_bytes = read_test_font("MPLUS1-Variable.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("Fraunces-all", |b| {
            let font_bytes = read_test_font("Fraunces.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        });
}

fn benchmark_subset_medium(c: &mut Criterion) {
    c.benchmark_group("subset-medium")
        .bench_function("Roboto-Regular-all", |b| {
            let font_bytes = read_test_font("Roboto-Regular.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("SourceSansPro-Regular-all", |b| {
            let font_bytes = read_test_font("SourceSansPro-Regular.otf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("AdobeVFPrototype-all", |b| {
            let font_bytes = read_test_font("AdobeVFPrototype.otf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        });
}

fn benchmark_subset_slow(c: &mut Criterion) {
    c.benchmark_group("subset-slow")
        .bench_function("NotoNastaliqUrdu-Regular-all", |b| {
            let font_bytes = read_test_font("NotoNastaliqUrdu-Regular.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("NotoSansDevanagari-Regular-all", |b| {
            let font_bytes = read_test_font("NotoSansDevanagari-Regular.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("SourceHanSans-Regular_subset-all", |b| {
            let font_bytes = read_test_font("SourceHanSans-Regular_subset.otf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        })
        .bench_function("RobotoFlex-Variable-all", |b| {
            let font_bytes = read_test_font("RobotoFlex-Variable.ttf");
            b.iter(|| {
                let font = FontRef::new(&font_bytes).unwrap();
                let plan = create_plan(&font, &IntSet::all());
                subset_font(&font, &plan).unwrap()
            });
        });
}

criterion_group!(benches, benchmark_subset);
criterion_group! {
    name = benches_medium;
    config = Criterion::default().measurement_time(Duration::from_secs(10));
    targets = benchmark_subset_medium
}
criterion_group! {
    name = benches_slow;
    config = Criterion::default().measurement_time(Duration::from_secs(30));
    targets = benchmark_subset_slow
}
criterion_main!(benches, benches_medium, benches_slow);
