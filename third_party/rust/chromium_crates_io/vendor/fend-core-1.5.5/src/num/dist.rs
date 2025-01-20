use crate::error::{FendError, Interrupt};
use crate::interrupt::{test_int, Never};
use crate::num::bigrat::BigRat;
use crate::num::complex::{self, Complex};
use crate::result::FResult;
use crate::serialize::{Deserialize, Serialize};
use std::cmp::Ordering;
use std::fmt::Write;
use std::ops::Neg;
use std::{fmt, io};

use super::real::Real;
use super::{Base, Exact, FormattingStyle};

#[derive(Clone)]
pub(crate) struct Dist {
	// invariant: probabilities must sum to 1
	parts: Vec<(Complex, BigRat)>,
}

impl Dist {
	pub(crate) fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.parts.len().serialize(write)?;
		for (a, b) in &self.parts {
			a.serialize(write)?;
			b.serialize(write)?;
		}
		Ok(())
	}

	pub(crate) fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let len = usize::deserialize(read)?;
		let mut parts = Vec::with_capacity(len);
		for _ in 0..len {
			let k = Complex::deserialize(read)?;
			let v = BigRat::deserialize(read)?;
			parts.push((k, v));
		}
		Ok(Self { parts })
	}

	pub(crate) fn one_point(self) -> FResult<Complex> {
		if self.parts.len() == 1 {
			Ok(self.parts.into_iter().next().unwrap().0)
		} else {
			Err(FendError::ProbabilityDistributionsNotAllowed)
		}
	}

	pub(crate) fn one_point_ref(&self) -> FResult<&Complex> {
		if self.parts.len() == 1 {
			Ok(&self.parts[0].0)
		} else {
			Err(FendError::ProbabilityDistributionsNotAllowed)
		}
	}

	pub(crate) fn new_die<I: Interrupt>(count: u32, faces: u32, int: &I) -> FResult<Self> {
		assert!(count != 0);
		assert!(faces != 0);
		if count > 1 {
			let mut result = Self::new_die(1, faces, int)?;
			for _ in 1..count {
				test_int(int)?;
				result = Exact::new(result, true)
					.add(&Exact::new(Self::new_die(1, faces, int)?, true), int)?
					.value;
			}
			return Ok(result);
		}
		let mut parts = Vec::new();
		let probability = BigRat::from(1).div(&BigRat::from(u64::from(faces)), int)?;
		for face in 1..=faces {
			test_int(int)?;
			parts.push((Complex::from(u64::from(face)), probability.clone()));
		}
		Ok(Self { parts })
	}

	pub(crate) fn equals_int<I: Interrupt>(&self, val: u64, int: &I) -> FResult<bool> {
		Ok(self.parts.len() == 1
			&& self.parts[0].0.compare(&val.into(), int)? == Some(Ordering::Equal))
	}

	#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
	pub(crate) fn sample<I: Interrupt>(self, ctx: &crate::Context, int: &I) -> FResult<Self> {
		if self.parts.len() == 1 {
			return Ok(self);
		}
		let mut random = ctx.random_u32.ok_or(FendError::RandomNumbersNotAvailable)?();
		let mut res = None;
		for (k, v) in self.parts {
			random = random.saturating_sub((v.into_f64(int)? * f64::from(u32::MAX)) as u32);
			if random == 0 {
				return Ok(Self::from(k));
			}
			res = Some(Self::from(k));
		}
		res.ok_or(FendError::EmptyDistribution)
	}

	pub(crate) fn mean<I: Interrupt>(self, int: &I) -> FResult<Self> {
		if self.parts.is_empty() {
			return Err(FendError::EmptyDistribution);
		} else if self.parts.len() == 1 {
			return Ok(self);
		}

		let mut result = Exact::new(Complex::from(0), true);
		for (k, v) in self.parts {
			result = Exact::new(k, true)
				.mul(&Exact::new(Complex::from(Real::from(v)), true), int)?
				.add(result, int)?;
		}

		Ok(Self::from(result.value))
	}

	#[allow(
		clippy::cast_possible_truncation,
		clippy::cast_sign_loss,
		clippy::too_many_arguments
	)]
	pub(crate) fn format<I: Interrupt>(
		&self,
		exact: bool,
		style: FormattingStyle,
		base: Base,
		use_parentheses: complex::UseParentheses,
		out: &mut String,
		ctx: &crate::Context,
		int: &I,
	) -> FResult<Exact<()>> {
		if self.parts.len() == 1 {
			let res = self.parts[0].0.format(
				exact,
				style,
				base,
				use_parentheses,
				ctx.decimal_separator,
				int,
			)?;
			write!(out, "{}", res.value)?;
			Ok(Exact::new((), res.exact))
		} else {
			let mut ordered_kvs = vec![];
			let mut max_prob = 0.0;
			for (n, prob) in &self.parts {
				let prob_f64 = prob.clone().into_f64(int)?;
				if prob_f64 > max_prob {
					max_prob = prob_f64;
				}
				ordered_kvs.push((n, prob, prob_f64));
			}
			ordered_kvs.sort_unstable_by(|(a, _, _), (b, _, _)| {
				a.compare(b, &Never).unwrap().unwrap_or(Ordering::Equal)
			});
			if ctx.output_mode == crate::OutputMode::SimpleText {
				write!(out, "{{ ")?;
			}
			let mut first = true;
			for (num, _prob, prob_f64) in ordered_kvs {
				let num = num
					.format(
						exact,
						style,
						base,
						use_parentheses,
						ctx.decimal_separator,
						int,
					)?
					.value
					.to_string();
				let prob_percentage = prob_f64 * 100.0;
				if ctx.output_mode == crate::OutputMode::TerminalFixedWidth {
					if !first {
						writeln!(out)?;
					}
					let mut bar = String::new();
					for _ in 0..(prob_f64 / max_prob * 30.0).min(30.0) as u32 {
						bar.push('#');
					}
					write!(out, "{num:>3}: {prob_percentage:>5.2}%  {bar}")?;
				} else {
					if !first {
						write!(out, ", ")?;
					}
					write!(out, "{num}: {prob_percentage:.2}%")?;
				}
				if first {
					first = false;
				}
			}
			if ctx.output_mode == crate::OutputMode::SimpleText {
				write!(out, " }}")?;
			}
			// TODO check exactness
			Ok(Exact::new((), true))
		}
	}

	fn bop<I: Interrupt>(
		self,
		rhs: &Self,
		mut f: impl FnMut(&Complex, &Complex, &I) -> FResult<Complex>,
		int: &I,
	) -> FResult<Self> {
		let mut parts = Vec::<(Complex, BigRat)>::new();
		for (n1, p1) in &self.parts {
			for (n2, p2) in &rhs.parts {
				let n = f(n1, n2, int)?;
				let p = p1.clone().mul(p2, int)?;
				let mut found = false;
				for (k, prob) in &mut parts {
					if k.compare(&n, int)? == Some(Ordering::Equal) {
						*prob = prob.clone().add(p.clone(), int)?;
						found = true;
						break;
					}
				}
				if !found {
					parts.push((n, p));
				}
			}
		}
		Ok(Self { parts })
	}
}

