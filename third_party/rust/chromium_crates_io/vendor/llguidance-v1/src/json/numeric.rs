use anyhow::{anyhow, Result};
use regex_syntax::escape;

use super::schema::NumberSchema;

/// coef * 10^-exp
#[cfg_attr(test, derive(PartialEq))]
#[derive(Debug, Clone)]
pub struct Decimal {
    pub coef: u32,
    pub exp: u32,
}

impl Decimal {
    fn new(coef: u32, exp: u32) -> Self {
        if coef == 0 {
            return Decimal { coef: 0, exp: 0 };
        }
        // reduce to simplest form
        let mut coef = coef;
        let mut exp = exp;
        while exp > 0 && coef % 10 == 0 {
            coef /= 10;
            exp -= 1;
        }
        Decimal { coef, exp }
    }

    pub fn lcm(&self, other: &Decimal) -> Decimal {
        if self.coef == 0 || other.coef == 0 {
            return Decimal::new(0, 0);
        }
        let a = self.coef * 10u32.pow(other.exp.saturating_sub(self.exp));
        let b = other.coef * 10u32.pow(self.exp.saturating_sub(other.exp));
        let coef = (a * b) / gcd(a, b);
        Decimal::new(coef, self.exp.max(other.exp))
    }

    pub fn to_f64(&self) -> f64 {
        self.coef as f64 / 10.0f64.powi(self.exp as i32)
    }
}

impl TryFrom<f64> for Decimal {
    type Error = anyhow::Error;

    fn try_from(value: f64) -> Result<Self, Self::Error> {
        if value < 0.0 {
            return Err(anyhow!("Value for 'multipleOf' must be non-negative"));
        }
        let mut value = value;
        let mut exp = 0;
        while value.fract() != 0.0 {
            value *= 10.0;
            exp += 1;
        }
        if value > u32::MAX as f64 {
            return Err(anyhow!(
                "Value for 'multipleOf' has too many digits: {}",
                value
            ));
        }
        Ok(Decimal::new(value as u32, exp))
    }
}

fn gcd(a: u32, b: u32) -> u32 {
    if b == 0 {
        a
    } else {
        gcd(b, a % b)
    }
}

fn mk_or(parts: Vec<String>) -> String {
    if parts.len() == 1 {
        parts[0].clone()
    } else {
        format!("({})", parts.join("|"))
    }
}

fn num_digits(n: i64) -> usize {
    n.abs().to_string().len()
}

