//! OpenType layout metadata, lookup glyph collection, and BASE baseline queries.

use core::cell::Cell;
use core::ffi::c_uint;
use std::collections::HashSet;

use harfrust::Tag;
use read_fonts::tables::base::{BaseAxis, BaseInstance};
use read_fonts::tables::gpos::{
    CursivePos, MarkBasePos, MarkLigPos, MarkMarkPos, PairPos, PositionSubtables,
};
use read_fonts::tables::gsub::{
    AlternateSubst, LigatureSubst, MultipleSubst, ReverseChainSingleSubst, SingleSubst,
    SubstitutionLookupList, SubstitutionSubtables,
};
use read_fonts::tables::layout::{
    ChainedSequenceContext, ClassDef, CoverageTable, FeatureList, ScriptList, SequenceContext,
    SequenceLookupRecord,
};
use read_fonts::types::F48Dot16;
use read_fonts::{ReadError, TableProvider};

use crate::common::hr_glyph_extents_t;
use crate::common::{
    hr_bool_t, hr_direction_t, hr_language_t, hr_position_t, hr_script_t, hr_tag_t, ot_round,
    script_to_rust, tag_from_rust, tag_to_rust, HR_DIRECTION_LTR, HR_DIRECTION_RTL,
};
use crate::face::hr_face_t;
use crate::font::{hr_font_get_glyph_extents, hr_font_get_nominal_glyph, hr_font_t};
use crate::object;
use crate::set::hr_set_t;

/// The OpenType substitution table tag.
pub const HR_OT_TAG_GSUB: hr_tag_t = 0x4753_5542u32;
/// The OpenType positioning table tag.
pub const HR_OT_TAG_GPOS: hr_tag_t = 0x4750_4F53u32;
/// The default OpenType script tag, `DFLT`.
pub const HR_OT_TAG_DEFAULT_SCRIPT: hr_tag_t = 0x4446_4C54u32;
/// The default OpenType language tag, `dflt`.
pub const HR_OT_TAG_DEFAULT_LANGUAGE: hr_tag_t = 0x6466_6C74u32;
/// No script was selected.
pub const HR_OT_LAYOUT_NO_SCRIPT_INDEX: c_uint = 0xFFFFu32;
/// No feature was found.
pub const HR_OT_LAYOUT_NO_FEATURE_INDEX: c_uint = 0xFFFFu32;
/// Selects a script's default language system.
pub const HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX: c_uint = 0xFFFFu32;

const GSUB_TAG: hr_tag_t = HR_OT_TAG_GSUB;
const GPOS_TAG: hr_tag_t = HR_OT_TAG_GPOS;

fn script_list(face: &hr_face_t, table_tag: hr_tag_t) -> Option<ScriptList<'_>> {
    let font = face.font()?;
    match table_tag {
        GSUB_TAG => font.tables().gsub().ok()?.script_list().ok(),
        GPOS_TAG => font.tables().gpos().ok()?.script_list().ok(),
        _ => None,
    }
}

fn feature_list(face: &hr_face_t, table_tag: hr_tag_t) -> Option<FeatureList<'_>> {
    let font = face.font()?;
    match table_tag {
        GSUB_TAG => font.tables().gsub().ok()?.feature_list().ok(),
        GPOS_TAG => font.tables().gpos().ok()?.feature_list().ok(),
        _ => None,
    }
}

/// Selects the first available requested script in GSUB or GPOS.
///
/// If none matches, tries `DFLT`, `dflt`, then `latn`. Returns true only for
/// a requested script, even when a fallback was selected. With no match,
/// writes `HR_OT_LAYOUT_NO_SCRIPT_INDEX` and a zero tag to the outputs.
///
/// # Safety
/// `face` must be null or live; `script_tags` must hold `script_count` tags
/// when non-null. Both output pointers must be null or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_table_select_script(
    face: *mut hr_face_t,
    table_tag: hr_tag_t,
    script_count: c_uint,
    script_tags: *const hr_tag_t,
    script_index: *mut c_uint,
    chosen_script: *mut hr_tag_t,
) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let list = script_list(face, table_tag);
    let records = list.as_ref().map_or(&[][..], |list| list.script_records());
    let requested = if script_tags.is_null() {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(script_tags, script_count as usize) }
    };
    let fallbacks = [
        HR_OT_TAG_DEFAULT_SCRIPT,
        HR_OT_TAG_DEFAULT_LANGUAGE,
        u32::from_be_bytes(*b"latn"),
    ];
    let selected = requested
        .iter()
        .map(|tag| (*tag, true))
        .chain(fallbacks.into_iter().map(|tag| (tag, false)))
        .find_map(|(tag, requested)| {
            records
                .binary_search_by_key(&tag_to_rust(tag), |record| record.script_tag())
                .ok()
                .map(|index| (index as c_uint, tag, requested))
        });
    let (index, tag, requested) = selected.unwrap_or((HR_OT_LAYOUT_NO_SCRIPT_INDEX, 0, false));
    if let Some(output) = unsafe { script_index.as_mut() } {
        *output = index;
    }
    if let Some(output) = unsafe { chosen_script.as_mut() } {
        *output = tag;
    }
    requested.into()
}

/// Returns the total number of feature records in GSUB or GPOS.
///
/// Copies tags starting at `start_offset`, including duplicates. When both
/// array and count are non-null, the count gives capacity on entry and the
/// number written on return. A null array leaves the count unchanged.
///
/// # Safety
/// `face` must be null or live; `feature_count` must be null or writable;
/// `feature_tags` must hold the input capacity when non-null.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_table_get_feature_tags(
    face: *mut hr_face_t,
    table_tag: hr_tag_t,
    start_offset: c_uint,
    feature_count: *mut c_uint,
    feature_tags: *mut hr_tag_t,
) -> c_uint {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let list = feature_list(face, table_tag);
    let records = list.as_ref().map_or(&[][..], |list| list.feature_records());
    if !feature_tags.is_null() {
        if let Some(count) = unsafe { feature_count.as_mut() } {
            let mut written = 0;
            for record in records
                .iter()
                .skip(start_offset as usize)
                .take(*count as usize)
            {
                unsafe {
                    feature_tags
                        .add(written)
                        .write(tag_from_rust(record.feature_tag()));
                };
                written += 1;
            }
            *count = written as c_uint;
        }
    }
    records.len() as c_uint
}

