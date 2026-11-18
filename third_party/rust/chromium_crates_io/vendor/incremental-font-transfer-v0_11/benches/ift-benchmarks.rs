use criterion::{criterion_group, criterion_main, BatchSize, BenchmarkId, Criterion};
use incremental_font_transfer::patch_group::{PatchGroup, UrlStatus};
use incremental_font_transfer::patchmap::{PatchMap, PatchUrl, SubsetDefinition};
use read_fonts::collections::IntSet;
use read_fonts::FontRef;
use std::collections::HashMap;
use std::path::{Path, PathBuf};

type PatchData = HashMap<PatchUrl, UrlStatus>;

struct FontEntry {
    name: &'static str,
    dir: &'static str,
    file: &'static str,
    initial_text: &'static str,
    additional_text: &'static str,
}

const FONTS: &[FontEntry] = &[
    FontEntry {
        name: "roboto",
        dir: "roboto",
        file: "Roboto-IFT.woff2",
        initial_text: TEXT_LATIN,
        additional_text: TEXT_VIETNAMESE,
    },
    FontEntry {
        name: "notosanshigh",
        dir: "notosanshigh",
        file: "NotoSansSC-HighFreq-IFT.woff2",
        initial_text: SC_TEXT_OPENING,
        additional_text: SC_TEXT_REMAINING,
    },
];

const SC_TEXT_OPENING: &str = "《出师表》〔两汉〕诸葛亮\n先帝创业未半而中道崩殂，今天下三分，益州疲弊，此诚危急存亡之秋也。然侍卫之臣不懈于内，忠志之士忘身于外者，盖追先帝之殊遇，欲报之于陛下也。诚宜开张圣听，以光先帝遗德，恢弘志士之气，不宜妄自菲薄，引喻失义，以塞忠谏之路也。";
const SC_TEXT_REMAINING: &str = "宫中府中，俱为一体；陟罚臧否，不宜异同。若有作奸犯科及为忠善者，宜付有司论其刑赏，以昭陛下平明之理，不宜偏私，使内外异法也。侍中、侍郎郭攸之、费祎、董允等，此皆良实，志虑忠纯，是以先帝简拔以遗陛下。愚以为宫中之事，事无大小，悉以咨之，然后施行，必能裨补阙漏，有所广益。将军向宠，性行淑均，晓畅军事，试用于昔日，先帝称之曰能，是以众议举宠为督。愚以为营中之事，悉以咨之，必能使行阵和睦，优劣得所。亲贤臣，远小人，此先汉所以兴隆也；亲小人，远贤臣，此后汉所以倾颓也。先帝在时，每与臣论此事，未尝不叹息痛恨于桓、灵也。侍中、尚书、长史、参军，此悉贞良死节之臣，愿陛下亲之信之，则汉室之隆，可计日而待也。臣本布衣，躬耕于南阳，苟全性命于乱世，不求闻达于诸侯。先帝不以臣卑鄙，猥自枉屈，三顾臣于草庐之中，咨臣以当世之事，由是感激，遂许先帝以驱驰。后值倾覆，受任于败军之际，奉命于危难之间，尔来二十有一年矣。先帝知臣谨慎，故临崩寄臣以大事也。受命以来，夙夜忧叹，恐托付不效，以伤先帝之明；故五月渡泸，深入不毛。今南方已定，兵甲已足，当奖率三军，北定中原，庶竭驽钝，攘除奸凶，兴复汉室，还于旧都。此臣所以报先帝而忠陛下之职分也。至于斟酌损益，进尽忠言，则攸之、祎、允之任也。愿陛下托臣以讨贼兴复之效，不效，则治臣之罪，以告先帝之灵。若无兴德之言，则责攸之、祎、允等之慢，以彰其咎；陛下亦宜自谋，以咨诹善道，察纳雅言，深追先帝遗诏。臣不胜受恩感激。今当远离，临表涕零，不知所言。";
const TEXT_LATIN: &str = "A peep at some distant orb has power to raise and purify our thoughts like a strain of sacred music, or a noble picture, or a passage from the grander poets. It always does one good.";
const TEXT_VIETNAMESE: &str = "Phải áp dụng chế độ giáo dục miễn phí, ít nhất là ở bậc tiểu học và giáo dục cơ sở Chúng tôi đã đạt tới độ cao rất lớn trong khí quyển vì bầu trời tối đen và các vì sao không còn lấp lánh. Ảo giác về đường chân trời khiến đám mây ảm đạm bên dưới lõm xuống và chiếc xe như trôi bồng bềnh giữa quả cầu khổng lồ tăm tối.";

