use std::borrow::Cow;
use std::collections::HashMap;
use std::fmt::{Display, Error, Formatter};
use std::hash::{BuildHasher, Hash, Hasher};
use std::ops::Deref;

use std::sync::{Arc, RwLock};

use murmur3::murmur3_32::MurmurHasher;

use crate::atn::ATN;
use crate::dfa::ScopeExt;
use crate::parser::ParserNodeType;
use crate::parser_atn_simulator::MergeCache;

use crate::prediction_context::PredictionContext::{Array, Singleton};
use crate::rule_context::RuleContext;

use crate::transition::RuleTransition;

pub const PREDICTION_CONTEXT_EMPTY_RETURN_STATE: i32 = 0x7FFFFFFF;

#[cfg(test)]
mod test;

//todo make return states ATNStateRef
#[derive(Eq, Clone, Debug)]
pub enum PredictionContext {
    Singleton(SingletonPredictionContext),
    Array(ArrayPredictionContext),
}

impl PartialEq for PredictionContext {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Array(s), Array(o)) => *s == *o,
            (Singleton(s), Singleton(o)) => *s == *o,
            _ => false,
        }
    }
}

#[derive(Eq, Clone, Debug)]
pub struct ArrayPredictionContext {
    cached_hash: i32,
    return_states: Vec<i32>,
    parents: Vec<Option<Arc<PredictionContext>>>,
}

impl PartialEq for ArrayPredictionContext {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.cached_hash == other.cached_hash
            && self.return_states == other.return_states
            && self.parents.iter().zip(other.parents.iter()).all(opt_eq)
    }
}

#[inline(always)]
fn opt_eq(
    arg: (
        &Option<Arc<PredictionContext>>,
        &Option<Arc<PredictionContext>>,
    ),
) -> bool {
    match arg {
        (Some(s), Some(o)) => Arc::ptr_eq(s, o) || *s == *o,
        (None, None) => true,
        _ => false,
    }
}

#[derive(Eq, Clone, Debug)]
pub struct SingletonPredictionContext {
    cached_hash: i32,
    return_state: i32,
    parent_ctx: Option<Arc<PredictionContext>>,
}

impl PartialEq for SingletonPredictionContext {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.cached_hash == other.cached_hash
            && self.return_state == other.return_state
            && opt_eq((&self.parent_ctx, &other.parent_ctx))
    }
}

impl SingletonPredictionContext {
    #[inline(always)]
    fn is_empty(&self) -> bool {
        self.return_state == PREDICTION_CONTEXT_EMPTY_RETURN_STATE && self.parent_ctx.is_none()
    }
}

impl Display for PredictionContext {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        match self {
            Singleton(s) => {
                if s.return_state == PREDICTION_CONTEXT_EMPTY_RETURN_STATE {
                    f.write_str("$")
                } else if let Some(parent) = &s.parent_ctx {
                    f.write_fmt(format_args!("{} {}", s.return_state, parent))
                } else {
                    f.write_fmt(format_args!("{}", s.return_state))
                }
            }
            Array(arr) => {
                f.write_str("[")?;
                for i in 0..arr.return_states.len() {
                    if i > 0 {
                        f.write_str(", ")?;
                    }
                    if arr.return_states[i] == PREDICTION_CONTEXT_EMPTY_RETURN_STATE {
                        f.write_str("$")?;
                    }
                    f.write_str(&arr.return_states[i].to_string())?;
                    if let Some(parent) = &arr.parents[i] {
                        f.write_fmt(format_args!(" {}", parent))?;
                    } else {
                        f.write_str(" null")?;
                    }
                }

                f.write_str("]")
            }
        }
    }
}

//impl PartialEq for PredictionContext {
//    fn eq(&self, other: &Self) -> bool {
//        self.hash_code() == other.hash_code()
//    }
//}

impl Hash for PredictionContext {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_i32(self.hash_code())
    }
}

lazy_static! {
    pub static ref EMPTY_PREDICTION_CONTEXT: Arc<PredictionContext> =
        Arc::new(PredictionContext::new_empty());
}