/// Finds an optional feature in a script's language system in GSUB or GPOS.
///
/// Use `HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX` for the default language system.
/// A required feature is not included in this search. Writes the feature's
/// table-wide index, or `HR_OT_LAYOUT_NO_FEATURE_INDEX` on failure.
///
/// # Safety
/// `face` must be null or live; `feature_index` must be null or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_language_find_feature(
    face: *mut hr_face_t,
    table_tag: hr_tag_t,
    script_index: c_uint,
    language_index: c_uint,
    feature_tag: hr_tag_t,
    feature_index: *mut c_uint,
) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let found = (|| {
        let scripts = script_list(face, table_tag)?;
        let script = scripts
            .script_records()
            .get(script_index as usize)?
            .script(scripts.offset_data())
            .ok()?;
        let language = if language_index == HR_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX {
            script.default_lang_sys()?.ok()?
        } else {
            script
                .lang_sys_records()
                .get(language_index as usize)?
                .lang_sys(script.offset_data())
                .ok()?
        };
        let features = feature_list(face, table_tag)?;
        language.feature_indices().iter().find_map(|index| {
            let index = index.get() as usize;
            let tag = features
                .feature_records()
                .get(index)
                .map_or(0, |record| tag_from_rust(record.feature_tag()));
            (tag == feature_tag).then_some(index as c_uint)
        })
    })();
    if let Some(output) = unsafe { feature_index.as_mut() } {
        *output = found.unwrap_or(HR_OT_LAYOUT_NO_FEATURE_INDEX);
    }
    found.is_some().into()
}

/// Returns whether the face has a readable GSUB table.
///
/// # Safety
/// `face` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_has_substitution(face: *mut hr_face_t) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    face.font()
        .is_some_and(|font| font.tables().gsub().is_ok())
        .into()
}

/// Returns whether the face has a readable GPOS table.
///
/// # Safety
/// `face` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_has_positioning(face: *mut hr_face_t) -> hr_bool_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    face.font()
        .is_some_and(|font| font.tables().gpos().is_ok())
        .into()
}

/// Returns the number of lookups in GSUB or GPOS.
///
/// # Safety
/// `face` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_table_get_lookup_count(
    face: *mut hr_face_t,
    table_tag: hr_tag_t,
) -> c_uint {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let Some(font) = face.font() else { return 0 };
    match table_tag {
        GSUB_TAG => font
            .tables()
            .gsub()
            .ok()
            .and_then(|table| table.lookup_list().ok())
            .map_or(0, |list| list.lookups().len() as c_uint),
        GPOS_TAG => font
            .tables()
            .gpos()
            .ok()
            .and_then(|table| table.lookup_list().ok())
            .map_or(0, |list| list.lookups().len() as c_uint),
        _ => 0,
    }
}

#[derive(Clone, Copy)]
struct SetTarget(*mut hr_set_t);

impl SetTarget {
    fn add(self, glyph: u32) {
        if let Some(set) = unsafe { object::as_mutable(self.0) } {
            set.add(glyph);
        }
    }

    fn include_all_glyphs(self) {
        if let Some(set) = unsafe { object::as_mutable(self.0) } {
            // An inverted empty set represents a conservative superset in
            // constant space, including IDs not backed by MAXP or metrics.
            set.values.clear();
            set.values.invert();
        }
    }
}

/// The steps one glyph collection call may take on a table.
///
/// Offsets may alias, so nested arrays of them can name far more data than the
/// table holds, and a lookup list may name one such lookup many times.
/// HarfBuzz's sanitizer bounds such aliasing once, when it loads the table,
/// with a work allowance proportional to the table's size; HarfRust reads the
/// table lazily, so every loop charges its steps here instead, and each call
/// gets an allowance proportional to the table's size. Running out takes the
/// every-glyph fallback.
struct Budget<'a> {
    limit: usize,
    spent: &'a Cell<usize>,
}

impl<'a> Budget<'a> {
    // The allowance the shaper gives a layout table's preprocessing.
    fn limit(bytes: usize) -> usize {
        bytes.saturating_mul(256).clamp(4 << 20, 128 << 20)
    }

    fn for_table(bytes: usize, spent: &'a Cell<usize>) -> Self {
        Self {
            limit: Self::limit(bytes),
            spent,
        }
    }

    fn work(&self) -> Result<(), ReadError> {
        let spent = self.spent.get();
        if spent >= self.limit {
            return Err(ReadError::MalformedData("lookup glyph collection ran out"));
        }
        self.spent.set(spent + 1);
        Ok(())
    }
}

/// The reader expands range arrays with `flat_map`: reversed ranges yield
/// no glyphs, but their array entries still have to be visited.
fn charge_coverage_ranges(coverage: &CoverageTable<'_>, budget: &Budget) -> Result<(), ReadError> {
    let count = match coverage {
        CoverageTable::Format2(table) => table.range_records().len(),
        CoverageTable::Format4(table) => table.range_records().len(),
        _ => 0,
    };
    for _ in 0..count {
        budget.work()?;
    }
    Ok(())
}

/// Charge records hidden by the reader's range expansion or wide-glyph
/// filtering as well as the glyphs yielded by its iterator below.
fn charge_class_entries(class_def: &ClassDef<'_>, budget: &Budget) -> Result<(), ReadError> {
    let count = match class_def {
        ClassDef::Format1(_) => 0,
        ClassDef::Format2(table) => table.class_range_records().len(),
        ClassDef::Format3(table) => table.class_value_array().len(),
        ClassDef::Format4(table) => table.class_range_records().len(),
    };
    for _ in 0..count {
        budget.work()?;
    }
    Ok(())
}

fn add_coverage(
    set: SetTarget,
    coverage: CoverageTable<'_>,
    budget: &Budget,
) -> Result<(), ReadError> {
    charge_coverage_ranges(&coverage, budget)?;
    for glyph in coverage.iter() {
        budget.work()?;
        set.add(glyph.to_u32());
    }
    Ok(())
}