struct ExpansionRequest {
    initial_font_bytes: Vec<u8>,
    patch_data: PatchData,
    subset: SubsetDefinition,
}

impl FontEntry {
    fn initial_subset(&self) -> SubsetDefinition {
        SubsetDefinition::codepoints(IntSet::from_iter(self.initial_text.chars().map(u32::from)))
    }

    fn incremental_subset(&self) -> SubsetDefinition {
        let chars = self
            .initial_text
            .chars()
            .chain(self.additional_text.chars());
        SubsetDefinition::codepoints(IntSet::from_iter(chars.map(u32::from)))
    }

    fn font_dir(&self) -> PathBuf {
        Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources/testdata/fonts")
            .join(self.dir)
    }

    fn font_bytes(&self) -> Vec<u8> {
        let path = self.font_dir().join(self.file);
        let bytes =
            std::fs::read(&path).unwrap_or_else(|e| panic!("failed to read font {path:?}: {e}"));
        woff2_patched::decode::convert_woff2_to_ttf(&mut std::io::Cursor::new(&bytes))
            .unwrap_or_else(|e| panic!("failed to decode font {path:?}: {e}"))
    }

    /// Runs an expansion outside timing to discover its patches and validate its result.
    /// Retains starting URL statuses while adding newly loaded patches as pending.
    fn prepare_expansion_request(
        &self,
        font_bytes: Vec<u8>,
        patch_data: PatchData,
        subset: SubsetDefinition,
    ) -> (ExpansionRequest, Vec<u8>, PatchData) {
        let mut expansion = ExpansionRequest {
            initial_font_bytes: font_bytes,
            patch_data,
            subset,
        };
        let mut final_font = expansion.initial_font_bytes.clone();
        let mut final_patch_data = expansion.patch_data.clone();
        let needs_patches = PatchMap::new(&FontRef::new(&expansion.initial_font_bytes).unwrap())
            .unwrap()
            .has_intersecting_patches(&expansion.subset);
        let rounds = apply_all_patches(
            &mut final_font,
            &mut final_patch_data,
            &expansion.subset,
            |group, patch_data| {
                for url in group.urls() {
                    if !patch_data.contains_key(url) {
                        let path = self.font_dir().join(url.as_ref());
                        let bytes = std::fs::read(&path)
                            .unwrap_or_else(|e| panic!("failed to read patch {path:?}: {e}"));
                        expansion
                            .patch_data
                            .insert(url.clone(), UrlStatus::Pending(bytes.clone()));
                        patch_data.insert(url.clone(), UrlStatus::Pending(bytes));
                    }
                }
            },
        );
        assert_eq!(rounds > 0, needs_patches);
        let final_map = PatchMap::new(&FontRef::new(&final_font).unwrap()).unwrap();
        assert!(!final_map.has_intersecting_patches(&expansion.subset));
        (expansion, final_font, final_patch_data)
    }
}

