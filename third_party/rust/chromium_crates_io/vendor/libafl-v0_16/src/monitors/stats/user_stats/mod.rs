//! User-defined statistics

mod user_stats_value;
use alloc::borrow::Cow;
use core::{fmt, num::NonZero};

use libafl_bolts::nonzero;
use serde::{Deserialize, Serialize};
pub use user_stats_value::*;

use super::manager::ClientStatsManager;

/// A tag that indicates what a certain stat is used for.
///  
/// The tag lets us identify the use-case for this stats
/// for display purposes (such as `TAG_MAP` for the (coverage) maps)
///
/// Ideally should be unique per use-case
/// The tags try to be 1337-speak of the values they represent.
/// They NEED to be unique.
///
/// We don't use an Enum here in oder to stay extensible for non-core tags.
pub type UserStatsTag = NonZero<u64>;

/// Tag that stignifies the stats are for a map of sorts.
pub const TAG_MAP: UserStatsTag = nonzero!(0xC07E9A6EC07E9A6E);

/// Tag that signifies AFL stats cycles done
pub const TAG_AFL_STATS_CYCLES_DONE: UserStatsTag = nonzero!(0xAF157A75_C1C135D0);
/// Tag that signifies AFL stats pending
pub const TAG_AFL_STATS_PENDING: UserStatsTag = nonzero!(0x0AF1_57A7_5934_D146);
/// Tag that signifies AFL stats pending favored
pub const TAG_AFL_STATS_PENDING_FAVORED: UserStatsTag = nonzero!(0xAF157A75_934D146F);
/// Tag that signifies AFL stats pending favored (custom)
pub const TAG_AFL_STATS_PENDING_FAV: UserStatsTag = nonzero!(0xAF157A75_FA1702ED);
/// Tag that signifies AFL stats own finds
pub const TAG_AFL_STATS_OWN_FINDS: UserStatsTag = nonzero!(0xAF157A75_0C4F11D5);
/// Tag that signifies AFL stats imported
pub const TAG_AFL_STATS_IMPORTED: UserStatsTag = nonzero!(0xAF157A75_1AAF07ED);
/// Tag that signifies AFL stats cycles without finds
pub const TAG_AFL_STATS_CYCLES_WO_FINDS: UserStatsTag = nonzero!(0xAF157A75_C1C0F1D5);

/// Tag that signifies calibration stability
pub const TAG_CALIBRATE_STABILITY: UserStatsTag = nonzero!(0x57AB1117157AB171);

/// Tag that signifies the core id of a node
pub const TAG_CORE_ID: UserStatsTag = nonzero!(0xC093C093C093C093);

/// The plot config for the user stats
#[derive(Serialize, Deserialize, Debug, Clone, Copy, PartialEq, Eq)]
pub enum PlotConfig {
    /// No grouping
    None,
    /// Group by color
    Color(u8, u8, u8),
    /// Group by simple color index (0-255)
    SimpleColor(u8),
}

/// user defined stats enum
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct UserStats {
    /// Optional tag that signifies the category this stat is for.
    tag: Option<UserStatsTag>,
    value: UserStatsValue,
    aggregator_op: AggregatorOps,
}

impl UserStats {
    /// Get the `AggregatorOps`
    #[must_use]
    pub fn aggregator_op(&self) -> &AggregatorOps {
        &self.aggregator_op
    }
    /// Get the actual value for the stats
    #[must_use]
    pub fn value(&self) -> &UserStatsValue {
        &self.value
    }
    /// Get the tag
    #[must_use]
    pub fn tag(&self) -> Option<UserStatsTag> {
        self.tag
    }

    /// Creates a new `UserStats` with values given
    #[must_use]
    pub fn new(value: UserStatsValue, aggregator_op: AggregatorOps) -> Self {
        Self {
            tag: None,
            value,
            aggregator_op,
        }
    }

    /// Creates a new `UserStats` with values given
    #[must_use]
    pub fn with_tag(
        value: UserStatsValue,
        aggregator_op: AggregatorOps,
        tag: UserStatsTag,
    ) -> Self {
        Self {
            value,
            aggregator_op,
            tag: Some(tag),
        }
    }
}

impl fmt::Display for UserStats {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.value())
    }
}

/// Definition of how we aggregate this across multiple clients
#[derive(Serialize, Deserialize, Debug, Copy, Clone, Eq, PartialEq)]
pub enum AggregatorOps {
    /// Do nothing
    None,
    /// Add stats up
    Sum,
    /// Average stats out
    Avg,
    /// Get the min
    Min,
    /// Get the max
    Max,
}

// clippy::ptr_arg is allowed here to avoid one unnecessary deep clone when
// inserting name into user_stats HashMap.
/// Aggregate user statistics according to their ops
#[allow(clippy::ptr_arg)]
pub(super) fn aggregate_user_stats(
    client_stats_manager: &mut ClientStatsManager,
    name: &Cow<'static, str>,
) {
    let mut gather = client_stats_manager
        .client_stats()
        .iter()
        .filter_map(|(_, client)| client.user_stats.get(name.as_ref()));

    let gather_count = gather.clone().count();

    let (mut init, op) = match gather.next() {
        Some(x) => (x.value().clone(), *x.aggregator_op()),
        _ => {
            return;
        }
    };

    for item in gather {
        match op {
            AggregatorOps::None => {
                // Nothing
                return;
            }
            AggregatorOps::Avg | AggregatorOps::Sum => {
                init = match init.stats_add(item.value()) {
                    Some(x) => x,
                    _ => {
                        return;
                    }
                };
            }
            AggregatorOps::Min => {
                init = match init.stats_min(item.value()) {
                    Some(x) => x,
                    _ => {
                        return;
                    }
                };
            }
            AggregatorOps::Max => {
                init = match init.stats_max(item.value()) {
                    Some(x) => x,
                    _ => {
                        return;
                    }
                };
            }
        }
    }

    if let AggregatorOps::Avg = op {
        // if avg then divide last.
        init = match init.stats_div(gather_count) {
            Some(x) => x,
            _ => {
                return;
            }
        }
    }

    client_stats_manager
        .cached_aggregated_user_stats
        .insert(name.clone(), init);
}