fn add_class_coverage(
    set: SetTarget,
    class_def: ClassDef<'_>,
    budget: &Budget,
) -> Result<(), ReadError> {
    charge_class_entries(&class_def, budget)?;
    match class_def {
        ClassDef::Format1(table) => {
            let start = table.start_glyph_id().to_u32();
            let mut previous_nonzero = false;
            for (index, class) in table.class_value_array().iter().enumerate() {
                budget.work()?;
                let nonzero = class.get() != 0;
                if nonzero || previous_nonzero {
                    set.add(start + index as u32);
                }
                previous_nonzero = nonzero;
            }
            if previous_nonzero {
                set.add(start + table.class_value_array().len() as u32);
            }
        }
        ClassDef::Format2(table) => {
            for (glyph, class) in table.iter() {
                budget.work()?;
                if class != 0 {
                    set.add(glyph.to_u32());
                }
            }
        }
        ClassDef::Format3(_) | ClassDef::Format4(_) => {
            for (glyph, class) in class_def.iter() {
                budget.work()?;
                if class != 0 {
                    set.add(glyph.to_u32());
                }
            }
        }
    }
    Ok(())
}

fn add_class(
    set: SetTarget,
    class_def: &ClassDef<'_>,
    selected: u16,
    budget: &Budget,
) -> Result<(), ReadError> {
    charge_class_entries(class_def, budget)?;
    for (glyph, class) in class_def.iter() {
        budget.work()?;
        if class == u32::from(selected) {
            set.add(glyph.to_u32());
        }
    }
    Ok(())
}

fn collect_nested_substitutions(
    records: &[SequenceLookupRecord],
    lookup_list: &SubstitutionLookupList<'_>,
    output: SetTarget,
    budget: &Budget,
    nested_seen: &mut HashSet<(u16, u8)>,
    depth: u8,
) -> Result<bool, ReadError> {
    for record in records {
        budget.work()?;
        let index = record.lookup_list_index();
        // A lookup collected at this depth before added the same output.
        if !nested_seen.insert((index, depth)) {
            continue;
        }
        let nested = lookup_list.lookups().get(index as usize)?;
        if !collect_substitution(
            nested,
            lookup_list,
            SetTarget(core::ptr::null_mut()),
            SetTarget(core::ptr::null_mut()),
            SetTarget(core::ptr::null_mut()),
            output,
            budget,
            nested_seen,
            depth + 1,
        )? {
            return Ok(false);
        }
    }
    Ok(true)
}