fn apply_all_patches(
    current_font: &mut Vec<u8>,
    patch_data: &mut PatchData,
    subset: &SubsetDefinition,
    mut prepare_group: impl FnMut(&PatchGroup<'_>, &mut PatchData),
) -> usize {
    let mut rounds = 0;
    loop {
        let group = PatchGroup::select_next_patches(
            FontRef::new(current_font).unwrap(),
            patch_data,
            subset,
        )
        .unwrap();
        if !group.has_urls() {
            return rounds;
        }
        prepare_group(&group, patch_data);
        *current_font = group.apply_next_patches(patch_data).unwrap();
        rounds += 1;
    }
}

fn bench_patch_map_new(c: &mut Criterion) {
    for entry in FONTS {
        let base_bytes = entry.font_bytes();
        let base_font = FontRef::new(&base_bytes).unwrap();

        c.bench_with_input(
            BenchmarkId::new("patch-map/new", entry.name),
            &base_font,
            |b, font| {
                b.iter(|| PatchMap::new(font).unwrap());
            },
        );
    }
}

// We test the `false` case of `has_intersecting_patches` as:
// - The true case may short circuit early which should be very fast.
// - Most runs should trigger the false case once the font is loaded.
//   This is a performance bottleneck.
fn bench_has_intersecting_patches_false(c: &mut Criterion) {
    for entry in FONTS {
        let initial_subset = entry.initial_subset();
        let (_, satisfied_bytes, _) = entry.prepare_expansion_request(
            entry.font_bytes(),
            PatchData::new(),
            initial_subset.clone(),
        );
        let satisfied_map = PatchMap::new(&FontRef::new(&satisfied_bytes).unwrap()).unwrap();
        assert!(!satisfied_map.has_intersecting_patches(&initial_subset));

        c.bench_with_input(
            BenchmarkId::new("patch-map/has-intersecting/false", entry.name),
            &(&satisfied_map, &initial_subset),
            |b, &(map, subset)| {
                b.iter(|| map.has_intersecting_patches(subset));
            },
        );
    }
}

fn bench_intersecting_patches_new_subset(c: &mut Criterion) {
    for entry in FONTS {
        let (_, satisfied_bytes, _) = entry.prepare_expansion_request(
            entry.font_bytes(),
            PatchData::new(),
            entry.initial_subset(),
        );
        let satisfied_map = PatchMap::new(&FontRef::new(&satisfied_bytes).unwrap()).unwrap();
        let incremental_subset = entry.incremental_subset();
        assert!(!satisfied_map
            .intersecting_patches(&incremental_subset)
            .unwrap()
            .is_empty());

        c.bench_with_input(
            BenchmarkId::new("patch-map/intersecting/new-subset", entry.name),
            &(&satisfied_map, &incremental_subset),
            |b, &(map, subset)| {
                b.iter(|| map.intersecting_patches(subset).unwrap());
            },
        );
    }
}

fn bench_intersecting_patches_all(c: &mut Criterion) {
    for entry in FONTS {
        let base_bytes = entry.font_bytes();
        let base_map = PatchMap::new(&FontRef::new(&base_bytes).unwrap()).unwrap();
        let subset = SubsetDefinition::all();
        assert!(!base_map.intersecting_patches(&subset).unwrap().is_empty());

        c.bench_with_input(
            BenchmarkId::new("patch-map/intersecting/all", entry.name),
            &(&base_map, &subset),
            |b, &(map, subset)| {
                b.iter(|| map.intersecting_patches(subset).unwrap());
            },
        );
    }
}

fn bench_apply_patches_incremental(c: &mut Criterion) {
    for entry in FONTS {
        let (_, satisfied_bytes, applied_statuses) = entry.prepare_expansion_request(
            entry.font_bytes(),
            PatchData::new(),
            entry.initial_subset(),
        );
        let (expansion, _, _) = entry.prepare_expansion_request(
            satisfied_bytes,
            applied_statuses,
            entry.incremental_subset(),
        );

        c.bench_with_input(
            BenchmarkId::new("apply-patches/incremental", entry.name),
            &expansion,
            |b, expansion| {
                b.iter_batched(
                    || {
                        (
                            expansion.initial_font_bytes.clone(),
                            expansion.patch_data.clone(),
                        )
                    },
                    |(mut font_bytes, mut patch_data)| {
                        let rounds = apply_all_patches(
                            &mut font_bytes,
                            &mut patch_data,
                            &expansion.subset,
                            |_, _| {},
                        );
                        (rounds, font_bytes, patch_data)
                    },
                    BatchSize::LargeInput,
                );
            },
        );
    }
}

fn bench_apply_patches_all(c: &mut Criterion) {
    for entry in FONTS {
        let (expansion, _, _) = entry.prepare_expansion_request(
            entry.font_bytes(),
            PatchData::new(),
            SubsetDefinition::all(),
        );

        c.bench_with_input(
            BenchmarkId::new("apply-patches/all", entry.name),
            &expansion,
            |b, expansion| {
                b.iter_batched(
                    || {
                        (
                            expansion.initial_font_bytes.clone(),
                            expansion.patch_data.clone(),
                        )
                    },
                    |(mut font_bytes, mut patch_data)| {
                        let rounds = apply_all_patches(
                            &mut font_bytes,
                            &mut patch_data,
                            &expansion.subset,
                            |_, _| {},
                        );
                        (rounds, font_bytes, patch_data)
                    },
                    BatchSize::LargeInput,
                );
            },
        );
    }
}

criterion_group!(
    benches,
    bench_patch_map_new,
    bench_has_intersecting_patches_false,
    bench_intersecting_patches_new_subset,
    bench_intersecting_patches_all,
    bench_apply_patches_incremental,
    bench_apply_patches_all,
);
criterion_main!(benches);