pub fn rx_int_range(left: Option<i64>, right: Option<i64>) -> Result<String> {
    match (left, right) {
        (None, None) => Ok("-?(0|[1-9][0-9]*)".to_string()),
        (Some(left), None) => {
            if left < 0 {
                Ok(mk_or(vec![
                    rx_int_range(Some(left), Some(-1))?,
                    rx_int_range(Some(0), None)?,
                ]))
            } else {
                let max_value = "9"
                    .repeat(num_digits(left))
                    .parse::<i64>()
                    .map_err(|e| anyhow!("Failed to parse max value for left {}: {}", left, e))?;
                Ok(mk_or(vec![
                    rx_int_range(Some(left), Some(max_value))?,
                    format!("[1-9][0-9]{{{},}}", num_digits(left)),
                ]))
            }
        }
        (None, Some(right)) => {
            if right >= 0 {
                Ok(mk_or(vec![
                    rx_int_range(Some(0), Some(right))?,
                    rx_int_range(None, Some(-1))?,
                ]))
            } else {
                Ok(format!("-{}", rx_int_range(Some(-right), None)?))
            }
        }
        (Some(left), Some(right)) => {
            if left > right {
                return Err(anyhow!(
                    "Invalid range: left ({}) cannot be greater than right ({})",
                    left,
                    right
                ));
            }
            if left < 0 {
                if right < 0 {
                    Ok(format!("(-{})", rx_int_range(Some(-right), Some(-left))?))
                } else {
                    Ok(format!(
                        "(-{}|{})",
                        rx_int_range(Some(0), Some(-left))?,
                        rx_int_range(Some(0), Some(right))?
                    ))
                }
            } else if num_digits(left) == num_digits(right) {
                let l = left.to_string();
                let r = right.to_string();
                if left == right {
                    return Ok(format!("({l})"));
                }

                let lpref = &l[..l.len() - 1];
                let lx = &l[l.len() - 1..];
                let rpref = &r[..r.len() - 1];
                let rx = &r[r.len() - 1..];

                if lpref == rpref {
                    return Ok(format!("({lpref}[{lx}-{rx}])"));
                }

                let mut left_rec = lpref.parse::<i64>().unwrap_or(0);
                let mut right_rec = rpref.parse::<i64>().unwrap_or(0);
                if left_rec >= right_rec {
                    return Err(anyhow!(
                        "Invalid recursive range: left_rec ({}) must be less than right_rec ({})",
                        left_rec,
                        right_rec
                    ));
                }

                let mut parts = Vec::new();

                if lx != "0" {
                    left_rec += 1;
                    parts.push(format!("{lpref}[{lx}-9]"));
                }

                if rx != "9" {
                    right_rec -= 1;
                    parts.push(format!("{rpref}[0-{rx}]"));
                }

                if left_rec <= right_rec {
                    let inner = rx_int_range(Some(left_rec), Some(right_rec))?;
                    parts.push(format!("{inner}[0-9]"));
                }

                Ok(mk_or(parts))
            } else {
                let break_point = 10_i64
                    .checked_pow(num_digits(left) as u32)
                    .ok_or_else(|| anyhow!("Overflow when calculating break point"))?
                    - 1;
                Ok(mk_or(vec![
                    rx_int_range(Some(left), Some(break_point))?,
                    rx_int_range(Some(break_point + 1), Some(right))?,
                ]))
            }
        }
    }
}

fn lexi_x_to_9(x: &str, incl: bool) -> Result<String> {
    if incl {
        if x.is_empty() {
            Ok("[0-9]*".to_string())
        } else if x.len() == 1 {
            Ok(format!("[{x}-9][0-9]*"))
        } else {
            let x0 = x
                .chars()
                .next()
                .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?
                .to_digit(10)
                .ok_or_else(|| anyhow!("Failed to parse character as digit"))?;
            let x_rest = &x[1..];
            let mut parts = vec![format!(
                "{}{}",
                x.chars()
                    .next()
                    .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?,
                lexi_x_to_9(x_rest, incl)?
            )];
            if x0 < 9 {
                parts.push(format!("[{}-9][0-9]*", x0 + 1));
            }
            Ok(mk_or(parts))
        }
    } else if x.is_empty() {
        Ok("[0-9]*[1-9]".to_string())
    } else {
        let x0 = x
            .chars()
            .next()
            .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?
            .to_digit(10)
            .ok_or_else(|| anyhow!("Failed to parse character as digit"))?;
        let x_rest = &x[1..];
        let mut parts = vec![format!(
            "{}{}",
            x.chars()
                .next()
                .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?,
            lexi_x_to_9(x_rest, incl)?
        )];
        if x0 < 9 {
            parts.push(format!("[{}-9][0-9]*", x0 + 1));
        }
        Ok(mk_or(parts))
    }
}

fn lexi_0_to_x(x: &str, incl: bool) -> Result<String> {
    if x.is_empty() {
        if incl {
            Ok("".to_string())
        } else {
            Err(anyhow!("Inclusive flag must be true for an empty string"))
        }
    } else {
        let x0 = x
            .chars()
            .next()
            .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?
            .to_digit(10)
            .ok_or_else(|| anyhow!("Failed to parse character as digit"))?;
        let x_rest = &x[1..];

        if !incl && x.len() == 1 {
            if x0 == 0 {
                return Err(anyhow!(
                    "x0 must be greater than 0 for non-inclusive single character"
                ));
            }
            return Ok(format!("[0-{}][0-9]*", x0 - 1));
        }

        let mut parts = vec![format!(
            "{}{}",
            x.chars()
                .next()
                .ok_or_else(|| anyhow!("String x is unexpectedly empty"))?,
            lexi_0_to_x(x_rest, incl)?
        )];
        if x0 > 0 {
            parts.push(format!("[0-{}][0-9]*", x0 - 1));
        }
        Ok(mk_or(parts))
    }
}