impl PredictionContext {
    pub fn new_array(
        parents: Vec<Option<Arc<PredictionContext>>>,
        return_states: Vec<i32>,
    ) -> PredictionContext {
        PredictionContext::Array(ArrayPredictionContext {
            cached_hash: 0,
            parents,
            return_states,
        })
    }

    pub fn new_singleton(
        parent_ctx: Option<Arc<PredictionContext>>,
        return_state: i32,
    ) -> PredictionContext {
        PredictionContext::Singleton(SingletonPredictionContext {
            cached_hash: 0,
            parent_ctx,
            return_state,
        })
        .modify_with(|x| x.calc_hash())
    }

    pub fn new_empty() -> PredictionContext {
        let mut ctx = PredictionContext::Singleton(SingletonPredictionContext {
            cached_hash: 0,
            parent_ctx: None,
            return_state: PREDICTION_CONTEXT_EMPTY_RETURN_STATE,
        });
        ctx.calc_hash();
        ctx
    }

    pub fn calc_hash(&mut self) {
        let mut hasher = MurmurHasher::default();
        match self {
            PredictionContext::Singleton(SingletonPredictionContext {
                parent_ctx,
                return_state,
                ..
            }) => {
                hasher.write_i32(match parent_ctx {
                    None => 0,
                    Some(x) => x.hash_code(),
                });
                hasher.write_i32(*return_state as i32);
            }
            PredictionContext::Array(ArrayPredictionContext {
                parents,
                return_states,
                ..
            }) => {
                parents.iter().for_each(|x| {
                    hasher.write_i32(match x {
                        None => 0,
                        Some(x) => x.hash_code(),
                    })
                });
                return_states
                    .iter()
                    .for_each(|x| hasher.write_i32(*x as i32));
            } //            PredictionContext::Empty { .. } => {}
        };

        let hash = hasher.finish() as i32;

        match self {
            PredictionContext::Singleton(SingletonPredictionContext { cached_hash, .. })
            | PredictionContext::Array(ArrayPredictionContext { cached_hash, .. })
//            | PredictionContext::Empty { cached_hash, .. }
            => *cached_hash = hash,
        };
    }

    pub fn get_parent(&self, index: usize) -> Option<&Arc<PredictionContext>> {
        match self {
            PredictionContext::Singleton(singleton) => {
                //                assert_eq!(index, 0);
                singleton.parent_ctx.as_ref()
            }
            PredictionContext::Array(array) => array.parents[index].as_ref(),
        }
    }

    pub fn get_return_state(&self, index: usize) -> i32 {
        match self {
            PredictionContext::Singleton(SingletonPredictionContext { return_state, .. }) => {
                *return_state
            }
            PredictionContext::Array(ArrayPredictionContext { return_states, .. }) => {
                return_states[index]
            }
        }
    }

    pub fn length(&self) -> usize {
        match self {
            PredictionContext::Singleton { .. } => 1,
            PredictionContext::Array(ArrayPredictionContext { return_states, .. }) => {
                return_states.len()
            }
        }
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        if let PredictionContext::Singleton(singleton) = self {
            return singleton.is_empty();
        }
        self.get_return_state(0) == PREDICTION_CONTEXT_EMPTY_RETURN_STATE
    }

    #[inline(always)]
    pub fn has_empty_path(&self) -> bool {
        self.get_return_state(self.length() - 1) == PREDICTION_CONTEXT_EMPTY_RETURN_STATE
    }

    #[inline(always)]
    pub fn hash_code(&self) -> i32 {
        match self {
            PredictionContext::Singleton(SingletonPredictionContext { cached_hash, .. })
            | PredictionContext::Array(ArrayPredictionContext { cached_hash, .. }) => *cached_hash,
        }
    }

