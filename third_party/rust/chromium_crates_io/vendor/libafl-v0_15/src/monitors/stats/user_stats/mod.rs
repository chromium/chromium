//! User-defined statistics

mod user_stats_value;
use alloc::borrow::Cow;
use core::fmt;

use serde::{Deserialize, Serialize};
pub use user_stats_value::*;

use super::manager::ClientStatsManager;

/// user defined stats enum
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct UserStats {
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
    /// Constructor
    #[must_use]
    pub fn new(value: UserStatsValue, aggregator_op: AggregatorOps) -> Self {
        Self {
            value,
            aggregator_op,
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