#[allow(clippy::too_many_arguments)]
fn collect_substitution(
    lookup: read_fonts::tables::gsub::SubstitutionLookup<'_>,
    lookup_list: &SubstitutionLookupList<'_>,
    before: SetTarget,
    input: SetTarget,
    after: SetTarget,
    output: SetTarget,
    budget: &Budget,
    nested_seen: &mut HashSet<(u16, u8)>,
    depth: u8,
) -> Result<bool, ReadError> {
    if depth >= 8 {
        return Ok(false);
    }
    // Extension type discovery scans offsets with find_map before returning
    // subtables, so unreadable extension headers must be charged up front.
    if let read_fonts::tables::gsub::SubstitutionLookup::Extension(table) = &lookup {
        for _ in 0..table.subtable_offsets().len() {
            budget.work()?;
        }
    }
    match lookup.subtables()? {
        SubstitutionSubtables::Single(subtables) => {
            for subtable in subtables.iter() {
                budget.work()?;
                match subtable? {
                    SingleSubst::Format1(table) => {
                        let coverage = table.coverage()?;
                        charge_coverage_ranges(&coverage, budget)?;
                        for glyph in coverage.iter() {
                            budget.work()?;
                            let id = glyph.to_u32();
                            input.add(id);
                            output.add(
                                (id as u16).wrapping_add_signed(table.delta_glyph_id()) as u32,
                            );
                        }
                    }
                    SingleSubst::Format2(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        for glyph in table.substitute_glyph_ids() {
                            budget.work()?;
                            output.add(glyph.get().to_u32());
                        }
                    }
                    SingleSubst::Format3(table) => {
                        let coverage = table.coverage()?;
                        charge_coverage_ranges(&coverage, budget)?;
                        for glyph in coverage.iter() {
                            budget.work()?;
                            let id = glyph.to_u32();
                            input.add(id);
                            output.add(
                                id.wrapping_add_signed(table.delta_glyph_id().to_i32())
                                    & 0x00FF_FFFF,
                            );
                        }
                    }
                    SingleSubst::Format4(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        for glyph in table.substitute_glyph_ids() {
                            budget.work()?;
                            output.add(glyph.get().to_u32());
                        }
                    }
                }
            }
        }
        SubstitutionSubtables::Multiple(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                // The extended formats fall back to every glyph.
                let MultipleSubst::Format1(table) = table? else {
                    return Ok(false);
                };
                add_coverage(input, table.coverage()?, budget)?;
                for sequence in table.sequences().iter() {
                    budget.work()?;
                    for glyph in sequence?.substitute_glyph_ids() {
                        budget.work()?;
                        output.add(glyph.get().to_u32());
                    }
                }
            }
        }
        SubstitutionSubtables::Alternate(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let AlternateSubst::Format1(table) = table? else {
                    return Ok(false);
                };
                add_coverage(input, table.coverage()?, budget)?;
                for alternate in table.alternate_sets().iter() {
                    budget.work()?;
                    for glyph in alternate?.alternate_glyph_ids() {
                        budget.work()?;
                        output.add(glyph.get().to_u32());
                    }
                }
            }
        }
        SubstitutionSubtables::Ligature(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let LigatureSubst::Format1(table) = table? else {
                    return Ok(false);
                };
                add_coverage(input, table.coverage()?, budget)?;
                for set in table.ligature_sets().iter() {
                    budget.work()?;
                    for ligature in set?.ligatures().iter() {
                        budget.work()?;
                        let ligature = ligature?;
                        output.add(ligature.ligature_glyph().to_u32());
                        for glyph in ligature.component_glyph_ids() {
                            budget.work()?;
                            input.add(glyph.get().to_u32());
                        }
                    }
                }
            }
        }
        SubstitutionSubtables::Reverse(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let ReverseChainSingleSubst::Format1(table) = table? else {
                    return Ok(false);
                };
                add_coverage(input, table.coverage()?, budget)?;
                for coverage in table.backtrack_coverages().iter() {
                    budget.work()?;
                    add_coverage(before, coverage?, budget)?;
                }
                for coverage in table.lookahead_coverages().iter() {
                    budget.work()?;
                    add_coverage(after, coverage?, budget)?;
                }
                for glyph in table.substitute_glyph_ids() {
                    budget.work()?;
                    output.add(glyph.get().to_u32());
                }
            }
        }
        SubstitutionSubtables::Contextual(subtables) => {
            for subtable in subtables.iter() {
                budget.work()?;
                match subtable? {
                    SequenceContext::Format1(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        for rule_set in table.seq_rule_sets().iter() {
                            // Null offsets still consume an array visit.
                            budget.work()?;
                            let Some(rule_set) = rule_set else {
                                continue;
                            };
                            for rule in rule_set?.seq_rules().iter() {
                                budget.work()?;
                                let rule = rule?;
                                for glyph in rule.input_sequence() {
                                    budget.work()?;
                                    input.add(glyph.get().to_u32());
                                }
                                if !collect_nested_substitutions(
                                    rule.seq_lookup_records(),
                                    lookup_list,
                                    output,
                                    budget,
                                    nested_seen,
                                    depth,
                                )? {
                                    return Ok(false);
                                }
                            }
                        }
                    }
                    SequenceContext::Format2(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        let class_def = table.class_def()?;
                        for rule_set in table.class_seq_rule_sets().iter() {
                            // Null offsets still consume an array visit.
                            budget.work()?;
                            let Some(rule_set) = rule_set else {
                                continue;
                            };
                            for rule in rule_set?.class_seq_rules().iter() {
                                budget.work()?;
                                let rule = rule?;
                                for class in rule.input_sequence() {
                                    budget.work()?;
                                    add_class(input, &class_def, class.get(), budget)?;
                                }
                                if !collect_nested_substitutions(
                                    rule.seq_lookup_records(),
                                    lookup_list,
                                    output,
                                    budget,
                                    nested_seen,
                                    depth,
                                )? {
                                    return Ok(false);
                                }
                            }
                        }
                    }
                    SequenceContext::Format3(table) => {
                        for coverage in table.coverages().iter() {
                            budget.work()?;
                            add_coverage(input, coverage?, budget)?;
                        }
                        if !collect_nested_substitutions(
                            table.seq_lookup_records(),
                            lookup_list,
                            output,
                            budget,
                            nested_seen,
                            depth,
                        )? {
                            return Ok(false);
                        }
                    }
                    _ => return Ok(false),
                }
            }
        }
        SubstitutionSubtables::ChainContextual(subtables) => {
            for subtable in subtables.iter() {
                budget.work()?;
                match subtable? {
                    ChainedSequenceContext::Format1(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        for rule_set in table.chained_seq_rule_sets().iter() {
                            // Null offsets still consume an array visit.
                            budget.work()?;
                            let Some(rule_set) = rule_set else {
                                continue;
                            };
                            for rule in rule_set?.chained_seq_rules().iter() {
                                budget.work()?;
                                let rule = rule?;
                                for glyph in rule.backtrack_sequence() {
                                    budget.work()?;
                                    before.add(glyph.get().to_u32());
                                }
                                for glyph in rule.input_sequence() {
                                    budget.work()?;
                                    input.add(glyph.get().to_u32());
                                }
                                for glyph in rule.lookahead_sequence() {
                                    budget.work()?;
                                    after.add(glyph.get().to_u32());
                                }
                                if !collect_nested_substitutions(
                                    rule.seq_lookup_records(),
                                    lookup_list,
                                    output,
                                    budget,
                                    nested_seen,
                                    depth,
                                )? {
                                    return Ok(false);
                                }
                            }
                        }
                    }
                    ChainedSequenceContext::Format2(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        let backtrack_class_def = table.backtrack_class_def()?;
                        let input_class_def = table.input_class_def()?;
                        let lookahead_class_def = table.lookahead_class_def()?;
                        for rule_set in table.chained_class_seq_rule_sets().iter() {
                            // Null offsets still consume an array visit.
                            budget.work()?;
                            let Some(rule_set) = rule_set else {
                                continue;
                            };
                            for rule in rule_set?.chained_class_seq_rules().iter() {
                                budget.work()?;
                                let rule = rule?;
                                for class in rule.backtrack_sequence() {
                                    budget.work()?;
                                    add_class(before, &backtrack_class_def, class.get(), budget)?;
                                }
                                for class in rule.input_sequence() {
                                    budget.work()?;
                                    add_class(input, &input_class_def, class.get(), budget)?;
                                }
                                for class in rule.lookahead_sequence() {
                                    budget.work()?;
                                    add_class(after, &lookahead_class_def, class.get(), budget)?;
                                }
                                if !collect_nested_substitutions(
                                    rule.seq_lookup_records(),
                                    lookup_list,
                                    output,
                                    budget,
                                    nested_seen,
                                    depth,
                                )? {
                                    return Ok(false);
                                }
                            }
                        }
                    }
                    ChainedSequenceContext::Format3(table) => {
                        let mut coverages = table.input_coverages().iter();
                        add_coverage(
                            input,
                            coverages.next().ok_or(ReadError::OutOfBounds)??,
                            budget,
                        )?;
                        for coverage in coverages {
                            budget.work()?;
                            add_coverage(input, coverage?, budget)?;
                        }
                        for coverage in table.backtrack_coverages().iter() {
                            budget.work()?;
                            add_coverage(before, coverage?, budget)?;
                        }
                        for coverage in table.lookahead_coverages().iter() {
                            budget.work()?;
                            add_coverage(after, coverage?, budget)?;
                        }
                        if !collect_nested_substitutions(
                            table.seq_lookup_records(),
                            lookup_list,
                            output,
                            budget,
                            nested_seen,
                            depth,
                        )? {
                            return Ok(false);
                        }
                    }
                    _ => return Ok(false),
                }
            }
        }
        SubstitutionSubtables::EmptyExtension => {}
    }
    Ok(true)
}