    fn to_array(&self) -> Cow<'_, ArrayPredictionContext> {
        match self {
            PredictionContext::Singleton(s) => Cow::Owned(ArrayPredictionContext {
                cached_hash: 0,
                parents: vec![s.parent_ctx.clone()],
                return_states: vec![s.return_state],
            }),
            PredictionContext::Array(arr) => Cow::Borrowed(arr),
        }
    }

    #[inline(always)]
    pub fn alloc(mut self) -> Arc<PredictionContext> {
        self.calc_hash();
        Arc::new(self)
    }

    pub fn merge(
        a: &Arc<PredictionContext>,
        b: &Arc<PredictionContext>,
        root_is_wildcard: bool,
        merge_cache: &mut Option<&mut MergeCache>,
        //                 eq_hash:&mut HashSet<(*const PredictionContext,*const PredictionContext)>
    ) -> Arc<PredictionContext> {
        if Arc::ptr_eq(a, b) || **a == **b {
            return a.clone();
        }

        if let Some(cache) = merge_cache {
            if let Some(old) = cache
                .get(&(a.clone(), b.clone()))
                .or_else(|| cache.get(&(b.clone(), a.clone())))
            {
                //            if let Some(old) = cache.get(a)
                //                .and_then(|it|it.get(b))
                //                .or_else(||cache.get(b).and_then(|it|it.get(a))){
                return old.clone();
            }
        }
        //        println!("merging {} {}",a,b);

        let r = match (a.deref(), b.deref()) {
            (PredictionContext::Singleton(sa), PredictionContext::Singleton(sb)) => {
                //                println!("single result = {}",result);
                Self::merge_singletons(sa, sb, root_is_wildcard, merge_cache)
            }
            (sa, sb) => {
                if root_is_wildcard {
                    if sa.is_empty() {
                        return EMPTY_PREDICTION_CONTEXT.clone();
                    }
                    if sb.is_empty() {
                        return EMPTY_PREDICTION_CONTEXT.clone();
                    }
                }

                let result =
                    Self::merge_arrays(sa.to_array(), sb.to_array(), root_is_wildcard, merge_cache)
                        .alloc();

                //                println!("array result = {}",result);

                if &*result == sa {
                    a.clone()
                } else if &*result == sb {
                    b.clone()
                } else {
                    result //.alloc()
                }
            }
        };
        assert_ne!(r.hash_code(), 0);
        if let Some(cache) = merge_cache {
            //            cache.entry(a.clone()).or_insert_with(||HashMap::new())
            //                .insert(b.clone(),r.clone());
            cache.insert((a.clone(), b.clone()), r.clone());
        }
        r
    }

    fn merge_singletons(
        a: &SingletonPredictionContext,
        b: &SingletonPredictionContext,
        root_is_wildcard: bool,
        merge_cache: &mut Option<&mut MergeCache>,
    ) -> Arc<PredictionContext> {
        Self::merge_root(a, b, root_is_wildcard).unwrap_or_else(|| {
            if a.return_state == b.return_state {
                let parent = Self::merge(
                    a.parent_ctx.as_ref().unwrap(),
                    b.parent_ctx.as_ref().unwrap(),
                    root_is_wildcard,
                    merge_cache,
                );
                if Arc::ptr_eq(&parent, a.parent_ctx.as_ref().unwrap()) {
                    Singleton(a.clone())
                } else if Arc::ptr_eq(&parent, b.parent_ctx.as_ref().unwrap()) {
                    Singleton(b.clone())
                } else {
                    Self::new_singleton(Some(parent), a.return_state)
                }
            } else {
                let parents = if a.parent_ctx == b.parent_ctx {
                    vec![a.parent_ctx.clone(), a.parent_ctx.clone()]
                } else {
                    vec![a.parent_ctx.clone(), b.parent_ctx.clone()]
                };
                let mut result = ArrayPredictionContext {
                    cached_hash: -1,
                    parents,
                    return_states: vec![a.return_state, b.return_state],
                };
                // if !result.return_states.is_sorted()
                if !result.return_states.windows(2).all(|x| x[0] <= x[1]) {
                    result.parents.swap(0, 1);
                    result.return_states.swap(0, 1);
                }
                Array(result)
            }
            .alloc()
        })
    }

    fn merge_root(
        a: &SingletonPredictionContext,
        b: &SingletonPredictionContext,
        root_is_wildcard: bool,
    ) -> Option<Arc<PredictionContext>> {
        if root_is_wildcard {
            if a.is_empty() || b.is_empty() {
                return Some(EMPTY_PREDICTION_CONTEXT.clone());
            }
        } else {
            if a.is_empty() && b.is_empty() {
                return Some(EMPTY_PREDICTION_CONTEXT.clone());
            }
            if a.is_empty() {
                return Some(
                    Self::new_array(
                        vec![b.parent_ctx.clone(), None],
                        vec![b.return_state, PREDICTION_CONTEXT_EMPTY_RETURN_STATE],
                    )
                    .alloc(),
                );
            }
            if b.is_empty() {
                return Some(
                    Self::new_array(
                        vec![a.parent_ctx.clone(), None],
                        vec![a.return_state, PREDICTION_CONTEXT_EMPTY_RETURN_STATE],
                    )
                    .alloc(),
                );
            }
        }

        None
    }

    fn merge_arrays(
        a: Cow<'_, ArrayPredictionContext>,
        b: Cow<'_, ArrayPredictionContext>,
        root_is_wildcard: bool,
        merge_cache: &mut Option<&mut MergeCache>,
    ) -> PredictionContext {
        //        let a = a.deref();
        //        let b = b.deref();
        let mut merged = ArrayPredictionContext {
            cached_hash: -1,
            parents: Vec::with_capacity(a.return_states.len() + b.return_states.len()),
            return_states: Vec::with_capacity(a.return_states.len() + b.return_states.len()),
        };
        let mut i = 0;
        let mut j = 0;

        while i < a.parents.len() && j < b.parents.len() {
            let a_parent = a.parents[i].as_ref();
            let b_parent = b.parents[j].as_ref();
            if a.return_states[i] == b.return_states[j] {
                let payload = a.return_states[i];
                let both = payload == PREDICTION_CONTEXT_EMPTY_RETURN_STATE
                    && a_parent.is_none()
                    && b_parent.is_none();
                let ax_ax = a_parent.is_some() && b_parent.is_some() && a_parent == b_parent;

                if both || ax_ax {
                    merged.return_states.push(payload);
                    merged.parents.push(a_parent.cloned());
                } else {
                    let merged_parent = Self::merge(
                        a_parent.unwrap(),
                        b_parent.unwrap(),
                        root_is_wildcard,
                        merge_cache,
                    );
                    merged.return_states.push(payload);
                    merged.parents.push(Some(merged_parent));
                }
                i += 1;
                j += 1;
            } else if a.return_states[i] < b.return_states[j] {
                merged.return_states.push(a.return_states[i]);
                merged.parents.push(a_parent.cloned());
                i += 1;
            } else {
                merged.return_states.push(b.return_states[j]);
                merged.parents.push(b_parent.cloned());
                j += 1;
            }
        }

        if i < a.return_states.len() {
            for p in i..a.return_states.len() {
                merged.parents.push(a.parents[p].clone());
                merged.return_states.push(a.return_states[p]);
            }
        }
        if j < b.return_states.len() {
            for p in j..b.return_states.len() {
                merged.parents.push(b.parents[p].clone());
                merged.return_states.push(b.return_states[p]);
            }
        }

        if merged.parents.len() < a.return_states.len() + b.return_states.len() {
            if merged.parents.len() == 1 {
                Self::new_singleton(merged.parents[0].clone(), merged.return_states[0]);
            }
            merged.return_states.shrink_to_fit();
            merged.parents.shrink_to_fit();
        }

        PredictionContext::combine_common_parents(&mut merged);

        //        if &m == a.deref(){ return ; }
        //        if &m == b.deref(){ return ; }

        Array(merged)
    }

    pub fn from_rule_context<'input, Ctx: ParserNodeType<'input>>(
        atn: &ATN,
        outer_context: &Ctx::Type,
    ) -> Arc<PredictionContext> {
        if outer_context.get_parent_ctx().is_none() || outer_context.is_empty()
        /*ptr::eq(outer_context, empty_ctx().as_ref())*/
        {
            return EMPTY_PREDICTION_CONTEXT.clone();
        }

        let parent = PredictionContext::from_rule_context::<Ctx>(
            atn,
            outer_context.get_parent_ctx().unwrap().deref(),
        );

        let transition = atn.states[outer_context.get_invoking_state() as usize]
            .get_transitions()
            .first()
            .unwrap()
            .deref()
            .cast::<RuleTransition>();

        PredictionContext::new_singleton(Some(parent), transition.follow_state as i32).alloc()
    }

    fn combine_common_parents(array: &mut ArrayPredictionContext) {
        let mut uniq_parents =
            HashMap::<Option<Arc<PredictionContext>>, Option<Arc<PredictionContext>>>::new();
        for p in 0..array.parents.len() {
            let parent = array.parents[p].as_ref().cloned();
            if !uniq_parents.contains_key(&parent) {
                uniq_parents.insert(parent.clone(), parent.clone());
            }
        }

        array.parents.iter_mut().for_each(|parent| {
            *parent = (*uniq_parents.get(parent).unwrap()).clone();
        });
    }
}

