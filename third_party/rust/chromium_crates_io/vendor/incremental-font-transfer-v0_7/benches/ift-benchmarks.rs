use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion};
use incremental_font_transfer::patch_group::{PatchGroup, UrlStatus};
use incremental_font_transfer::patchmap::{PatchUrl, SubsetDefinition};
use read_fonts::collections::IntSet;
use read_fonts::FontRef;
use std::collections::HashMap;
use std::path::Path;

/// Font bytes, accumulated patch status map, and newly-loaded patch bytes.
type PatchResult = (
    Vec<u8>,
    HashMap<PatchUrl, UrlStatus>,
    HashMap<PatchUrl, Vec<u8>>,
);

struct FontEntry {
    dir: &'static str,
    file: &'static str,
}

const ROBOTO: FontEntry = FontEntry {
    dir: "roboto",
    file: "Roboto-IFT.woff2",
};

const NOTO_SC_HIGH: FontEntry = FontEntry {
    dir: "notosanshigh",
    file: "NotoSansSC-HighFreq-IFT.woff2",
};

const FONTS: &[FontEntry] = &[ROBOTO, NOTO_SC_HIGH];

impl FontEntry {
    fn font_dir(&self) -> std::path::PathBuf {
        Path::new("resources/testdata/fonts").join(self.dir)
    }

    fn font_bytes(&self) -> Vec<u8> {
        let woff2_path = self.font_dir().join(self.file);
        let woff2_bytes = std::fs::read(&woff2_path).unwrap_or_else(|e| {
            let cwd = std::env::current_dir()
                .map(|p| p.display().to_string())
                .unwrap_or_else(|_| "<unknown>".to_string());
            panic!("failed to read font file {woff2_path:?} (cwd: {cwd}): {e}")
        });
        let mut cursor = std::io::Cursor::new(&woff2_bytes);
        woff2_patched::decode::convert_woff2_to_ttf(&mut cursor)
            .unwrap_or_else(|e| panic!("Failed to decode {woff2_path:?} as woff2: {e}"))
    }

    /// Applies all patches needed for `subset`, starting from `start_font` with existing `patch_data`.
    /// Returns `(final_font, updated_patch_data, newly_loaded_patches)`.
    fn apply_patches_for_subset_def(
        &self,
        start_font: Vec<u8>,
        mut patch_data: HashMap<PatchUrl, UrlStatus>,
        subset: &SubsetDefinition,
    ) -> PatchResult {
        let font_dir = self.font_dir();
        let mut newly_loaded: HashMap<PatchUrl, Vec<u8>> = HashMap::new();
        let mut current_font = start_font;
        loop {
            let pg = PatchGroup::select_next_patches(
                FontRef::new(&current_font).unwrap(),
                &patch_data,
                subset,
            )
            .unwrap();
            if !pg.has_urls() {
                break;
            }
            for url in pg.urls() {
                if !patch_data.contains_key(url) {
                    let path = font_dir.join(url.as_ref());
                    let bytes = std::fs::read(&path)
                        .unwrap_or_else(|e| panic!("failed to read patch {path:?}: {e}"));
                    patch_data.insert(url.clone(), UrlStatus::Pending(bytes.clone()));
                    newly_loaded.insert(url.clone(), bytes);
                }
            }
            current_font = pg.apply_next_patches(&mut patch_data).unwrap();
        }
        (current_font, patch_data, newly_loaded)
    }

    /// Applies all patches needed for `chars`, starting from `start_font` with existing `patch_data`.
    /// Returns `(final_font, updated_patch_data, newly_loaded_patches)`.
    fn apply_patches_for_chars(
        &self,
        start_font: Vec<u8>,
        patch_data: HashMap<PatchUrl, UrlStatus>,
        chars: impl Iterator<Item = char>,
    ) -> PatchResult {
        let subset = chars_to_subset(chars);
        self.apply_patches_for_subset_def(start_font, patch_data, &subset)
    }

    /// Pre-loads all patch bytes needed to fully extend the font for `SubsetDefinition::all()`.
    fn preload_patches(&self, font_bytes: &[u8]) -> HashMap<PatchUrl, Vec<u8>> {
        let (_, _, newly_loaded) = self.apply_patches_for_subset_def(
            font_bytes.to_vec(),
            HashMap::new(),
            &SubsetDefinition::all(),
        );
        newly_loaded
    }
}