fn collect_positioning(
    lookup: read_fonts::tables::gpos::PositionLookup<'_>,
    input: SetTarget,
    budget: &Budget,
) -> Result<bool, ReadError> {
    // As for GSUB, extension type discovery can filter unreadable offsets.
    if let read_fonts::tables::gpos::PositionLookup::Extension(table) = &lookup {
        for _ in 0..table.subtable_offsets().len() {
            budget.work()?;
        }
    }
    match lookup.subtables()? {
        PositionSubtables::Single(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                match table? {
                    read_fonts::tables::gpos::SinglePos::Format1(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                    }
                    read_fonts::tables::gpos::SinglePos::Format2(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                    }
                    read_fonts::tables::gpos::SinglePos::Format3(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                    }
                    read_fonts::tables::gpos::SinglePos::Format4(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                    }
                }
            }
        }
        PositionSubtables::Pair(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                match table? {
                    PairPos::Format1(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        for pair_set in table.pair_sets().iter() {
                            budget.work()?;
                            for record in pair_set?.pair_value_records().iter() {
                                budget.work()?;
                                input.add(record?.second_glyph().to_u32());
                            }
                        }
                    }
                    PairPos::Format2(table) => {
                        add_coverage(input, table.coverage()?, budget)?;
                        add_class_coverage(input, table.class_def2()?, budget)?;
                    }
                    _ => return Ok(false),
                }
            }
        }
        PositionSubtables::Cursive(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                add_coverage(
                    input,
                    match table? {
                        CursivePos::Format1(table) => table.coverage()?,
                        CursivePos::Format2(table) => table.coverage()?,
                    },
                    budget,
                )?;
            }
        }
        PositionSubtables::MarkToBase(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let (marks, bases) = match table? {
                    MarkBasePos::Format1(table) => (table.mark_coverage()?, table.base_coverage()?),
                    MarkBasePos::Format2(table) => (table.mark_coverage()?, table.base_coverage()?),
                };
                add_coverage(input, marks, budget)?;
                add_coverage(input, bases, budget)?;
            }
        }
        PositionSubtables::MarkToLig(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let (marks, ligatures) = match table? {
                    MarkLigPos::Format1(table) => {
                        (table.mark_coverage()?, table.ligature_coverage()?)
                    }
                    MarkLigPos::Format2(table) => {
                        (table.mark_coverage()?, table.ligature_coverage()?)
                    }
                };
                add_coverage(input, marks, budget)?;
                add_coverage(input, ligatures, budget)?;
            }
        }
        PositionSubtables::MarkToMark(subtables) => {
            for table in subtables.iter() {
                budget.work()?;
                let (marks1, marks2) = match table? {
                    MarkMarkPos::Format1(table) => {
                        (table.mark1_coverage()?, table.mark2_coverage()?)
                    }
                    MarkMarkPos::Format2(table) => {
                        (table.mark1_coverage()?, table.mark2_coverage()?)
                    }
                };
                add_coverage(input, marks1, budget)?;
                add_coverage(input, marks2, budget)?;
            }
        }
        PositionSubtables::EmptyExtension => {}
        _ => return Ok(false),
    }
    Ok(true)
}

/// Collects glyphs touched by one GSUB or GPOS lookup into the supplied sets.
/// Forms not yet enumerated, malformed data, and work exhaustion conservatively
/// include every set value, represented by an inverted empty set.
///
/// # Safety
/// `face` and each non-`NULL` set must be live. Output sets may be the same.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_lookup_collect_glyphs(
    face: *mut hr_face_t,
    table_tag: hr_tag_t,
    lookup_index: c_uint,
    glyphs_before: *mut hr_set_t,
    glyphs_input: *mut hr_set_t,
    glyphs_after: *mut hr_set_t,
    glyphs_output: *mut hr_set_t,
) {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let Some(font) = face.font() else { return };
    // Each insertion borrows its destination separately, so output pointers may alias.
    let before = SetTarget(glyphs_before);
    let input = SetTarget(glyphs_input);
    let after = SetTarget(glyphs_after);
    let output = SetTarget(glyphs_output);
    let complete = match table_tag {
        GSUB_TAG => font.tables().gsub().ok().and_then(|table| {
            let list = table.lookup_list().ok()?;
            let lookup = list.lookups().get(lookup_index as usize).ok()?;
            let bytes = table.offset_data().len();
            let spent = Cell::new(0);
            let budget = Budget::for_table(bytes, &spent);
            let complete = collect_substitution(
                lookup,
                &list,
                before,
                input,
                after,
                output,
                &budget,
                &mut HashSet::new(),
                0,
            );
            Some(complete)
        }),
        GPOS_TAG => font.tables().gpos().ok().and_then(|table| {
            let list = table.lookup_list().ok()?;
            let lookup = list.lookups().get(lookup_index as usize).ok()?;
            let bytes = table.offset_data().len();
            let spent = Cell::new(0);
            let budget = Budget::for_table(bytes, &spent);
            let complete = collect_positioning(lookup, input, &budget);
            Some(complete)
        }),
        _ => None,
    };
    let Some(complete) = complete else { return };
    if !matches!(complete, Ok(true)) {
        for set in [before, input, after, output] {
            set.include_all_glyphs();
        }
    }
}

/// A registered OpenType BASE baseline tag. The numeric value is the tag itself.
pub type hr_ot_layout_baseline_tag_t = hr_tag_t;

pub const HR_OT_LAYOUT_BASELINE_TAG_ROMAN: hr_ot_layout_baseline_tag_t = 0x726F_6D6Eu32;
pub const HR_OT_LAYOUT_BASELINE_TAG_HANGING: hr_ot_layout_baseline_tag_t = 0x6861_6E67u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT: hr_ot_layout_baseline_tag_t =
    0x6963_6662u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT: hr_ot_layout_baseline_tag_t =
    0x6963_6674u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_CENTRAL: hr_ot_layout_baseline_tag_t = 0x4963_6663u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT: hr_ot_layout_baseline_tag_t =
    0x6964_656Fu32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT: hr_ot_layout_baseline_tag_t =
    0x6964_7470u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL: hr_ot_layout_baseline_tag_t =
    0x4964_6365u32;