//
//    fn get_cached_base_prediction_context(context PredictionContext, contextCache: * PredictionContextCache, visited: map[PredictionContext]PredictionContext) -> PredictionContext { unimplemented!() }

///Public for implementation reasons
#[derive(Debug)]
pub struct PredictionContextCache {
    //todo test dashmap
    cache: RwLock<HashMap<Arc<PredictionContext>, Arc<PredictionContext>, MurmurHasherBuilder>>,
}

#[doc(hidden)]
#[derive(Debug)]
pub struct MurmurHasherBuilder {}

impl BuildHasher for MurmurHasherBuilder {
    type Hasher = MurmurHasher;

    fn build_hasher(&self) -> Self::Hasher {
        MurmurHasher::default()
    }
}

impl PredictionContextCache {
    #[doc(hidden)]
    pub fn new() -> PredictionContextCache {
        PredictionContextCache {
            cache: RwLock::new(HashMap::with_hasher(MurmurHasherBuilder {})),
        }
    }

    #[doc(hidden)]
    pub fn get_shared_context(
        &self,
        context: &Arc<PredictionContext>,
        visited: &mut HashMap<*const PredictionContext, Arc<PredictionContext>>,
    ) -> Arc<PredictionContext> {
        if context.is_empty() {
            return context.clone();
        }

        if let Some(old) = visited.get(&(context.deref() as *const PredictionContext)) {
            return old.clone();
        }

        if let Some(old) = self.cache.read().unwrap().get(context) {
            return old.clone();
        }
        let mut parents = Vec::with_capacity(context.length());
        let mut changed = false;
        for i in 0..parents.len() {
            let parent = self.get_shared_context(context.get_parent(i).unwrap(), visited);
            if changed || &parent != context.get_parent(i).unwrap() {
                if !changed {
                    for j in 0..i {
                        parents.push(context.get_parent(j).cloned())
                    }
                    changed = true;
                }
                parents.push(Some(parent.clone()))
            }
        }
        if !changed {
            self.cache
                .write()
                .unwrap()
                .insert(context.clone(), context.clone());
            visited.insert(context.deref(), context.clone());
            return context.clone();
        }

        let updated = if parents.is_empty() {
            return EMPTY_PREDICTION_CONTEXT.clone();
        } else if parents.len() == 1 {
            PredictionContext::new_singleton(parents[0].clone(), context.get_return_state(0))
        } else if let Array(array) = context.deref() {
            PredictionContext::new_array(parents, array.return_states.clone())
        } else {
            unreachable!()
        };

        let updated = Arc::new(updated);
        self.cache
            .write()
            .unwrap()
            .insert(updated.clone(), updated.clone());
        visited.insert(context.deref(), updated.clone());
        visited.insert(updated.deref(), updated.clone());

        updated
    }

    #[doc(hidden)]
    pub fn length(&self) -> usize {
        self.cache.read().unwrap().len()
    }
}