fn lexi_range(ld: &str, rd: &str, ld_incl: bool, rd_incl: bool) -> Result<String> {
    if ld.len() != rd.len() {
        return Err(anyhow!("ld and rd must have the same length"));
    }
    if ld == rd {
        if ld_incl && rd_incl {
            Ok(ld.to_string())
        } else {
            Err(anyhow!(
                "Empty range when ld equals rd and not both inclusive"
            ))
        }
    } else {
        let l0 = ld
            .chars()
            .next()
            .ok_or_else(|| anyhow!("ld is unexpectedly empty"))?
            .to_digit(10)
            .ok_or_else(|| anyhow!("Failed to parse character as digit"))?;
        let r0 = rd
            .chars()
            .next()
            .ok_or_else(|| anyhow!("rd is unexpectedly empty"))?
            .to_digit(10)
            .ok_or_else(|| anyhow!("Failed to parse character as digit"))?;
        if l0 == r0 {
            let ld_rest = &ld[1..];
            let rd_rest = &rd[1..];
            Ok(format!(
                "{}{}",
                ld.chars()
                    .next()
                    .ok_or_else(|| anyhow!("ld is unexpectedly empty"))?,
                lexi_range(ld_rest, rd_rest, ld_incl, rd_incl)?
            ))
        } else {
            if l0 >= r0 {
                return Err(anyhow!("l0 must be less than r0"));
            }
            let ld_rest = ld[1..].trim_end_matches('0');
            let mut parts = vec![format!(
                "{}{}",
                ld.chars()
                    .next()
                    .ok_or_else(|| anyhow!("ld is unexpectedly empty"))?,
                lexi_x_to_9(ld_rest, ld_incl)?
            )];
            if l0 + 1 < r0 {
                parts.push(format!("[{}-{}][0-9]*", l0 + 1, r0 - 1));
            }
            let rd_rest = rd[1..].trim_end_matches('0');
            if !rd_rest.is_empty() || rd_incl {
                parts.push(format!(
                    "{}{}",
                    rd.chars()
                        .next()
                        .ok_or_else(|| anyhow!("rd is unexpectedly empty"))?,
                    lexi_0_to_x(rd_rest, rd_incl)?
                ));
            }
            Ok(mk_or(parts))
        }
    }
}

fn float_to_str(f: f64) -> String {
    format!("{f}")
}