pub const HR_OT_LAYOUT_BASELINE_TAG_MATH: hr_ot_layout_baseline_tag_t = 0x6D61_7468u32;

/// Returns the dominant horizontal baseline for a Unicode script.
#[no_mangle]
pub extern "C" fn hr_ot_layout_get_horizontal_baseline_tag_for_script(
    script: hr_script_t,
) -> hr_ot_layout_baseline_tag_t {
    tag_from_rust(read_fonts::tables::base::horizontal_baseline_tag_for_script(tag_to_rust(script)))
}

fn axis(direction: hr_direction_t) -> BaseAxis {
    if matches!(direction, HR_DIRECTION_LTR | HR_DIRECTION_RTL) {
        BaseAxis::Horizontal
    } else {
        BaseAxis::Vertical
    }
}

fn scale_metric(value: F48Dot16, scale: i32, upem: i32) -> i32 {
    ot_round(value.to_f64() * f64::from(scale) / f64::from(upem)) as i32
}

fn baseline(
    font: &hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script_tag: hr_tag_t,
) -> Option<hr_position_t> {
    let instance = font.instance()?;
    let base = instance.tables().base().ok()?;
    let base = BaseInstance::with_coords(base, instance.normalized_coords());
    let horizontal = matches!(axis(direction), BaseAxis::Horizontal);
    // The baseline is a y coordinate in horizontal text and an x coordinate
    // in vertical text, so use the ppem and scale of that coordinate axis.
    let ppem = if horizontal { font.y_ppem } else { font.x_ppem };
    // OpenType device tables address a 16-bit ppem. A larger setting cannot
    // select an entry, but the design-unit coordinate remains available.
    let device_ppem = u16::try_from(ppem).unwrap_or(0);
    let value = base.baseline_for_ppem(
        tag_to_rust(baseline_tag),
        axis(direction),
        tag_to_rust(script_tag),
        device_ppem,
    )?;
    let scale = if horizontal {
        font.y_scale
    } else {
        font.x_scale
    };
    let scaled = scale_metric(value.value, scale, font.upem());
    let device = if device_ppem == 0 {
        0
    } else {
        ot_round(f64::from(value.delta_px) * f64::from(scale) / f64::from(device_ppem)) as i32
    };
    Some(scaled.saturating_add(device))
}

/// Reads a baseline from `BASE`. The language tag is currently unused.
/// Returns false without changing `coord` when the font has no such baseline.
///
/// # Safety
/// `font` must be `NULL` or a live font, and `coord` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_get_baseline(
    font: *mut hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script_tag: hr_tag_t,
    _language_tag: hr_tag_t,
    coord: *mut hr_position_t,
) -> hr_bool_t {
    let font = unsafe { object::or_empty(font.cast_const()) };
    let Some(value) = baseline(font, baseline_tag, direction, script_tag) else {
        return 0;
    };
    if let Some(coord) = unsafe { coord.as_mut() } {
        *coord = value;
    }
    1
}

fn has_base_script(font: &hr_font_t, direction: hr_direction_t, tag: Tag) -> bool {
    font.instance()
        .and_then(|instance| instance.tables().base().ok())
        .and_then(|base| base.axis(axis(direction)))
        .and_then(|axis| axis.base_script_list().ok())
        .is_some_and(|list| {
            list.base_script_records()
                .iter()
                .any(|record| record.base_script_tag() == tag)
        })
}

fn selected_base_script(font: &hr_font_t, direction: hr_direction_t, script: hr_script_t) -> Tag {
    let Some(script) = script_to_rust(script) else {
        return Tag::from_u32(0);
    };
    let tags = script.tags();
    tags.as_slice()
        .iter()
        .copied()
        .find(|tag| has_base_script(font, direction, *tag))
        .unwrap_or_else(|| tags.as_slice().first().copied().unwrap_or(Tag::from_u32(0)))
}

/// Reads a baseline using a Unicode script instead of an OpenType script tag.
///
/// # Safety
/// As for [`hr_ot_layout_get_baseline`].
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_get_baseline2(
    font: *mut hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script: hr_script_t,
    _language: hr_language_t,
    coord: *mut hr_position_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    let selected = selected_base_script(state, direction, script);
    if let Some(value) = baseline(state, baseline_tag, direction, tag_from_rust(selected)) {
        if let Some(coord) = unsafe { coord.as_mut() } {
            *coord = value;
        }
        1
    } else {
        0
    }
}

fn extents(font: &hr_font_t, direction: hr_direction_t) -> (i32, i32) {
    let horizontal = matches!(axis(direction), BaseAxis::Horizontal);
    let Some(instance) = font.instance() else {
        return (0, 0);
    };
    if horizontal {
        instance.metrics().hhea_line.map_or((0, 0), |line| {
            (
                scale_metric(line.ascender, font.y_scale, font.upem()),
                scale_metric(line.descender, font.y_scale, font.upem()),
            )
        })
    } else {
        instance
            .metrics()
            .vhea_line
            .map_or((font.x_scale / 2, -font.x_scale / 2), |line| {
                (
                    scale_metric(line.ascender, font.x_scale, font.upem()),
                    scale_metric(line.descender, font.x_scale, font.upem()),
                )
            })
    }
}

unsafe fn glyph_top(font: *mut hr_font_t, ch: u32) -> Option<hr_glyph_extents_t> {
    let mut glyph = 0;
    if unsafe { hr_font_get_nominal_glyph(font, ch, &raw mut glyph) } == 0 {
        return None;
    }
    let mut extents = hr_glyph_extents_t::default();
    if unsafe { hr_font_get_glyph_extents(font, glyph, &raw mut extents) } == 0 {
        return None;
    }
    Some(extents)
}

