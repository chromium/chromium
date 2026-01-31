use std::collections::HashMap;

use bit_set::BitSet;

use crate::atn::INVALID_ALT;
use crate::atn_config::ATNConfig;
use crate::atn_config_set::ATNConfigSet;
use crate::atn_state::ATNStateRef;
use crate::prediction_context::PredictionContext;
use crate::semantic_context::SemanticContext;

/// This enum defines the prediction modes available in ANTLR 4 along with
/// utility methods for analyzing configuration sets for conflicts and/or
/// ambiguities.
///
/// It is set through `ParserATNSimulator::
#[allow(non_camel_case_types)]
#[derive(Eq, PartialEq, Copy, Clone, Debug)]
pub enum PredictionMode {
    /// The SLL(*) prediction mode. This prediction mode ignores the current
    /// parser context when making predictions. This is the fastest prediction
    /// mode, and provides correct results for many grammars. This prediction
    /// mode is more powerful than the prediction mode provided by ANTLR 3, but
    /// may result in syntax errors for grammar and input combinations which are
    /// not SLL.
    ///
    /// <p>
    /// When using this prediction mode, the parser will either return a correct
    /// parse tree (i.e. the same parse tree that would be returned with the
    /// {@link #LL} prediction mode), or it will report a syntax error. If a
    /// syntax error is encountered when using the {@link #SLL} prediction mode,
    /// it may be due to either an actual syntax error in the input or indicate
    /// that the particular combination of grammar and input requires the more
    /// powerful {@link #LL} prediction abilities to complete successfully.</p>
    ///
    /// <p>
    /// This prediction mode does not provide any guarantees for prediction
    /// behavior for syntactically-incorrect inputs.</p>
    ///
    SLL = 0,
    ///
    /// The LL(*) prediction mode. This prediction mode allows the current parser
    /// context to be used for resolving SLL conflicts that occur during
    /// prediction. This is the fastest prediction mode that guarantees correct
    /// parse results for all combinations of grammars with syntactically correct
    /// inputs.
    ///
    /// <p>
    /// When using this prediction mode, the parser will make correct decisions
    /// for all syntactically-correct grammar and input combinations. However, in
    /// cases where the grammar is truly ambiguous this prediction mode might not
    /// report a precise answer for <em>exactly which</em> alternatives are
    /// ambiguous.</p>
    ///
    /// <p>
    /// This prediction mode does not provide any guarantees for prediction
    /// behavior for syntactically-incorrect inputs.</p>
    ///
    LL,
    ///
    /// The LL(*) prediction mode with exact ambiguity detection. In addition to
    /// the correctness guarantees provided by the {@link #LL} prediction mode,
    /// this prediction mode instructs the prediction algorithm to determine the
    /// complete and exact set of ambiguous alternatives for every ambiguous
    /// decision encountered while parsing.
    ///
    /// <p>
    /// This prediction mode may be used for diagnosing ambiguities during
    /// grammar development. Due to the performance overhead of calculating sets
    /// of ambiguous alternatives, this prediction mode should be avoided when
    /// the exact results are not necessary.</p>
    ///
    /// <p>
    /// This prediction mode does not provide any guarantees for prediction
    /// behavior for syntactically-incorrect inputs.</p>
    ///
    LL_EXACT_AMBIG_DETECTION,
}

impl PredictionMode {
    //todo move everything here
}

//
//
pub(crate) fn has_sll_conflict_terminating_prediction(
    mode: PredictionMode,
    configs: &ATNConfigSet,
) -> bool {
    //    if all_configs_in_rule_stop_states(configs) {
    //        return true          checked outside
    //    }
    let mut dup = ATNConfigSet::new_base_atnconfig_set(true);
    let mut configs = configs;
    if mode == PredictionMode::SLL && configs.has_semantic_context() {
        configs.get_items().for_each(|it| {
            let c = ATNConfig::new_with_semantic(
                it.get_state(),
                it.get_alt(),
                it.get_context().cloned(),
                Box::new(SemanticContext::NONE),
            );
            dup.add(Box::new(c));
        });
        configs = &dup;
    }

    let altsets = get_conflicting_alt_subsets(configs);

    has_conflicting_alt_set(&altsets) && !has_state_associated_with_one_alt(configs)
}

//fn all_configs_in_rule_stop_states(configs: &ATNConfigSet) -> bool {
//    for co
//}

pub(crate) fn resolves_to_just_one_viable_alt(altsets: &Vec<BitSet>) -> i32 {
    get_single_viable_alt(altsets)
}

pub(crate) fn all_subsets_conflict(altsets: &Vec<BitSet>) -> bool {
    !has_non_conflicting_alt_set(altsets)
}

pub(crate) fn all_subsets_equal(altsets: &Vec<BitSet>) -> bool {
    let mut iter = altsets.iter();
    let first = iter.next();
    iter.all(|it| it == first.unwrap())
}

fn has_non_conflicting_alt_set(altsets: &Vec<BitSet>) -> bool {
    altsets.iter().any(|it| it.len() == 1)
}

fn has_conflicting_alt_set(altsets: &Vec<BitSet>) -> bool {
    for alts in altsets {
        if alts.len() > 1 {
            return true;
        }
    }
    false
}

//fn get_unique_alt(altsets: &Vec<BitSet>) -> int { unimplemented!() }
//
pub(crate) fn get_alts(altsets: &Vec<BitSet>) -> BitSet {
    altsets.iter().fold(BitSet::new(), |mut acc, it| {
        acc.extend(it);
        acc
    })
}

//
pub(crate) fn get_conflicting_alt_subsets(configs: &ATNConfigSet) -> Vec<BitSet> {
    let mut configs_to_alts: HashMap<(ATNStateRef, &PredictionContext), BitSet> = HashMap::new();
    for c in configs.get_items() {
        let alts = configs_to_alts
            .entry((c.get_state(), c.get_context().unwrap()))
            .or_default();

        alts.insert(c.get_alt() as usize);
    }
    configs_to_alts.drain().map(|(_, x)| x).collect()
}

fn get_state_to_alt_map(configs: &ATNConfigSet) -> HashMap<ATNStateRef, BitSet> {
    let mut m = HashMap::new();
    for c in configs.get_items() {
        let alts = m.entry(c.get_state()).or_insert(BitSet::new());
        alts.insert(c.get_alt() as usize);
    }
    m
}

fn has_state_associated_with_one_alt(configs: &ATNConfigSet) -> bool {
    let x = get_state_to_alt_map(configs);
    for alts in x.values() {
        if alts.len() == 1 {
            return true;
        }
    }
    false
}

pub(crate) fn get_single_viable_alt(altsets: &Vec<BitSet>) -> i32 {
    let mut viable_alts = BitSet::new();
    let mut min_alt = INVALID_ALT as usize;
    for alt in altsets {
        min_alt = alt.iter().next().unwrap();
        viable_alts.insert(min_alt);
        if viable_alts.len() > 1 {
            return INVALID_ALT;
        }
    }
    min_alt as i32
}