/// Applies all pending patches to `current_font` for the given subset until no more are needed.
/// Returns the number of patch rounds applied.
fn apply_all_patches(
    current_font: &mut Vec<u8>,
    patch_data: &mut HashMap<PatchUrl, UrlStatus>,
    subset: &SubsetDefinition,
) -> usize {
    let mut count = 0;
    loop {
        let pg = PatchGroup::select_next_patches(
            FontRef::new(current_font).unwrap(),
            patch_data,
            subset,
        )
        .unwrap();
        if !pg.has_urls() {
            break;
        }
        count += 1;
        *current_font = pg.apply_next_patches(patch_data).unwrap();
    }
    count
}

fn chars_to_subset(chars: impl Iterator<Item = char>) -> SubsetDefinition {
    let mut codepoints = IntSet::new();
    codepoints.extend(chars.map(|c| c as u32));
    SubsetDefinition::codepoints(codepoints)
}

fn bench_patch_selection(c: &mut Criterion) {
    let mut group = c.benchmark_group("select-all");

    for entry in FONTS {
        let font_bytes = entry.font_bytes();
        let patch_data: HashMap<PatchUrl, UrlStatus> = HashMap::new();
        let subset = SubsetDefinition::all();
        let url_count = PatchGroup::select_next_patches(
            FontRef::new(&font_bytes).unwrap(),
            &patch_data,
            &subset,
        )
        .unwrap()
        .urls()
        .count();

        group.bench_with_input(
            BenchmarkId::new(entry.dir, format!("{url_count}-patches")),
            &(font_bytes, patch_data, subset),
            |b, (font_bytes, patch_data, subset)| {
                b.iter(|| {
                    let font = FontRef::new(font_bytes).unwrap();
                    PatchGroup::select_next_patches(font, patch_data, subset)
                });
            },
        );
    }
    group.finish();
}

fn bench_apply_patches_all(c: &mut Criterion) {
    let mut group = c.benchmark_group("apply-all");
    let subset = SubsetDefinition::all();

    for entry in FONTS {
        let font_bytes = entry.font_bytes();
        let preloaded = entry.preload_patches(&font_bytes);

        let patch_applications = {
            let mut current_font = font_bytes.clone();
            let mut patch_data: HashMap<PatchUrl, UrlStatus> = preloaded
                .iter()
                .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone())))
                .collect();
            apply_all_patches(&mut current_font, &mut patch_data, &subset)
        };

        group.bench_with_input(
            BenchmarkId::new(entry.dir, format!("{patch_applications}-rounds")),
            &(font_bytes, preloaded),
            |b, (font_bytes, preloaded)| {
                b.iter(|| {
                    let mut current_font = font_bytes.clone();
                    let mut patch_data: HashMap<PatchUrl, UrlStatus> = preloaded
                        .iter()
                        .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone())))
                        .collect();
                    apply_all_patches(&mut current_font, &mut patch_data, &subset);
                    current_font
                });
            },
        );
    }
    group.finish();
}

// 出师表 — opening paragraph
const SC_TEXT_OPENING: &str = "《出师表》〔两汉〕诸葛亮\n先帝创业未半而中道崩殂，今天下三分，益州疲弊，此诚危急存亡之秋也。然侍卫之臣不懈于内，忠志之士忘身于外者，盖追先帝之殊遇，欲报之于陛下也。诚宜开张圣听，以光先帝遗德，恢弘志士之气，不宜妄自菲薄，引喻失义，以塞忠谏之路也。";
// remaining paragraphs (full article)
const SC_TEXT_REMAINING: &str = "宫中府中，俱为一体；陟罚臧否，不宜异同。若有作奸犯科及为忠善者，宜付有司论其刑赏，以昭陛下平明之理，不宜偏私，使内外异法也。侍中、侍郎郭攸之、费祎、董允等，此皆良实，志虑忠纯，是以先帝简拔以遗陛下。愚以为宫中之事，事无大小，悉以咨之，然后施行，必能裨补阙漏，有所广益。将军向宠，性行淑均，晓畅军事，试用于昔日，先帝称之曰能，是以众议举宠为督。愚以为营中之事，悉以咨之，必能使行阵和睦，优劣得所。亲贤臣，远小人，此先汉所以兴隆也；亲小人，远贤臣，此后汉所以倾颓也。先帝在时，每与臣论此事，未尝不叹息痛恨于桓、灵也。侍中、尚书、长史、参军，此悉贞良死节之臣，愿陛下亲之信之，则汉室之隆，可计日而待也。臣本布衣，躬耕于南阳，苟全性命于乱世，不求闻达于诸侯。先帝不以臣卑鄙，猥自枉屈，三顾臣于草庐之中，咨臣以当世之事，由是感激，遂许先帝以驱驰。后值倾覆，受任于败军之际，奉命于危难之间，尔来二十有一年矣。先帝知臣谨慎，故临崩寄臣以大事也。受命以来，夙夜忧叹，恐托付不效，以伤先帝之明；故五月渡泸，深入不毛。今南方已定，兵甲已足，当奖率三军，北定中原，庶竭驽钝，攘除奸凶，兴复汉室，还于旧都。此臣所以报先帝而忠陛下之职分也。至于斟酌损益，进尽忠言，则攸之、祎、允之任也。愿陛下托臣以讨贼兴复之效，不效，则治臣之罪，以告先帝之灵。若无兴德之言，则责攸之、祎、允等之慢，以彰其咎；陛下亦宜自谋，以咨诹善道，察纳雅言，深追先帝遗诏。臣不胜受恩感激。今当远离，临表涕零，不知所言。";