pub fn rx_float_range(
    left: Option<f64>,
    right: Option<f64>,
    left_inclusive: bool,
    right_inclusive: bool,
) -> Result<String> {
    match (left, right) {
        (None, None) => Ok("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?".to_string()),
        (Some(left), None) => {
            if left < 0.0 {
                Ok(mk_or(vec![
                    rx_float_range(Some(left), Some(0.0), left_inclusive, false)?,
                    rx_float_range(Some(0.0), None, true, false)?,
                ]))
            } else {
                let left_int_part = left as i64;
                Ok(mk_or(vec![
                    rx_float_range(
                        Some(left),
                        Some(10f64.powi(num_digits(left_int_part) as i32)),
                        left_inclusive,
                        false,
                    )?,
                    format!("[1-9][0-9]{{{},}}(\\.[0-9]+)?", num_digits(left_int_part)),
                ]))
            }
        }
        (None, Some(right)) => {
            if right == 0.0 {
                let r = format!("-{}", rx_float_range(Some(0.0), None, false, false)?);
                if right_inclusive {
                    Ok(mk_or(vec![r, "0".to_string()]))
                } else {
                    Ok(r)
                }
            } else if right > 0.0 {
                Ok(mk_or(vec![
                    format!("-{}", rx_float_range(Some(0.0), None, false, false)?),
                    rx_float_range(Some(0.0), Some(right), true, right_inclusive)?,
                ]))
            } else {
                Ok(format!(
                    "-{}",
                    rx_float_range(Some(-right), None, right_inclusive, false)?
                ))
            }
        }
        (Some(left), Some(right)) => {
            if left > right {
                return Err(anyhow!(
                    "Invalid range: left ({}) cannot be greater than right ({})",
                    left,
                    right
                ));
            }
            if left == right {
                if left_inclusive && right_inclusive {
                    Ok(format!("({})", escape(&float_to_str(left))))
                } else {
                    Err(anyhow!(
                        "Empty range when left equals right and not both inclusive"
                    ))
                }
            } else if left < 0.0 {
                if right < 0.0 {
                    Ok(format!(
                        "(-{})",
                        rx_float_range(Some(-right), Some(-left), right_inclusive, left_inclusive)?
                    ))
                } else {
                    let mut parts = vec![];
                    let neg_part = rx_float_range(Some(0.0), Some(-left), false, left_inclusive)?;
                    parts.push(format!("(-{neg_part})"));

                    if right > 0.0 || right_inclusive {
                        let pos_part =
                            rx_float_range(Some(0.0), Some(right), true, right_inclusive)?;
                        parts.push(pos_part);
                    }
                    Ok(mk_or(parts))
                }
            } else {
                let l = float_to_str(left);
                let r = float_to_str(right);
                if l == r {
                    return Err(anyhow!(
                        "Unexpected equality of left and right string representations"
                    ));
                }
                if !left.is_finite() || !right.is_finite() {
                    return Err(anyhow!("Infinite numbers not supported"));
                }

                let mut left_rec: i64 = l
                    .split('.')
                    .next()
                    .ok_or_else(|| anyhow!("Failed to split left integer part"))?
                    .parse()
                    .map_err(|e| anyhow!("Failed to parse left integer part: {}", e))?;
                let right_rec: i64 = r
                    .split('.')
                    .next()
                    .ok_or_else(|| anyhow!("Failed to split right integer part"))?
                    .parse()
                    .map_err(|e| anyhow!("Failed to parse right integer part: {}", e))?;

                let mut ld = l.split('.').nth(1).unwrap_or("").to_string();
                let mut rd = r.split('.').nth(1).unwrap_or("").to_string();

                if left_rec == right_rec {
                    while ld.len() < rd.len() {
                        ld.push('0');
                    }
                    while rd.len() < ld.len() {
                        rd.push('0');
                    }
                    let suff = format!(
                        "\\.{}",
                        lexi_range(&ld, &rd, left_inclusive, right_inclusive)?
                    );
                    if ld.parse::<i64>().unwrap_or(0) == 0 {
                        Ok(format!("({left_rec}({suff})?)"))
                    } else {
                        Ok(format!("({left_rec}{suff})"))
                    }
                } else {
                    let mut parts = vec![];
                    if !ld.is_empty() || !left_inclusive {
                        parts.push(format!(
                            "({}\\.{})",
                            left_rec,
                            lexi_x_to_9(&ld, left_inclusive)?
                        ));
                        left_rec += 1;
                    }

                    if right_rec > left_rec {
                        let inner = rx_int_range(Some(left_rec), Some(right_rec - 1))?;
                        parts.push(format!("({inner}(\\.[0-9]+)?)"));
                    }

                    if !rd.is_empty() {
                        parts.push(format!(
                            "({}(\\.{})?)",
                            right_rec,
                            lexi_0_to_x(&rd, right_inclusive)?
                        ));
                    } else if right_inclusive {
                        parts.push(format!("{right_rec}(\\.0+)?"));
                    }

                    Ok(mk_or(parts))
                }
            }
        }
    }
}

