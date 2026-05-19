//! Value type of user stats

use alloc::borrow::Cow;
use core::fmt;

use serde::{Deserialize, Serialize};

/// The actual value for the userstats
#[derive(Serialize, Deserialize, Debug, Clone)]
pub enum UserStatsValue {
    /// A numerical value
    Number(u64),
    /// A Float value
    Float(f64),
    /// A `String`
    String(Cow<'static, str>),
    /// A ratio of two values
    Ratio(u64, u64),
    /// Percent
    Percent(f64),
}

impl UserStatsValue {
    /// Check if this guy is numeric
    #[must_use]
    pub fn is_numeric(&self) -> bool {
        match &self {
            Self::Number(_) | Self::Float(_) | Self::Ratio(_, _) | Self::Percent(_) => true,
            Self::String(_) => false,
        }
    }

    /// Divide by the number of elements
    #[expect(clippy::cast_precision_loss)]
    pub fn stats_div(&mut self, divisor: usize) -> Option<Self> {
        match self {
            Self::Number(x) => Some(Self::Float(*x as f64 / divisor as f64)),
            Self::Float(x) => Some(Self::Float(*x / divisor as f64)),
            Self::Percent(x) => Some(Self::Percent(*x / divisor as f64)),
            Self::Ratio(x, y) => Some(Self::Percent((*x as f64 / divisor as f64) / *y as f64)),
            Self::String(_) => None,
        }
    }

    /// min user stats with the other
    #[expect(clippy::cast_precision_loss)]
    pub fn stats_max(&mut self, other: &Self) -> Option<Self> {
        match (self, other) {
            (Self::Number(x), Self::Number(y)) => {
                if y > x {
                    Some(Self::Number(*y))
                } else {
                    Some(Self::Number(*x))
                }
            }
            (Self::Float(x), Self::Float(y)) => {
                if y > x {
                    Some(Self::Float(*y))
                } else {
                    Some(Self::Float(*x))
                }
            }
            (Self::Ratio(x, a), Self::Ratio(y, b)) => {
                let first = *x as f64 / *a as f64;
                let second = *y as f64 / *b as f64;
                if first > second {
                    Some(Self::Percent(first))
                } else {
                    Some(Self::Percent(second))
                }
            }
            (Self::Percent(x), Self::Percent(y)) => {
                if y > x {
                    Some(Self::Percent(*y))
                } else {
                    Some(Self::Percent(*x))
                }
            }
            _ => None,
        }
    }

    /// min user stats with the other
    #[expect(clippy::cast_precision_loss)]
    pub fn stats_min(&mut self, other: &Self) -> Option<Self> {
        match (self, other) {
            (Self::Number(x), Self::Number(y)) => {
                if y > x {
                    Some(Self::Number(*x))
                } else {
                    Some(Self::Number(*y))
                }
            }
            (Self::Float(x), Self::Float(y)) => {
                if y > x {
                    Some(Self::Float(*x))
                } else {
                    Some(Self::Float(*y))
                }
            }
            (Self::Ratio(x, a), Self::Ratio(y, b)) => {
                let first = *x as f64 / *a as f64;
                let second = *y as f64 / *b as f64;
                if first > second {
                    Some(Self::Percent(second))
                } else {
                    Some(Self::Percent(first))
                }
            }
            (Self::Percent(x), Self::Percent(y)) => {
                if y > x {
                    Some(Self::Percent(*x))
                } else {
                    Some(Self::Percent(*y))
                }
            }
            _ => None,
        }
    }

    /// add user stats with the other
    #[expect(clippy::cast_precision_loss)]
    pub fn stats_add(&mut self, other: &Self) -> Option<Self> {
        match (self, other) {
            (Self::Number(x), Self::Number(y)) => Some(Self::Number(*x + *y)),
            (Self::Float(x), Self::Float(y)) => Some(Self::Float(*x + *y)),
            (Self::Percent(x), Self::Percent(y)) => Some(Self::Percent(*x + *y)),
            (Self::Ratio(x, a), Self::Ratio(y, b)) => {
                let first = *x as f64 / *a as f64;
                let second = *y as f64 / *b as f64;
                Some(Self::Percent(first + second))
            }
            (Self::Percent(x), Self::Ratio(y, b)) => {
                let ratio = *y as f64 / *b as f64;
                Some(Self::Percent(*x + ratio))
            }
            (Self::Ratio(x, a), Self::Percent(y)) => {
                let ratio = *x as f64 / *a as f64;
                Some(Self::Percent(ratio + *y))
            }
            _ => None,
        }
    }
}

impl fmt::Display for UserStatsValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self {
            UserStatsValue::Number(n) => write!(f, "{n}"),
            UserStatsValue::Float(n) => write!(f, "{}", crate::monitors::stats::prettify_float(*n)),
            UserStatsValue::Percent(n) => write!(f, "{:.3}%", n * 100.0),
            UserStatsValue::String(s) => write!(f, "{s}"),
            UserStatsValue::Ratio(a, b) => {
                if *b == 0 {
                    write!(f, "{a}/{b}")
                } else {
                    write!(f, "{a}/{b} ({}%)", a * 100 / b)
                }
            }
        }
    }
}