/// Synthesizes a baseline when `BASE` does not provide one.
unsafe fn baseline_with_fallback(
    font_ptr: *mut hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script_tag: hr_tag_t,
) -> hr_position_t {
    let font = unsafe { object::or_empty(font_ptr.cast_const()) };
    if let Some(value) = baseline(font, baseline_tag, direction, script_tag) {
        return value;
    }
    let horizontal = matches!(axis(direction), BaseAxis::Horizontal);
    let span = if horizontal {
        font.y_scale
    } else {
        font.x_scale
    };
    match baseline_tag {
        HR_OT_LAYOUT_BASELINE_TAG_ROMAN => 0,
        HR_OT_LAYOUT_BASELINE_TAG_MATH => {
            if horizontal {
                if let Some(extents) = unsafe {
                    glyph_top(font_ptr, 0x2212).or_else(|| glyph_top(font_ptr, b'-' as u32))
                } {
                    return extents.y_bearing.saturating_add(extents.height / 2);
                }
            }
            let x_height = font
                .instance()
                .and_then(|instance| instance.metrics().x_height)
                .map_or(font.y_scale / 2, |height| {
                    scale_metric(height, font.y_scale, font.upem())
                });
            x_height / 2
        }
        HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT => {
            if let Some(bottom) = baseline(
                font,
                HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
                direction,
                script_tag,
            ) {
                bottom.saturating_add(span)
            } else {
                extents(font, direction).0
            }
        }
        HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT => {
            if let Some(top) = baseline(
                font,
                HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
                direction,
                script_tag,
            ) {
                top.saturating_sub(span)
            } else {
                extents(font, direction).1
            }
        }
        HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT
        | HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT => {
            let top = unsafe {
                baseline_with_fallback(
                    font_ptr,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
                    direction,
                    script_tag,
                )
            };
            let bottom = unsafe {
                baseline_with_fallback(
                    font_ptr,
                    HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
                    direction,
                    script_tag,
                )
            };
            if baseline_tag == HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT {
                top.saturating_add(
                    (i64::from(bottom) - i64::from(top)).clamp(i32::MIN as i64, i32::MAX as i64)
                        as i32
                        / 10,
                )
            } else {
                bottom.saturating_add(
                    (i64::from(top) - i64::from(bottom)).clamp(i32::MIN as i64, i32::MAX as i64)
                        as i32
                        / 10,
                )
            }
        }
        HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL
        | HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_CENTRAL => {
            let (top_tag, bottom_tag) =
                if baseline_tag == HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL {
                    (
                        HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
                        HR_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
                    )
                } else {
                    (
                        HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
                        HR_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
                    )
                };
            let top = unsafe { baseline_with_fallback(font_ptr, top_tag, direction, script_tag) };
            let bottom =
                unsafe { baseline_with_fallback(font_ptr, bottom_tag, direction, script_tag) };
            ((i64::from(top) + i64::from(bottom)) / 2) as i32
        }
        HR_OT_LAYOUT_BASELINE_TAG_HANGING => {
            let ch = match script_tag.to_be_bytes() {
                [b'B', b'e', b'n', b'g'] => 0x0995,
                [b'D', b'e', b'v', b'a'] => 0x0915,
                [b'G', b'u', b'j', b'r'] => 0x0a95,
                [b'G', b'u', b'r', b'u'] => 0x0a15,
                [b'T', b'i', b'b', b't'] => 0x0f40,
                _ => 0,
            };
            if horizontal && ch != 0 {
                if let Some(extents) = unsafe { glyph_top(font_ptr, ch) } {
                    return extents.y_bearing;
                }
            }
            span.saturating_mul(6) / 10
        }
        _ => 0,
    }
}

/// Reads a baseline or synthesizes one when absent from `BASE`.
///
/// # Safety
/// `font` must be `NULL` or a live font; `coord` must point to writable storage.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_get_baseline_with_fallback(
    font: *mut hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script_tag: hr_tag_t,
    _language_tag: hr_tag_t,
    coord: *mut hr_position_t,
) {
    if let Some(coord) = unsafe { coord.as_mut() } {
        *coord = unsafe { baseline_with_fallback(font, baseline_tag, direction, script_tag) };
    }
}

/// Reads or synthesizes a baseline using a Unicode script.
///
/// # Safety
/// As for [`hr_ot_layout_get_baseline_with_fallback`].
#[no_mangle]
pub unsafe extern "C" fn hr_ot_layout_get_baseline_with_fallback2(
    font: *mut hr_font_t,
    baseline_tag: hr_ot_layout_baseline_tag_t,
    direction: hr_direction_t,
    script: hr_script_t,
    _language: hr_language_t,
    coord: *mut hr_position_t,
) {
    let Some(coord) = (unsafe { coord.as_mut() }) else {
        return;
    };
    let state = unsafe { object::or_empty(font.cast_const()) };
    let selected = selected_base_script(state, direction, script);
    if let Some(value) = baseline(state, baseline_tag, direction, tag_from_rust(selected)) {
        *coord = value;
        return;
    }
    *coord = unsafe { baseline_with_fallback(font, baseline_tag, direction, script) };
}

#[cfg(test)]
mod budget_tests {
    use super::*;
    use read_fonts::{FontData, FontRead};

    /// One contextual lookup with 1,000 nullable rule-set slots. In the
    /// mixed case, every other slot aliases one readable empty rule set.
    fn contextual_rule_sets(lookup_type: u16, format: u16, mixed: bool) -> Vec<u8> {
        let count = 1000u16;
        let class_based = format == 2;
        let chained = lookup_type == 6;
        let header_words = match (chained, class_based) {
            (_, false) => 3,
            (false, true) => 4,
            (true, true) => 6,
        };
        let coverage_offset = (header_words + usize::from(count)) * 2;
        let class_def_offset = coverage_offset + 4;
        let rule_set_offset = class_def_offset + if class_based { 6 } else { 0 };
        let mut words = vec![format, coverage_offset as u16];
        if class_based {
            words.push(class_def_offset as u16);
            if chained {
                words.extend([class_def_offset as u16; 2]);
            }
        }
        words.push(count);
        words.extend((0..count).map(|index| {
            if mixed && index % 2 == 1 {
                rule_set_offset as u16
            } else {
                0
            }
        }));
        words.extend([1, 0]); // Empty coverage format 1.
        if class_based {
            words.extend([1, 0, 0]); // Empty class definition format 1.
        }
        words.push(0); // A readable rule set with no rules.
        let mut data: Vec<u8> = [1, 4, lookup_type, 0, 1, 8]
            .into_iter()
            .flat_map(u16::to_be_bytes)
            .collect();
        data.extend(words.into_iter().flat_map(u16::to_be_bytes));
        data
    }