impl Exact<Dist> {
	pub(crate) fn add<I: Interrupt>(self, rhs: &Self, int: &I) -> FResult<Self> {
		let self_exact = self.exact;
		let rhs_exact = rhs.exact;
		let mut exact = true;
		Ok(Self::new(
			self.value.bop(
				&rhs.value,
				|a, b, int| {
					let sum = Exact::new(a.clone(), self_exact)
						.add(Exact::new(b.clone(), rhs_exact), int)?;
					exact &= sum.exact;
					Ok(sum.value)
				},
				int,
			)?,
			exact,
		))
	}

	pub(crate) fn mul<I: Interrupt>(self, rhs: &Self, int: &I) -> FResult<Self> {
		let self_exact = self.exact;
		let rhs_exact = rhs.exact;
		let mut exact = true;
		Ok(Self::new(
			self.value.bop(
				&rhs.value,
				|a, b, int| {
					let sum = Exact::new(a.clone(), self_exact)
						.mul(&Exact::new(b.clone(), rhs_exact), int)?;
					exact &= sum.exact;
					Ok(sum.value)
				},
				int,
			)?,
			exact,
		))
	}

	pub(crate) fn div<I: Interrupt>(self, rhs: &Self, int: &I) -> FResult<Self> {
		let self_exact = self.exact;
		let rhs_exact = rhs.exact;
		let mut exact = true;
		Ok(Self::new(
			self.value.bop(
				&rhs.value,
				|a, b, int| {
					let sum = Exact::new(a.clone(), self_exact)
						.div(Exact::new(b.clone(), rhs_exact), int)?;
					exact &= sum.exact;
					Ok(sum.value)
				},
				int,
			)?,
			exact,
		))
	}
}

impl From<Complex> for Dist {
	fn from(v: Complex) -> Self {
		Self {
			parts: vec![(v, 1.into())],
		}
	}
}

impl From<Real> for Dist {
	fn from(v: Real) -> Self {
		Self::from(Complex::from(v))
	}
}

impl From<u64> for Dist {
	fn from(i: u64) -> Self {
		Self::from(Complex::from(i))
	}
}

impl fmt::Debug for Dist {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match self.one_point_ref() {
			Ok(complex) => write!(f, "{complex:?}"),
			Err(_) => write!(f, "dist {:?}", self.parts),
		}
	}
}

impl Neg for Dist {
	type Output = Self;
	fn neg(mut self) -> Self {
		for (k, _) in &mut self.parts {
			*k = -k.clone();
		}
		self
	}
}