const TEXT_LATIN: &str = "A peep at some distant orb has power to raise and purify our thoughts like a strain of sacred music, or a noble picture, or a passage from the grander poets. It always does one good.";
const TEXT_VIETNAMESE: &str = "Phải áp dụng chế độ giáo dục miễn phí, ít nhất là ở bậc tiểu học và giáo dục cơ sở Chúng tôi đã đạt tới độ cao rất lớn trong khí quyển vì bầu trời tối đen và các vì sao không còn lấp lánh. Ảo giác về đường chân trời khiến đám mây ảm đạm bên dưới lõm xuống và chiếc xe như trôi bồng bềnh giữa quả cầu khổng lồ tăm tối.";
const TEXT_GREEK_CYRILLIC: &str = "Είναι προικισμένοι με λογική και συνείδηση, και οφείλουν να συμπεριφέρονται μεταξύ τους με πνεύμα αδελφοσύνης. Όλοι οι άνθρωποι γεννιούνται ελεύθεροι και ίσοι στην αξιοπρέπεια και τα δικαιώματα. Είναι προικισμένοι με λογική και συνείδηση, και οφείλουν να συμπεριφέρονται μεταξύ τους με πνεύμα αδελφοσύνης. Высокая гравитация выматывала его, но мышцы изо всех сил пытались приспособиться. Обессиленный, он уже не валился в постель сразу после занятий. Кошмары, не покидавшие его, стали только хуже.";

