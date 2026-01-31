use std::cmp::max;
use std::collections::HashMap;
use std::fmt::{Debug, Error, Formatter};
use std::hash::{Hash, Hasher};
use std::ops::Deref;

use bit_set::BitSet;
use murmur3::murmur3_32::MurmurHasher;

use crate::atn_config::ATNConfig;
use crate::atn_simulator::IATNSimulator;
use crate::atn_state::ATNStateRef;
use crate::parser_atn_simulator::MergeCache;
use crate::prediction_context::{MurmurHasherBuilder, PredictionContext};
use crate::semantic_context::SemanticContext;

pub struct ATNConfigSet {
    cached_hash: u64,

    //todo looks like we need only iteration for configs
    // so i think we can replace configs and lookup with indexhashset
    config_lookup: HashMap<Key, usize, MurmurHasherBuilder>,

    //todo remove box?
    pub(crate) configs: Vec<Box<ATNConfig>>,

    pub(crate) conflicting_alts: BitSet,

    dips_into_outer_context: bool,

    full_ctx: bool,

    has_semantic_context: bool,

    read_only: bool,

    unique_alt: i32,

    /// creates key for lookup
    /// Key::Full - for Lexer
    /// Key::Partial  - for Parser
    hasher: fn(&ATNConfig) -> Key,
}

#[derive(Eq, PartialEq)]
enum Key {
    Full(ATNConfig),
    Partial(i32, ATNStateRef, i32, SemanticContext),
}

impl Hash for Key {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self {
            Key::Full(x) => x.hash(state),
            Key::Partial(hash, _, _, _) => state.write_i32(*hash),
        }
    }
}

impl Debug for ATNConfigSet {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), Error> {
        _f.write_str("ATNConfigSet")?;
        _f.debug_list().entries(self.configs.iter()).finish()?;
        if self.has_semantic_context {
            _f.write_str(",hasSemanticContext=true")?
        }
        if self.conflicting_alts.is_empty() {
            _f.write_fmt(format_args!(",uniqueAlt={}", self.unique_alt))
        } else {
            _f.write_fmt(format_args!(",conflictingAlts={:?}", self.conflicting_alts))
        }
    }
}

impl PartialEq for ATNConfigSet {
    fn eq(&self, other: &Self) -> bool {
        self.configs == other.configs
            && self.full_ctx == other.full_ctx
            && self.unique_alt == other.unique_alt
            && self.conflicting_alts == other.conflicting_alts
            && self.has_semantic_context == other.has_semantic_context
            && self.dips_into_outer_context == other.dips_into_outer_context
    }
}

impl Eq for ATNConfigSet {}

impl Hash for ATNConfigSet {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.configs.hash(state)
    }
}

impl ATNConfigSet {
    pub fn new_base_atnconfig_set(full_ctx: bool) -> ATNConfigSet {
        ATNConfigSet {
            cached_hash: 0,
            config_lookup: HashMap::with_hasher(MurmurHasherBuilder {}),
            configs: vec![],
            conflicting_alts: Default::default(),
            dips_into_outer_context: false,
            full_ctx,
            has_semantic_context: false,
            read_only: false,
            unique_alt: 0,
            hasher: Self::local_hash_key,
        }
    }

    // for lexerATNConfig
    pub fn new_ordered() -> ATNConfigSet {
        let mut a = ATNConfigSet::new_base_atnconfig_set(true);

        a.hasher = Self::full_hash_key;
        a
    }

    fn full_hash_key(config: &ATNConfig) -> Key {
        Key::Full(config.clone())
    }

    fn local_hash_key(config: &ATNConfig) -> Key {
        let mut hasher = MurmurHasher::default();
        config.get_state().hash(&mut hasher);
        config.get_alt().hash(&mut hasher);
        config.semantic_context.hash(&mut hasher);

        Key::Partial(
            hasher.finish() as i32,
            config.get_state(),
            config.get_alt(),
            config.semantic_context.deref().clone(),
        )
    }

    pub fn add_cached(
        &mut self,
        config: Box<ATNConfig>,
        mut merge_cache: Option<&mut MergeCache>,
    ) -> bool {
        assert!(!self.read_only);

        if *config.semantic_context != SemanticContext::NONE {
            self.has_semantic_context = true
        }

        if config.get_reaches_into_outer_context() > 0 {
            self.dips_into_outer_context = true
        }

        let hasher = self.hasher;
        let key = hasher(config.as_ref());

        if let Some(existing) = self.config_lookup.get(&key) {
            let existing = self.configs.get_mut(*existing).unwrap().as_mut();
            let root_is_wildcard = !self.full_ctx;

            let merged = PredictionContext::merge(
                existing.get_context().unwrap(),
                config.get_context().unwrap(),
                root_is_wildcard,
                &mut merge_cache,
            );

            existing.set_reaches_into_outer_context(max(
                existing.get_reaches_into_outer_context(),
                config.get_reaches_into_outer_context(),
            ));

            if config.is_precedence_filter_suppressed() {
                existing.set_precedence_filter_suppressed(true)
            }

            existing.set_context(merged);
        } else {
            self.config_lookup.insert(key, self.configs.len());
            self.cached_hash = 0;
            self.configs.push(config);
        }
        true
    }

    pub fn add(&mut self, config: Box<ATNConfig>) -> bool {
        self.add_cached(config, None)
    }

    pub fn get_items(&self) -> impl Iterator<Item = &ATNConfig> {
        self.configs.iter().map(|c| c.as_ref())
    }

    pub fn optimize_configs(&mut self, _interpreter: &dyn IATNSimulator) {
        if self.configs.is_empty() {
            return;
        }

        for config in self.configs.iter_mut() {
            let mut visited = HashMap::new();
            config.set_context(
                _interpreter
                    .shared_context_cache()
                    .get_shared_context(config.get_context().unwrap(), &mut visited),
            );
        }
    }

    pub fn length(&self) -> usize {
        self.configs.len()
    }

    pub fn is_empty(&self) -> bool {
        self.configs.is_empty()
    }

    pub fn has_semantic_context(&self) -> bool {
        self.has_semantic_context
    }

    pub fn set_has_semantic_context(&mut self, _v: bool) {
        self.has_semantic_context = _v;
    }

    pub fn read_only(&self) -> bool {
        self.read_only
    }

    pub fn set_read_only(&mut self, _read_only: bool) {
        self.read_only = _read_only;
    }

    pub fn full_context(&self) -> bool {
        self.full_ctx
    }

    //duplicate of the self.conflicting_alts???
    pub fn get_alts(&self) -> BitSet {
        self.configs.iter().fold(BitSet::new(), |mut acc, c| {
            acc.insert(c.get_alt() as usize);
            acc
        })
    }

    pub fn get_unique_alt(&self) -> i32 {
        self.unique_alt
    }

    pub fn set_unique_alt(&mut self, _v: i32) {
        self.unique_alt = _v
    }

    pub fn get_dips_into_outer_context(&self) -> bool {
        self.dips_into_outer_context
    }

    pub fn set_dips_into_outer_context(&mut self, _v: bool) {
        self.dips_into_outer_context = _v
    }
}