    fn collection_work(data: &[u8], limit: usize) -> (Result<bool, ReadError>, usize) {
        let lookup_list = SubstitutionLookupList::Original(
            read_fonts::tables::gsub::OriginalSubstitutionLookupList::read(FontData::new(data))
                .unwrap(),
        );
        let spent = Cell::new(0);
        let budget = Budget {
            limit,
            spent: &spent,
        };
        let none = SetTarget(core::ptr::null_mut());
        let result = collect_substitution(
            lookup_list.lookups().get(0).unwrap(),
            &lookup_list,
            none,
            none,
            none,
            none,
            &budget,
            &mut HashSet::new(),
            0,
        );
        (result, spent.get())
    }

    #[test]
    fn null_contextual_rule_sets_consume_collection_work() {
        for lookup_type in [5, 6] {
            for format in [1, 2] {
                for mixed in [false, true] {
                    let data = contextual_rule_sets(lookup_type, format, mixed);
                    for limit in [500, 1001] {
                        let (result, spent) = collection_work(&data, limit);
                        if limit == 500 {
                            assert!(
                                result.is_err(),
                                "lookup {lookup_type}, format {format}, mixed {mixed}"
                            );
                        } else {
                            assert!(
                                result.unwrap(),
                                "lookup {lookup_type}, format {format}, mixed {mixed}"
                            );
                        }
                        assert_eq!(spent, limit);
                    }
                }
            }
        }
    }

    // Every range is reversed, so the reader visits the records but yields
    // no glyphs. Both the narrow and extended formats must charge the records.
    fn reversed_ranges(format: u16, class_def: bool) -> Vec<u8> {
        let count = 1000u16;
        let mut data = format.to_be_bytes().to_vec();
        let record = if format == 2 {
            data.extend(count.to_be_bytes());
            vec![0, 2, 0, 1, 0, 1]
        } else {
            data.extend([0, 3, 0xE8]); // Uint24 count.
            if class_def {
                vec![0, 0, 2, 0, 0, 1, 0, 1]
            } else {
                vec![0, 0, 2, 0, 0, 1, 0, 0, 0]
            }
        };
        for _ in 0..count {
            data.extend_from_slice(&record);
        }
        data
    }

    #[test]
    fn reversed_coverage_ranges_consume_collection_work() {
        let none = SetTarget(core::ptr::null_mut());
        for format in [2, 4] {
            let data = reversed_ranges(format, false);
            let coverage = CoverageTable::read(FontData::new(&data)).unwrap();
            for limit in [500, 1000] {
                let spent = Cell::new(0);
                let budget = Budget {
                    limit,
                    spent: &spent,
                };
                assert_eq!(
                    add_coverage(none, coverage.clone(), &budget).is_ok(),
                    limit == 1000
                );
                assert_eq!(spent.get(), limit);
            }
            // SingleSubst delta forms walk coverage without add_coverage.
            for single_format in [1, 3] {
                let mut lookup: Vec<u8> = [1u16, 4, 1, 0, 1, 8]
                    .into_iter()
                    .flat_map(u16::to_be_bytes)
                    .collect();
                if single_format == 1 {
                    lookup.extend([0, 1, 0, 6, 0, 1]);
                } else {
                    lookup.extend([0, 3, 0, 0, 0, 9, 0, 0, 1]);
                }
                lookup.extend_from_slice(&data);
                for limit in [500, 1001] {
                    let (result, spent) = collection_work(&lookup, limit);
                    assert_eq!(result.is_ok(), limit == 1001);
                    assert_eq!(spent, limit);
                }
            }
        }
    }

    #[test]
    fn reversed_class_ranges_and_overflowed_glyphs_consume_collection_work() {
        let mut overflowed = vec![0, 3, 0xFF, 0xFF, 0xFF, 0, 3, 0xE8];
        for _ in 0..1000 {
            overflowed.extend([0, 0, 1]);
        }
        let none = SetTarget(core::ptr::null_mut());
        for data in [
            reversed_ranges(2, true),
            reversed_ranges(4, true),
            overflowed,
        ] {
            let class_def = ClassDef::read(FontData::new(&data)).unwrap();
            let total_work = if matches!(class_def, ClassDef::Format3(_)) {
                1001
            } else {
                1000
            };
            for selected_class in [false, true] {
                for limit in [500, total_work] {
                    let spent = Cell::new(0);
                    let budget = Budget {
                        limit,
                        spent: &spent,
                    };
                    let result = if selected_class {
                        add_class(none, &class_def, 1, &budget)
                    } else {
                        add_class_coverage(none, class_def.clone(), &budget)
                    };
                    assert_eq!(result.is_ok(), limit == total_work);
                    assert_eq!(spent.get(), limit);
                }
            }
        }
    }

    #[test]
    fn unreadable_extension_headers_consume_collection_work() {
        let count = 1000u16;
        for positioning in [false, true] {
            let lookup_type = if positioning { 9u16 } else { 7 };
            let mut data: Vec<u8> = [lookup_type, 0, count]
                .into_iter()
                .flat_map(u16::to_be_bytes)
                .collect();
            data.extend((0..count).flat_map(|_| u16::MAX.to_be_bytes()));
            for limit in [500, 1000] {
                let (result, spent) = if positioning {
                    let lookup =
                        read_fonts::tables::gpos::PositionLookup::read(FontData::new(&data))
                            .unwrap();
                    let spent = Cell::new(0);
                    let budget = Budget {
                        limit,
                        spent: &spent,
                    };
                    let result =
                        collect_positioning(lookup, SetTarget(core::ptr::null_mut()), &budget);
                    (result, spent.get())
                } else {
                    let mut list = vec![0, 1, 0, 4];
                    list.extend_from_slice(&data);
                    collection_work(&list, limit)
                };
                assert_eq!(result.is_ok(), limit == 1000);
                assert_eq!(spent, limit);
            }
        }
    }
}