fn bench_apply_patches_incremental(c: &mut Criterion) {
    let roboto_font_bytes = ROBOTO.font_bytes();

    // Precompute font states and patch sets for each step.
    let (font_after_latin, pd_after_latin, patches_latin) = ROBOTO.apply_patches_for_chars(
        roboto_font_bytes.clone(),
        HashMap::new(),
        TEXT_LATIN.chars(),
    );
    let (font_after_viet, pd_after_viet, patches_viet) = ROBOTO.apply_patches_for_chars(
        font_after_latin.clone(),
        pd_after_latin,
        TEXT_LATIN.chars().chain(TEXT_VIETNAMESE.chars()),
    );
    let (_, _, patches_gc) = ROBOTO.apply_patches_for_chars(
        font_after_viet.clone(),
        pd_after_viet,
        TEXT_LATIN
            .chars()
            .chain(TEXT_VIETNAMESE.chars())
            .chain(TEXT_GREEK_CYRILLIC.chars()),
    );

    let mut group = c.benchmark_group(format!("apply-incremental/{}", ROBOTO.dir));
    group.bench_with_input(
        BenchmarkId::from_parameter("initial"),
        &(roboto_font_bytes.clone(), patches_latin.clone()),
        |b, (font_bytes, patches_latin)| {
            b.iter(|| {
                let mut current_font = font_bytes.clone();
                let mut patch_data: HashMap<PatchUrl, UrlStatus> = patches_latin
                    .iter()
                    .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone())))
                    .collect();
                let subset = chars_to_subset(TEXT_LATIN.chars());
                loop {
                    let pg = PatchGroup::select_next_patches(
                        FontRef::new(&current_font).unwrap(),
                        &patch_data,
                        &subset,
                    )
                    .unwrap();
                    if !pg.has_urls() {
                        return current_font;
                    }
                    current_font = pg.apply_next_patches(&mut patch_data).unwrap();
                }
            });
        },
    );
    group.bench_with_input(
        BenchmarkId::from_parameter("+vietnamese"),
        &(
            font_after_latin.clone(),
            patches_latin.clone(),
            patches_viet.clone(),
        ),
        |b, (font_after_latin, patches_latin, patches_viet)| {
            b.iter(|| {
                let mut current_font = font_after_latin.clone();
                let mut patch_data: HashMap<PatchUrl, UrlStatus> = patches_latin
                    .keys()
                    .map(|url| (url.clone(), UrlStatus::Applied))
                    .chain(
                        patches_viet
                            .iter()
                            .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone()))),
                    )
                    .collect();
                let subset = chars_to_subset(TEXT_LATIN.chars().chain(TEXT_VIETNAMESE.chars()));
                loop {
                    let pg = PatchGroup::select_next_patches(
                        FontRef::new(&current_font).unwrap(),
                        &patch_data,
                        &subset,
                    )
                    .unwrap();
                    if !pg.has_urls() {
                        return current_font;
                    }
                    current_font = pg.apply_next_patches(&mut patch_data).unwrap();
                }
            });
        },
    );
    group.bench_with_input(
        BenchmarkId::from_parameter("+greek-cyrillic"),
        &(font_after_viet, patches_latin, patches_viet, patches_gc),
        |b, (font_after_viet, patches_latin, patches_viet, patches_gc)| {
            b.iter(|| {
                let mut current_font = font_after_viet.clone();
                let mut patch_data: HashMap<PatchUrl, UrlStatus> = patches_latin
                    .keys()
                    .chain(patches_viet.keys())
                    .map(|url| (url.clone(), UrlStatus::Applied))
                    .chain(
                        patches_gc
                            .iter()
                            .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone()))),
                    )
                    .collect();
                let subset = chars_to_subset(
                    TEXT_LATIN
                        .chars()
                        .chain(TEXT_VIETNAMESE.chars())
                        .chain(TEXT_GREEK_CYRILLIC.chars()),
                );
                loop {
                    let pg = PatchGroup::select_next_patches(
                        FontRef::new(&current_font).unwrap(),
                        &patch_data,
                        &subset,
                    )
                    .unwrap();
                    if !pg.has_urls() {
                        return current_font;
                    }
                    current_font = pg.apply_next_patches(&mut patch_data).unwrap();
                }
            });
        },
    );
    group.finish();

    let noto_sc_font_bytes = NOTO_SC_HIGH.font_bytes();

    // Precompute font states and patch sets for each step.
    let (font_after_opening, pd_after_opening, patches_opening) = NOTO_SC_HIGH
        .apply_patches_for_chars(
            noto_sc_font_bytes.clone(),
            HashMap::new(),
            SC_TEXT_OPENING.chars(),
        );
    let (_, _, patches_remaining) = NOTO_SC_HIGH.apply_patches_for_chars(
        font_after_opening.clone(),
        pd_after_opening,
        SC_TEXT_OPENING.chars().chain(SC_TEXT_REMAINING.chars()),
    );

    let mut group = c.benchmark_group(format!("incremental/{}", NOTO_SC_HIGH.dir));

    group.bench_with_input(
        BenchmarkId::from_parameter("initial"),
        &(noto_sc_font_bytes, patches_opening.clone()),
        |b, (font_bytes, patches_opening)| {
            b.iter(|| {
                let mut current_font = font_bytes.clone();
                let mut patch_data: HashMap<PatchUrl, UrlStatus> = patches_opening
                    .iter()
                    .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone())))
                    .collect();
                let subset = chars_to_subset(SC_TEXT_OPENING.chars());
                loop {
                    let pg = PatchGroup::select_next_patches(
                        FontRef::new(&current_font).unwrap(),
                        &patch_data,
                        &subset,
                    )
                    .unwrap();
                    if !pg.has_urls() {
                        return current_font;
                    }
                    current_font = pg.apply_next_patches(&mut patch_data).unwrap();
                }
            });
        },
    );
    group.bench_with_input(
        BenchmarkId::from_parameter("+full-article"),
        &(font_after_opening, patches_opening, patches_remaining),
        |b, (font_after_opening, patches_opening, patches_remaining)| {
            b.iter(|| {
                let mut current_font = font_after_opening.clone();
                let mut patch_data: HashMap<PatchUrl, UrlStatus> = patches_opening
                    .keys()
                    .map(|url| (url.clone(), UrlStatus::Applied))
                    .chain(
                        patches_remaining
                            .iter()
                            .map(|(url, bytes)| (url.clone(), UrlStatus::Pending(bytes.clone()))),
                    )
                    .collect();
                let subset =
                    chars_to_subset(SC_TEXT_OPENING.chars().chain(SC_TEXT_REMAINING.chars()));
                loop {
                    let pg = PatchGroup::select_next_patches(
                        FontRef::new(&current_font).unwrap(),
                        &patch_data,
                        &subset,
                    )
                    .unwrap();
                    if !pg.has_urls() {
                        return current_font;
                    }
                    current_font = pg.apply_next_patches(&mut patch_data).unwrap();
                }
            });
        },
    );

    group.finish();
}

criterion_group!(
    benches,
    bench_patch_selection,
    bench_apply_patches_all,
    bench_apply_patches_incremental,
);
criterion_main!(benches);