pub fn check_number_bounds(num: &NumberSchema) -> Result<(), String> {
    let (minimum, exclusive_minimum) = num.get_minimum();
    let (maximum, exclusive_maximum) = num.get_maximum();
    if let (Some(min), Some(max)) = (minimum, maximum) {
        if min > max {
            return Err(format!("minimum ({min}) is greater than maximum ({max})"));
        }
        if min == max && (exclusive_minimum || exclusive_maximum) {
            let minimum_repr = if exclusive_minimum {
                "exclusiveMinimum"
            } else {
                "minimum"
            };
            let maximum_repr = if exclusive_maximum {
                "exclusiveMaximum"
            } else {
                "maximum"
            };
            return Err(format!(
                "{minimum_repr} ({min}) is equal to {maximum_repr} ({max})"
            ));
        }
    }
    if let Some(d) = num.multiple_of.as_ref() {
        if d.coef == 0 {
            if let Some(min) = minimum {
                if min > 0.0 || (exclusive_minimum && min >= 0.0) {
                    return Err(format!(
                        "minimum ({min}) is greater than 0, but multipleOf is 0"
                    ));
                }
            };
            if let Some(max) = maximum {
                if max < 0.0 || (exclusive_maximum && max <= 0.0) {
                    return Err(format!(
                        "maximum ({max}) is less than 0, but multipleOf is 0"
                    ));
                }
            };
            return Ok(());
        }
        // If interval is not unbounded in at least one direction, check if the range contains a multiple of multipleOf
        if let (Some(min), Some(max)) = (minimum, maximum) {
            let step = d.to_f64();
            // Adjust the range depending on whether it's exclusive or not
            let min = {
                let first_num_ge_min = (min / step).ceil() * step;
                let adjusted_min = if exclusive_minimum && first_num_ge_min == min {
                    first_num_ge_min + step
                } else {
                    first_num_ge_min
                };
                if num.integer {
                    adjusted_min.ceil()
                } else {
                    adjusted_min
                }
            };
            let max = {
                let first_num_le_max = (max / step).floor() * step;
                let adjusted_max = if exclusive_maximum && first_num_le_max == max {
                    first_num_le_max - step
                } else {
                    first_num_le_max
                };
                if num.integer {
                    adjusted_max.floor()
                } else {
                    adjusted_max
                }
            };
            if min > max {
                return Err(format!(
                    "range {}{}, {}{} does not contain a multiple of {}",
                    if exclusive_minimum { "(" } else { "[" },
                    min,
                    max,
                    if exclusive_maximum { ")" } else { "]" },
                    step
                ));
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod test_ranges {
    use super::{rx_float_range, rx_int_range};
    use regex::Regex;

    fn do_test_int_range(rx: &str, left: Option<i64>, right: Option<i64>) {
        let re = Regex::new(&format!("^{rx}$")).unwrap();
        for n in (left.unwrap_or(0) - 1000)..=(right.unwrap_or(0) + 1000) {
            let matches = re.is_match(&n.to_string());
            let expected =
                (left.is_none() || left.unwrap() <= n) && (right.is_none() || n <= right.unwrap());
            if expected != matches {
                let range_str = match (left, right) {
                    (Some(l), Some(r)) => format!("[{l}, {r}]"),
                    (Some(l), None) => format!("[{l}, ∞)"),
                    (None, Some(r)) => format!("(-∞, {r}]"),
                    (None, None) => "(-∞, ∞)".to_string(),
                };
                if matches {
                    panic!("{n} not in range {range_str} but matches {rx:?}");
                } else {
                    panic!("{n} in range {range_str} but does not match {rx:?}");
                }
            }
        }
    }

    #[test]
    fn test_int_range() {
        let cases = vec![
            (Some(0), Some(9)),
            (Some(1), Some(7)),
            (Some(0), Some(99)),
            (Some(13), Some(170)),
            (Some(13), Some(17)),
            (Some(13), Some(27)),
            (Some(13), Some(57)),
            (Some(72), Some(91)),
            (Some(723), Some(915)),
            (Some(23), Some(915)),
            (Some(-1), Some(915)),
            (Some(-9), Some(9)),
            (Some(-3), Some(3)),
            (Some(-3), Some(0)),
            (Some(-72), Some(13)),
            (None, Some(0)),
            (None, Some(7)),
            (None, Some(23)),
            (None, Some(725)),
            (None, Some(-1)),
            (None, Some(-17)),
            (None, Some(-283)),
            (Some(0), None),
            (Some(2), None),
            (Some(33), None),
            (Some(234), None),
            (Some(-1), None),
            (Some(-87), None),
            (Some(-329), None),
            (None, None),
            (Some(-13), Some(-13)),
            (Some(-1), Some(-1)),
            (Some(0), Some(0)),
            (Some(1), Some(1)),
            (Some(13), Some(13)),
        ];

        for (left, right) in cases {
            let rx = rx_int_range(left, right).unwrap();
            do_test_int_range(&rx, left, right);
        }
    }

    fn do_test_float_range(
        rx: &str,
        left: Option<f64>,
        right: Option<f64>,
        left_inclusive: bool,
        right_inclusive: bool,
    ) {
        let re = Regex::new(&format!("^{rx}$")).unwrap();
        let left_int = left.map(|x| {
            let left_int = x.ceil() as i64;
            if !left_inclusive && x == left_int as f64 {
                left_int + 1
            } else {
                left_int
            }
        });
        let right_int = right.map(|x| {
            let right_int = x.floor() as i64;
            if !right_inclusive && x == right_int as f64 {
                right_int - 1
            } else {
                right_int
            }
        });
        do_test_int_range(rx, left_int, right_int);

        let eps1 = 0.0000001;
        let eps2 = 0.01;
        let test_cases = vec![
            left.unwrap_or(-1000.0),
            right.unwrap_or(1000.0),
            0.0,
            left_int.unwrap_or(-1000) as f64,
            right_int.unwrap_or(1000) as f64,
        ];
        for x in test_cases {
            for offset in [0.0, -eps1, eps1, -eps2, eps2, 1.0, -1.0].iter() {
                let n = x + offset;
                let matches = re.is_match(&n.to_string());
                let left_cond =
                    left.is_none() || left.unwrap() < n || (left.unwrap() == n && left_inclusive);
                let right_cond = right.is_none()
                    || right.unwrap() > n
                    || (right.unwrap() == n && right_inclusive);
                let expected = left_cond && right_cond;
                if expected != matches {
                    let lket = if left_inclusive { "[" } else { "(" };
                    let rket = if right_inclusive { "]" } else { ")" };
                    let range_str = match (left, right) {
                        (Some(l), Some(r)) => format!("{lket}{l}, {r}{rket}"),
                        (Some(l), None) => format!("{lket}{l}, ∞)"),
                        (None, Some(r)) => format!("(-∞, {r}{rket}"),
                        (None, None) => "(-∞, ∞)".to_string(),
                    };
                    if matches {
                        panic!("{n} not in range {range_str} but matches {rx:?}");
                    } else {
                        panic!("{n} in range {range_str} but does not match {rx:?}");
                    }
                }
            }
        }
    }

    #[test]
    fn test_float_range() {
        let cases = vec![
            (Some(0.0), Some(10.0)),
            (Some(-10.0), Some(0.0)),
            (Some(0.5), Some(0.72)),
            (Some(0.5), Some(1.72)),
            (Some(0.5), Some(1.32)),
            (Some(0.45), Some(0.5)),
            (Some(0.3245), Some(0.325)),
            (Some(0.443245), Some(0.44325)),
            (Some(1.0), Some(2.34)),
            (Some(1.33), Some(2.0)),
            (Some(1.0), Some(10.34)),
            (Some(1.33), Some(10.0)),
            (Some(-1.33), Some(10.0)),
            (Some(-17.23), Some(-1.33)),
            (Some(-1.23), Some(-1.221)),
            (Some(-10.2), Some(45293.9)),
            (None, Some(0.0)),
            (None, Some(1.0)),
            (None, Some(1.5)),
            (None, Some(1.55)),
            (None, Some(-17.23)),
            (None, Some(-1.33)),
            (None, Some(-1.23)),
            (None, Some(103.74)),
            (None, Some(100.0)),
            (Some(0.0), None),
            (Some(1.0), None),
            (Some(1.5), None),
            (Some(1.55), None),
            (Some(-17.23), None),
            (Some(-1.33), None),
            (Some(-1.23), None),
            (Some(103.74), None),
            (Some(100.0), None),
            (None, None),
            (Some(-103.4), Some(-103.4)),
            (Some(-27.0), Some(-27.0)),
            (Some(-1.5), Some(-1.5)),
            (Some(-1.0), Some(-1.0)),
            (Some(0.0), Some(0.0)),
            (Some(1.0), Some(1.0)),
            (Some(1.5), Some(1.5)),
            (Some(27.0), Some(27.0)),
            (Some(103.4), Some(103.4)),
        ];

        for (left, right) in cases {
            for left_inclusive in [true, false].iter() {
                for right_inclusive in [true, false].iter() {
                    match (left, right) {
                        (Some(left), Some(right))
                            if left == right && !(*left_inclusive && *right_inclusive) =>
                        {
                            assert!(rx_float_range(
                                Some(left),
                                Some(right),
                                *left_inclusive,
                                *right_inclusive
                            )
                            .is_err());
                        }
                        _ => {
                            let rx = rx_float_range(left, right, *left_inclusive, *right_inclusive)
                                .unwrap();
                            do_test_float_range(
                                &rx,
                                left,
                                right,
                                *left_inclusive,
                                *right_inclusive,
                            );
                        }
                    }
                }
            }
        }
    }
}

#[cfg(test)]
mod test_decimal {
    use super::Decimal;

    #[test]
    fn test_from_f64() {
        let cases = vec![
            (0.0, Decimal { coef: 0, exp: 0 }),
            (1.0, Decimal { coef: 1, exp: 0 }),
            (10.0, Decimal { coef: 10, exp: 0 }),
            (100.0, Decimal { coef: 100, exp: 0 }),
            (0.1, Decimal { coef: 1, exp: 1 }),
            (0.01, Decimal { coef: 1, exp: 2 }),
            (1.1, Decimal { coef: 11, exp: 1 }),
            (1.01, Decimal { coef: 101, exp: 2 }),
            (10.1, Decimal { coef: 101, exp: 1 }),
            (10.01, Decimal { coef: 1001, exp: 2 }),
            (100.1, Decimal { coef: 1001, exp: 1 }),
            (
                100.01,
                Decimal {
                    coef: 10001,
                    exp: 2,
                },
            ),
        ];
        for (f, d) in cases {
            assert_eq!(Decimal::try_from(f).unwrap(), d);
        }
    }

    #[test]
    fn test_simplified() {
        let cases = vec![
            (Decimal::new(10, 1), Decimal { coef: 1, exp: 0 }),
            (Decimal::new(100, 2), Decimal { coef: 1, exp: 0 }),
            (Decimal::new(100, 1), Decimal { coef: 10, exp: 0 }),
            (Decimal::new(1000, 3), Decimal { coef: 1, exp: 0 }),
            (Decimal::new(1000, 2), Decimal { coef: 10, exp: 0 }),
            (Decimal::new(1000, 1), Decimal { coef: 100, exp: 0 }),
            (Decimal::new(10000, 4), Decimal { coef: 1, exp: 0 }),
            (Decimal::new(10000, 3), Decimal { coef: 10, exp: 0 }),
            (Decimal::new(10000, 2), Decimal { coef: 100, exp: 0 }),
            (Decimal::new(10000, 1), Decimal { coef: 1000, exp: 0 }),
        ];
        for (d, s) in cases {
            assert_eq!(d, s);
        }
    }

    #[test]
    fn test_lcm() {
        let cases = vec![
            (2.0, 3.0, 6.0),
            (0.5, 1.5, 1.5),
            (0.5, 0.5, 0.5),
            (0.5, 0.25, 0.5),
            (0.3, 0.2, 0.6),
            (0.3, 0.4, 1.2),
            (0.05, 0.36, 1.8),
            (0.3, 14.0, 42.0),
        ];
        for (a, b, c) in cases {
            let a = Decimal::try_from(a).unwrap();
            let b = Decimal::try_from(b).unwrap();
            let c = Decimal::try_from(c).unwrap();
            assert_eq!(a.lcm(&b), c);
        }
    }
}

#[cfg(test)]
mod test_number_bounds {
    use crate::json::schema::NumberSchema;

    use super::{check_number_bounds, Decimal};

    #[derive(Debug)]
    struct Case {
        minimum: Option<f64>,
        maximum: Option<f64>,
        exclusive_minimum: bool,
        exclusive_maximum: bool,
        integer: bool,
        multiple_of: Option<Decimal>,
        ok: bool,
    }

    impl Case {
        fn to_number_schema(&self) -> NumberSchema {
            NumberSchema {
                minimum: if self.exclusive_minimum {
                    None
                } else {
                    self.minimum
                },
                maximum: if self.exclusive_maximum {
                    None
                } else {
                    self.maximum
                },
                exclusive_minimum: if self.exclusive_minimum {
                    self.minimum
                } else {
                    None
                },
                exclusive_maximum: if self.exclusive_maximum {
                    self.maximum
                } else {
                    None
                },
                integer: self.integer,
                multiple_of: self.multiple_of.clone(),
            }
        }
    }

    #[test]
    fn test_check_number_bounds() {
        let cases = vec![
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: true,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: true,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: true,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: false,
                exclusive_maximum: true,
                integer: false,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: true,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: true,
                exclusive_maximum: true,
                integer: false,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: false,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: true,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: true,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: true,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: false,
                exclusive_maximum: true,
                integer: true,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: false,
            },
            Case {
                minimum: Some(5.5),
                maximum: Some(6.0),
                exclusive_minimum: true,
                exclusive_maximum: true,
                integer: true,
                multiple_of: Decimal::try_from(0.5).ok(),
                ok: false,
            },
            // Zero bounds
            Case {
                minimum: Some(0.0),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(2.0).ok(),
                ok: true,
            },
            Case {
                minimum: Some(0.0),
                maximum: Some(10.0),
                exclusive_minimum: true,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(2.0).ok(),
                ok: true,
            },
            Case {
                minimum: Some(0.0),
                maximum: Some(10.0),
                exclusive_minimum: true,
                exclusive_maximum: true,
                integer: true,
                multiple_of: Decimal::try_from(2.0).ok(),
                ok: true,
            },
            // Negative ranges
            Case {
                minimum: Some(-10.0),
                maximum: Some(-5.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(1.0).ok(),
                ok: true,
            },
            Case {
                minimum: Some(-10.0),
                maximum: Some(-5.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.1).ok(),
                ok: true,
            },
            // Tiny ranges
            Case {
                minimum: Some(1.0),
                maximum: Some(1.01),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.005).ok(),
                ok: true,
            },
            Case {
                minimum: Some(1.0),
                maximum: Some(1.01),
                exclusive_minimum: true,
                exclusive_maximum: true,
                integer: false,
                multiple_of: Decimal::try_from(0.005).ok(),
                ok: true,
            },
            Case {
                minimum: Some(1.0),
                maximum: Some(1.01),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.01).ok(),
                ok: true,
            },
            Case {
                minimum: Some(1.0),
                maximum: Some(1.01),
                exclusive_minimum: true,
                exclusive_maximum: true,
                integer: false,
                multiple_of: Decimal::try_from(0.01).ok(),
                ok: false,
            },
            // Large ranges
            Case {
                minimum: Some(1.0),
                maximum: Some(1e9),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(100000.0).ok(),
                ok: true,
            },
            // Non-finite values
            Case {
                minimum: Some(f64::NEG_INFINITY),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(1.0).ok(),
                ok: true,
            },
            Case {
                minimum: Some(0.0),
                maximum: Some(f64::INFINITY),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: true,
                multiple_of: Decimal::try_from(1.0).ok(),
                ok: true,
            },
            // `multiple_of` edge cases
            Case {
                minimum: Some(1.0),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(1.0).ok(),
                ok: true,
            },
            Case {
                minimum: Some(1.0),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.3).ok(),
                ok: true,
            },
            Case {
                minimum: Some(1.0),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.0).ok(),
                ok: false,
            },
            Case {
                minimum: Some(0.0),
                maximum: Some(10.0),
                exclusive_minimum: true,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.0).ok(),
                ok: false,
            },
            Case {
                minimum: Some(0.0),
                maximum: Some(10.0),
                exclusive_minimum: false,
                exclusive_maximum: false,
                integer: false,
                multiple_of: Decimal::try_from(0.0).ok(),
                ok: true,
            },
        ];
        for case in cases {
            let result = check_number_bounds(&case.to_number_schema());
            assert_eq!(
                result.is_ok(),
                case.ok,
                "Failed for case {case:?} with result {result:?}"
            );
        }
    }
}
