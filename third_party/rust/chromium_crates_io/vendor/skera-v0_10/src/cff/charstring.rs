//! Type 2 interpretation with separate closure, retained, and flattened actions.
//!
//! Retained programs borrow source bytes and record only edits. Literal spans
//! are sufficient for the common case. Retained calls require immediate literals;
//! dropped hint operands are deleted from whichever program pushed them, and
//! glyphs whose deletions conflict in a shared program are flattened.

use super::{
    encoding,
    source::{Cff1, CffFlavor, CharStringSource},
    Error, Result,
};
use crate::FastHashMap;
use std::collections::{BTreeMap, BTreeSet};
use std::marker::PhantomData;
use std::ops::Range;

const TRANSIENT_ARRAY_SIZE: usize = 32;

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub(super) enum Program {
    Glyph(u32),
    Local(u32, u32),
    Global(u32, u32),
}

// CFF INDEX object IDs and offsets fit in u32.
#[derive(Clone, Copy, Debug)]
pub(super) struct Origin {
    pub program: Program,
    start: u32,
    end: u32,
}

impl Origin {
    pub fn span(self) -> Range<usize> {
        self.start as usize..self.end as usize
    }
}

#[derive(Clone, Default)]
pub(super) struct Value {
    pub default: f64,
    pub blend: Option<(usize, Vec<f64>)>,
    // Only an unmodified literal has a span. Computations never grow a producer tree.
    origin: Option<Origin>,
}
impl Value {
    fn number(v: f64, origin: Origin) -> Self {
        Self {
            default: v,
            blend: None,
            origin: Some(origin),
        }
    }
    fn int(&self) -> Result<i32> {
        if self.blend.is_some()
            || self.default.fract() != 0.
            || self.default < i32::MIN as f64
            || self.default > i32::MAX as f64
        {
            return Err(Error);
        }
        Ok(self.default as i32)
    }

    fn seac_code(&self) -> Option<u8> {
        let code = self.default.trunc();
        (self.blend.is_none() && (0.0..=255.0).contains(&code)).then_some(code as u8)
    }

    pub(super) fn origin(&self) -> Option<Origin> {
        self.origin
    }
}

pub(super) struct Command {
    pub op: u16,
    pub args: Vec<Value>,
    pub mask: Vec<u8>,
}

pub(super) trait CharStringActions<'s> {
    // Retaining original bytes and finding closure need blend identity and
    // validation, while consumers that emit values need the actual deltas.
    const MATERIALIZE_BLENDS: bool = true;
    fn program(&mut self, _key: Program, _data: &'s [u8]) {}
    fn call(
        &mut self,
        _key: Program,
        _start: usize,
        _end: usize,
        _target: Program,
        _operand: &Value,
    ) -> Result<()> {
        Ok(())
    }
    fn width(&mut self, _origin: Option<Origin>) {}
    fn mask(&mut self, _key: Program, _start: usize, _end: usize) {}
    fn hint(&mut self, _key: Program, _start: usize, _end: usize, _args: &[Value]) {}
    fn command(&mut self, _op: u16, _args: &[Value], _mask: &[u8]) {}
}
// seac closure only needs the interpreter's result.
impl CharStringActions<'_> for () {
    const MATERIALIZE_BLENDS: bool = false;
}

#[derive(Default)]
pub(super) struct Flattener {
    pub commands: Vec<Command>,
}
impl CharStringActions<'_> for Flattener {
    fn command(&mut self, op: u16, args: &[Value], mask: &[u8]) {
        self.commands.push(Command {
            op,
            args: args.to_vec(),
            mask: mask.to_vec(),
        });
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
struct Edit {
    end: usize,
    call: Option<Program>,
}
struct RecordedProgram<'s> {
    id: usize,
    data: &'s [u8],
    edits: BTreeMap<usize, Edit>,
    masks: BTreeMap<usize, usize>,
}
impl PartialEq for RecordedProgram<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.data == other.data && self.edits == other.edits && self.masks == other.masks
    }
}
impl Eq for RecordedProgram<'_> {}
#[derive(Default)]
pub(super) struct Recorder<'s> {
    programs: FastHashMap<Program, RecordedProgram<'s>>,
    pub needs_flatten: bool,
}
impl<'s> CharStringActions<'s> for Recorder<'s> {
    const MATERIALIZE_BLENDS: bool = false;

    fn program(&mut self, key: Program, data: &'s [u8]) {
        let id = self.programs.len();
        self.programs.entry(key).or_insert_with(|| RecordedProgram {
            id,
            data,
            edits: BTreeMap::new(),
            masks: BTreeMap::new(),
        });
    }
    fn call(
        &mut self,
        key: Program,
        start: usize,
        end: usize,
        target: Program,
        operand: &Value,
    ) -> Result<()> {
        if let Some(origin) = operand.origin {
            let span = origin.span();
            if origin.program == key && span.end == start {
                self.edit(
                    key,
                    span.start,
                    Edit {
                        end,
                        call: Some(target),
                    },
                );
                return Ok(());
            }
        }
        // Retained subroutine calls require a literal in the same program.
        // A caller-supplied target cannot be remapped at this shared offset.
        Err(Error)
    }
    fn width(&mut self, origin: Option<Origin>) {
        // Like HarfBuzz, remove the width literal from whichever program
        // pushed it, often a subroutine shared with the glyph's hint operands.
        // merge() flattens glyphs whose deletion conflicts with other callers.
        if let Some(origin) = origin {
            let span = origin.span();
            self.edit(
                origin.program,
                span.start,
                Edit {
                    end: span.end,
                    call: None,
                },
            );
        } else {
            self.needs_flatten = true;
        }
    }
    fn mask(&mut self, key: Program, start: usize, end: usize) {
        let old = self
            .programs
            .get_mut(&key)
            .unwrap()
            .masks
            .entry(start)
            .or_insert(end);
        self.needs_flatten |= *old != end;
    }
    fn hint(&mut self, key: Program, start: usize, end: usize, args: &[Value]) {
        // Hint operators consume the whole stack. HarfBuzz drops each operand
        // together with the operator, even when a caller or another subroutine
        // pushed it, so delete every literal from the program that produced it.
        // Operands that immediately precede the operator join its deletion.
        let mut first = start;
        let mut detached = Vec::new();
        for value in args.iter().rev() {
            let Some(origin) = value.origin else {
                self.needs_flatten = true;
                return;
            };
            let span = origin.span();
            if origin.program == key && span.end == first {
                first = span.start;
            } else {
                detached.push(origin);
            }
        }
        self.edit(key, first, Edit { end, call: None });
        for origin in detached {
            let span = origin.span();
            self.edit(
                origin.program,
                span.start,
                Edit {
                    end: span.end,
                    call: None,
                },
            );
        }
    }
}
impl<'s> Recorder<'s> {
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            programs: FastHashMap::with_capacity_and_hasher(
                capacity,
                foldhash::fast::FixedState::default(),
            ),
            needs_flatten: false,
        }
    }

    pub fn merge(&mut self, other: Self) -> bool {
        // Validate the entire glyph before modifying any shared program.
        // Flattening must discard every staged edit, including later edits
        // made after the interpreter first discovered an incompatible mask.
        if other.needs_flatten
            || other
                .programs
                .iter()
                .any(|(key, program)| self.programs.get(key).is_some_and(|old| old != program))
        {
            return false;
        }
        for (key, mut program) in other.programs {
            let id = self.programs.len();
            self.programs.entry(key).or_insert_with(|| {
                program.id = id;
                program
            });
        }
        true
    }

    fn edit(&mut self, key: Program, start: usize, edit: Edit) {
        let edits = &mut self.programs.get_mut(&key).unwrap().edits;
        // A mask's byte length can depend on its caller's stem count, too.
        if let Some(previous) = edits.get(&start) {
            if *previous != edit {
                self.needs_flatten = true;
            }
        } else {
            edits.insert(start, edit);
        }
    }
    fn only_hints(&self, key: Program, visiting: &mut BTreeSet<Program>) -> bool {
        if !visiting.insert(key) {
            return false;
        }
        let result = self.programs.get(&key).is_some_and(|p| {
            let mut pos = 0;
            for (&start, edit) in &p.edits {
                if start < pos
                    || p.data[pos..start].iter().any(|&b| b != 11)
                    || edit.call.is_some_and(|t| !self.only_hints(t, visiting))
                {
                    return false;
                }
                pos = edit.end;
            }
            p.data[pos..].iter().all(|&b| b == 11)
        });
        visiting.remove(&key);
        result
    }
    pub fn remove_hint_calls(&mut self) {
        let dead: BTreeSet<_> = self
            .programs
            .keys()
            .copied()
            .filter(|&p| {
                !matches!(p, Program::Glyph(_)) && self.only_hints(p, &mut BTreeSet::new())
            })
            .collect();
        for p in self.programs.values_mut() {
            for edit in p.edits.values_mut() {
                if edit.call.is_some_and(|p| dead.contains(&p)) {
                    edit.call = None;
                }
            }
        }
    }
    pub fn closure(&self, roots: &[Program]) -> Result<Vec<Program>> {
        let mut used = vec![false; self.programs.len()];
        let mut result = Vec::new();
        let mut pending = roots.to_vec();
        while let Some(key) = pending.pop() {
            let program = self.programs.get(&key).ok_or(Error)?;
            if std::mem::replace(&mut used[program.id], true) {
                continue;
            }
            result.push(key);
            for edit in program.edits.values() {
                if let Some(target) = edit.call {
                    pending.push(target);
                }
            }
        }
        Ok(result)
    }
    pub fn fd_dependent(&self, key: Program) -> bool {
        match key {
            Program::Local(..) => true,
            _ => self.programs[&key]
                .edits
                .values()
                .filter_map(|e| e.call)
                .any(|t| self.fd_dependent(t)),
        }
    }
    pub fn share_global(&self, a: Program, b: Program) -> bool {
        if self.fd_dependent(a) || self.fd_dependent(b) {
            return false;
        }
        let a = &self.programs[&a];
        let b = &self.programs[&b];
        a.data == b.data
            && a.edits.len() == b.edits.len()
            && a.edits.iter().zip(&b.edits).all(|((s, a), (t, b))| {
                s == t
                    && a.end == b.end
                    && match (a.call, b.call) {
                        (None, None) => true,
                        (Some(a @ Program::Global(_, i)), Some(b @ Program::Global(_, j))) => {
                            i == j && self.share_global(a, b)
                        }
                        _ => false,
                    }
            })
    }
    pub fn encode(
        &self,
        key: Program,
        remap: &FastHashMap<Program, usize>,
        global_count: usize,
        local_counts: &[usize],
    ) -> Result<Vec<u8>> {
        let program = self.programs.get(&key).ok_or(Error)?;
        let mut out = Vec::with_capacity(program.data.len());
        let mut pos = 0;
        for (&start, edit) in &program.edits {
            out.extend(program.data.get(pos..start).ok_or(Error)?);
            if let Some(target) = edit.call {
                let (op, count) = match target {
                    Program::Local(fd, _) => (10, *local_counts.get(fd as usize).ok_or(Error)?),
                    Program::Global(_, _) => (29, global_count),
                    _ => return Err(Error),
                };
                encoding::integer(
                    &mut out,
                    i32::try_from(*remap.get(&target).ok_or(Error)?).map_err(|_| Error)?
                        - encoding::bias(count),
                );
                out.push(op);
            }
            pos = edit.end;
        }
        out.extend(program.data.get(pos..).ok_or(Error)?);
        Ok(out)
    }
}

pub(super) fn operand_stack<F: CffFlavor>() -> Vec<Value> {
    // Use the CFF1 stack limit as the starting capacity, also for CFF2.
    Vec::with_capacity(F::STACK_LIMIT.min(Cff1::STACK_LIMIT))
}

pub(super) struct Interpreter<'a, 's, F, S, A> {
    source: &'s S,
    actions: &'a mut A,
    fd: usize,
    stack: &'a mut Vec<Value>,
    transient: Vec<Value>,
    hints: usize,
    initial_ivs: usize,
    ivs: usize,
    seen_ivs: bool,
    seen_blend: bool,
    width_read: bool,
    pub width: Option<f64>,
    pub seac: Option<(u8, u8)>,
    remaining: usize,
    drop_hints: bool,
    flavor: PhantomData<F>,
}
impl<'a, 's, F: CffFlavor, S: CharStringSource, A: CharStringActions<'s>>
    Interpreter<'a, 's, F, S, A>
{
    pub fn new(
        source: &'s S,
        actions: &'a mut A,
        fd: usize,
        drop_hints: bool,
        stack: &'a mut Vec<Value>,
    ) -> Self {
        debug_assert!(u32::try_from(fd).is_ok());
        stack.clear();
        let initial_ivs = if F::CFF2 { source.initial_ivs(fd) } else { 0 };
        Self {
            source,
            actions,
            fd,
            stack,
            // Type 2's transient array is rarely used. Allocate its large
            // symbolic values only when a program executes `put`.
            transient: Vec::new(),
            hints: 0,
            initial_ivs,
            ivs: initial_ivs,
            seen_ivs: false,
            seen_blend: false,
            width_read: F::CFF2,
            width: None,
            seac: None,
            remaining: 200_000,
            drop_hints,
            flavor: PhantomData,
        }
    }
    /// The inherited variation index, unchanged by any `vsindex` during execution.
    pub fn initial_variation_index(&self) -> usize {
        self.initial_ivs
    }
    fn pop(&mut self) -> Result<Value> {
        self.stack.pop().ok_or(Error)
    }
    fn push(&mut self, v: Value) -> Result<()> {
        if self.stack.len() >= F::STACK_LIMIT || !v.default.is_finite() {
            return Err(Error);
        }
        self.stack.push(v);
        Ok(())
    }
    fn width(&mut self, op: u16) -> Result<()> {
        if self.width_read {
            return Ok(());
        }
        let have = match op {
            1 | 3 | 18 | 19 | 20 | 23 | 14 => self.stack.len() % 2 == 1,
            4 | 22 => self.stack.len() > 1,
            21 => self.stack.len() > 2,
            _ => return Ok(()),
        };
        self.width_read = true;
        if have {
            let value = self.stack.remove(0);
            self.width = Some(value.default);
            // Flattening emits the width separately, while recording keeps its original location.
            if self.drop_hints {
                self.actions.width(value.origin);
            }
        }
        Ok(())
    }
    pub fn run(&mut self, key: Program, data: &'s [u8]) -> Result<()> {
        self.execute(key, data, 0).map(|_| ())
    }
    fn execute(&mut self, key: Program, data: &'s [u8], depth: usize) -> Result<bool> {
        if depth > 10 {
            return Err(Error);
        }
        debug_assert!(u32::try_from(data.len()).is_ok());
        let mut p = 0;
        self.actions.program(key, data);
        while p < data.len() {
            if self.remaining == 0 {
                return Err(Error);
            }
            self.remaining -= 1;
            let start = p;
            let byte = *data.get(p).ok_or(Error)?;
            if byte >= 28 {
                if let Some(value) = encoding::number(data, &mut p, false)? {
                    self.push(Value::number(
                        value,
                        Origin {
                            program: key,
                            start: start as u32,
                            end: p as u32,
                        },
                    ))?;
                    continue;
                }
            }
            let op = encoding::op(data, &mut p)?;
            let mut mask = &[][..];
            self.width(op)?;
            if matches!(op, 1 | 3 | 18 | 23 | 19 | 20) {
                if self.stack.len() % 2 != 0 {
                    return Err(Error);
                }
                self.hints = self.hints.checked_add(self.stack.len() / 2).ok_or(Error)?;
                if self.hints > 96 {
                    return Err(Error);
                }
                if matches!(op, 19 | 20) {
                    let end = p.checked_add(self.hints.div_ceil(8)).ok_or(Error)?;
                    mask = data.get(p..end).ok_or(Error)?;
                    p = end;
                    self.actions.mask(key, start, p);
                }
            }
            match op {
                10 | 29 => {
                    let value = self.pop()?;
                    let global = op == 29;
                    let index = value
                        .int()?
                        .checked_add(self.source.subr_bias(global, self.fd))
                        .ok_or(Error)?;
                    let index = usize::try_from(index).map_err(|_| Error)?;
                    let target = if global {
                        Program::Global(self.fd as u32, index as u32)
                    } else {
                        Program::Local(self.fd as u32, index as u32)
                    };
                    self.actions.call(key, start, p, target, &value)?;
                    if self.execute(
                        target,
                        self.source.program(global, self.fd, index)?,
                        depth + 1,
                    )? {
                        return Ok(true);
                    }
                }
                11 => {
                    if depth == 0 && !F::CFF2 {
                        return Err(Error);
                    }
                    return Ok(false);
                }
                15 if F::CFF2 => {
                    if self.seen_ivs || self.seen_blend {
                        return Err(Error);
                    }
                    self.ivs = usize::try_from(self.pop()?.int()?).map_err(|_| Error)?;
                    self.source.region_count(self.ivs)?;
                    self.seen_ivs = true;
                }
                16 if F::CFF2 => {
                    self.seen_blend = true;
                    let n = usize::try_from(self.pop()?.int()?).map_err(|_| Error)?;
                    let k = self.source.region_count(self.ivs)?;
                    let count = n.checked_mul(k.checked_add(1).ok_or(Error)?).ok_or(Error)?;
                    let group_start = self.stack.len().checked_sub(count).ok_or(Error)?;
                    for i in 0..n {
                        if self.stack[group_start + i].blend.is_some() {
                            return Err(Error);
                        }
                        let deltas =
                            &self.stack[group_start + n + i * k..group_start + n + (i + 1) * k];
                        if deltas.iter().any(|v| v.blend.is_some()) {
                            return Err(Error);
                        }
                        let deltas = if A::MATERIALIZE_BLENDS {
                            deltas.iter().map(|v| v.default).collect()
                        } else {
                            Vec::new()
                        };
                        let mut value = std::mem::take(&mut self.stack[group_start + i]);
                        value.blend = Some((self.ivs, deltas));
                        value.origin = None;
                        self.stack[group_start + i] = value;
                    }
                    self.stack.truncate(group_start + n);
                }
                0x100 => {
                    // dotsection is a deprecated no-op. Its live operands
                    // belong to the next command, even when hints are removed.
                    if self.drop_hints {
                        self.actions.hint(key, start, p, &[]);
                    }
                }
                0x103..=0x105
                | 0x109..=0x10c
                | 0x10e..=0x10f
                | 0x112
                | 0x114..=0x118
                | 0x11a..=0x11e => self.arithmetic(op)?,
                1
                | 3
                | 4
                | 5
                | 6
                | 7
                | 8
                | 14
                | 18
                | 19
                | 20
                | 21
                | 22
                | 23
                | 24
                | 25
                | 26
                | 27
                | 30
                | 31
                | 0x122..=0x125 => {
                    if op == 14 && !F::CFF2 && self.stack.len() >= 4 {
                        let n = self.stack.len();
                        // HarfBuzz truncates fractional character codes toward zero.
                        self.seac = self.stack[n - 2]
                            .seac_code()
                            .zip(self.stack[n - 1].seac_code());
                    }
                    let hint = matches!(op, 1 | 3 | 18 | 19 | 20 | 23);
                    if hint && self.drop_hints {
                        self.actions.hint(key, start, p, self.stack);
                    } else if !(F::CFF2 && op == 14) {
                        self.actions.command(op, self.stack, mask);
                    }
                    self.stack.clear();
                    if op == 14 {
                        return Ok(true);
                    }
                }
                // Reserved operators are ignored and clear the stack, as by other consumers.
                _ => self.stack.clear(),
            }
        }
        if !F::CFF2 {
            return Err(Error);
        }
        if depth == 0 && !self.stack.is_empty() {
            return Err(Error);
        }
        Ok(false)
    }
    // Evaluate Type 2 arithmetic and transient operators. HarfBuzz clears
    // the stack for these operators.
    fn arithmetic(&mut self, op: u16) -> Result<()> {
        // Stack/transient operations may share values. Their producer is no
        // longer a removable literal, even if the numeric result is unchanged.
        for value in self.stack.iter_mut().chain(&mut self.transient) {
            value.origin = None;
        }
        let mut a = self.pop()?;
        if a.blend.is_some() {
            return Err(Error);
        }
        match op {
            0x109 => a.default = a.default.abs(),
            0x10e => a.default = -a.default,
            0x105 => a.default = f64::from(a.default == 0.),
            0x11a => {
                if a.default < 0. {
                    return Err(Error);
                }
                a.default = a.default.sqrt();
            }
            0x112 => return Ok(()),
            0x11b => {
                self.push(a.clone())?;
            }
            0x114 => {
                let idx = usize::try_from(a.int()?).map_err(|_| Error)?;
                if idx >= TRANSIENT_ARRAY_SIZE {
                    return Err(Error);
                }
                let v = self.pop()?;
                if self.transient.is_empty() {
                    self.transient
                        .resize_with(TRANSIENT_ARRAY_SIZE, Value::default);
                }
                self.transient[idx] = v;
                return Ok(());
            }
            0x115 => {
                let idx = usize::try_from(a.int()?).map_err(|_| Error)?;
                if idx >= TRANSIENT_ARRAY_SIZE {
                    return Err(Error);
                }
                a = self.transient.get(idx).cloned().unwrap_or_default();
            }
            0x11d => {
                let idx = a.int()?.max(0) as usize;
                a = self
                    .stack
                    .get(
                        self.stack
                            .len()
                            .checked_sub(idx.min(self.stack.len().saturating_sub(1)) + 1)
                            .ok_or(Error)?,
                    )
                    .ok_or(Error)?
                    .clone();
            }
            0x11c => {
                let b = self.pop()?;
                self.push(a)?;
                a = b;
            }
            0x11e => {
                let shift = a.int()?;
                let n = usize::try_from(self.pop()?.int()?).map_err(|_| Error)?;
                let start = self.stack.len().checked_sub(n).ok_or(Error)?;
                if n > 0 {
                    self.stack[start..].rotate_right(shift.rem_euclid(n as i32) as usize);
                }
                return Ok(());
            }
            0x116 => {
                let b = self.pop()?;
                let v2 = self.pop()?;
                let v1 = self.pop()?;
                a = if b.default <= a.default { v1 } else { v2 };
            }
            0x117 => {
                return Err(Error);
            } // Random-dependent programs cannot be deterministically flattened.
            _ => {
                let mut b = self.pop()?;
                if b.blend.is_some() {
                    return Err(Error);
                }
                b.default = match op {
                    0x103 => f64::from(b.default != 0. && a.default != 0.),
                    0x104 => f64::from(b.default != 0. || a.default != 0.),
                    0x10a => b.default + a.default,
                    0x10b => b.default - a.default,
                    0x10c => {
                        if a.default == 0. {
                            return Err(Error);
                        }
                        b.default / a.default
                    }
                    0x10f => f64::from(b.default == a.default),
                    0x118 => b.default * a.default,
                    _ => return Err(Error),
                };
                a = b;
            }
        }
        self.push(a)
    }
}

pub(super) fn flatten(
    commands: &[Command],
    width: Option<f64>,
    cff2: bool,
    inherited_ivs: usize,
) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    if let Some(width) = width {
        encoding::encode(&mut out, width, false)?;
    }
    let active = commands
        .iter()
        .flat_map(|c| &c.args)
        .find_map(|v| v.blend.as_ref().map(|b| b.0));
    if let Some(ivs) = active.filter(|&ivs| ivs != inherited_ivs) {
        encoding::integer(&mut out, i32::try_from(ivs).map_err(|_| Error)?);
        out.push(15);
    }
    for cmd in commands {
        let mut i = 0;
        while i < cmd.args.len() {
            let v = &cmd.args[i];
            if let Some((vs, deltas)) = &v.blend {
                if Some(*vs) != active {
                    return Err(Error);
                }
                let room = 513usize.checked_sub(i + 1).ok_or(Error)? / (deltas.len() + 1);
                let n = cmd.args[i..]
                    .iter()
                    .take(room)
                    .take_while(|v| {
                        v.blend
                            .as_ref()
                            .is_some_and(|(vsi, ds)| vsi == vs && ds.len() == deltas.len())
                    })
                    .count();
                if n == 0 {
                    return Err(Error);
                }
                for value in &cmd.args[i..i + n] {
                    encoding::encode(&mut out, value.default, false)?;
                }
                for value in &cmd.args[i..i + n] {
                    for &delta in &value.blend.as_ref().ok_or(Error)?.1 {
                        encoding::encode(&mut out, delta, false)?;
                    }
                }
                encoding::integer(&mut out, n as i32);
                out.push(16);
                i += n;
            } else {
                encoding::encode(&mut out, v.default, false)?;
                i += 1;
            }
        }
        encoding::emit_op(&mut out, cmd.op);
        out.extend(&cmd.mask);
    }
    if !cff2 && out.last() != Some(&14) {
        out.push(14);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cff::source::Cff2;
    struct Source(Vec<Vec<u8>>);
    impl CharStringSource for Source {
        fn initial_ivs(&self, fd: usize) -> usize {
            // Give each mock Font DICT a distinct inherited variation index.
            fd
        }
        fn program(&self, _: bool, _: usize, i: usize) -> Result<&[u8]> {
            self.0.get(i).map(Vec::as_slice).ok_or(Error)
        }
        fn subr_count(&self, _: bool, _: usize) -> usize {
            self.0.len()
        }
        fn region_count(&self, _: usize) -> Result<usize> {
            Ok(1)
        }
    }

    struct Interpretation {
        width: Option<f64>,
        seac: Option<(u8, u8)>,
    }

    fn run_cff1<'s>(
        source: &'s Source,
        actions: &mut impl CharStringActions<'s>,
        data: &'s [u8],
        drop_hints: bool,
    ) -> Result<Interpretation> {
        let mut stack = operand_stack::<Cff1>();
        let mut interp = Interpreter::<Cff1, _, _>::new(source, actions, 0, drop_hints, &mut stack);
        interp.run(Program::Glyph(0), data)?;
        Ok(Interpretation {
            width: interp.width,
            seac: interp.seac,
        })
    }

    #[test]
    fn repeated_dup_add_has_bounded_state() {
        let source = Source(vec![]);
        let mut data = vec![139];
        for _ in 0..40 {
            data.extend([12, 27, 12, 10]);
        }
        data.extend([149, 1, 139, 139, 21, 14]);
        let mut rec = Recorder::default();
        run_cff1(&source, &mut rec, &data, true).unwrap();
        assert!(rec.needs_flatten);
        let mut flat = Flattener::default();
        run_cff1(&source, &mut flat, &data, true).unwrap();
        assert_eq!(
            flatten(&flat.commands, None, false, 0).unwrap(),
            [139, 139, 21, 14]
        );
    }
    #[test]
    fn hint_operands_from_subroutines_are_deleted_there() {
        // The local subroutine pushes the hstem operands for glyph 0, but the
        // rmoveto operands for glyph 1.
        let source = Source(vec![vec![139, 149, 11]]);
        fn record<'s>(source: &'s Source, glyph: &'s [u8], gid: u32) -> Recorder<'s> {
            let mut rec = Recorder::default();
            Interpreter::<Cff1, _, _>::new(source, &mut rec, 0, true, &mut operand_stack::<Cff1>())
                .run(Program::Glyph(gid), glyph)
                .unwrap();
            rec
        }
        let mut merged = Recorder::default();
        let hinted = record(&source, &[32, 10, 1, 139, 139, 21, 14], 0);
        assert!(!hinted.needs_flatten);
        assert!(merged.merge(hinted));
        merged.remove_hint_calls();
        // The operands are deleted from the subroutine, which is then only a
        // return, so the call is dropped as well.
        assert_eq!(
            merged.closure(&[Program::Glyph(0)]).unwrap(),
            [Program::Glyph(0)]
        );
        assert_eq!(
            merged
                .encode(Program::Glyph(0), &FastHashMap::default(), 0, &[1])
                .unwrap(),
            [139, 139, 21, 14]
        );
        // Glyph 1 needs the subroutine's literals, which conflicts with the
        // deletions already merged, so it must be flattened instead.
        assert!(!merged.merge(record(&source, &[32, 10, 21, 14], 1)));
    }
    #[test]
    fn hints_with_local_literals_remain_subroutinized() {
        let source = Source(vec![vec![139, 149, 1, 11]]);
        let mut rec = Recorder::default();
        run_cff1(&source, &mut rec, &[32, 10, 139, 139, 21, 14], true).unwrap();
        assert!(!rec.needs_flatten);
        rec.remove_hint_calls();
        assert_eq!(
            rec.closure(&[Program::Glyph(0)]).unwrap(),
            [Program::Glyph(0)]
        );
        assert_eq!(
            rec.encode(Program::Glyph(0), &FastHashMap::default(), 0, &[0])
                .unwrap(),
            [139, 139, 21, 14]
        );
    }
    #[test]
    fn caller_supplied_subroutine_operand_is_rejected() {
        let source = Source(vec![vec![10, 11]]);
        let mut rec = Recorder::default();
        assert!(run_cff1(&source, &mut rec, &[32, 32, 10, 14], false).is_err());
    }
    #[test]
    fn flatten_keeps_blend_groups() {
        let source = Source(vec![]);
        let mut flat = Flattener::default();
        let data = [139, 149, 140, 141, 141, 16, 21];
        Interpreter::<Cff2, _, _>::new(&source, &mut flat, 0, false, &mut operand_stack::<Cff2>())
            .run(Program::Glyph(0), &data)
            .unwrap();
        assert_eq!(flatten(&flat.commands, None, true, 0).unwrap(), data);
    }
    #[test]
    fn cff2_inherits_the_font_dict_variation_index() {
        let source = Source(vec![]);
        for prefix in [&[][..], &[139, 15]] {
            let mut flat = Flattener::default();
            let mut data = prefix.to_vec();
            data.extend([139, 149, 140, 141, 141, 16, 21]);
            let mut stack = operand_stack::<Cff2>();
            let mut interp =
                Interpreter::<Cff2, _, _>::new(&source, &mut flat, 2, false, &mut stack);
            interp.run(Program::Glyph(0), &data).unwrap();
            let initial_ivs = interp.initial_variation_index();
            assert_eq!(initial_ivs, 2);
            // Preserve both inherited blends and an explicit switch to index 0.
            // Flattening needs the initial index even when vsindex changes it.
            assert_eq!(
                flatten(&flat.commands, None, true, initial_ivs).unwrap(),
                data
            );
        }
    }

    #[test]
    fn recursion_and_truncated_operands_are_errors() {
        let source = Source(vec![vec![32, 10, 11]]);
        for bytes in [
            vec![32, 10, 14],
            vec![28],
            vec![255, 0],
            vec![139, 19],
            vec![32, 10],
        ] {
            assert!(run_cff1(&source, &mut (), &bytes, false).is_err());
        }
    }

    #[test]
    fn seac_with_an_explicit_width() {
        let source = Source(vec![]);
        // 500 width, 10 adx, 20 ady, 65 bchar, 66 achar, endchar.
        let data = [248, 136, 149, 159, 204, 205, 14];
        let result = run_cff1(&source, &mut (), &data, false).unwrap();
        assert_eq!(result.width, Some(500.0));
        assert_eq!(result.seac, Some((65, 66)));
    }

    #[test]
    fn seac_uses_the_last_two_of_at_least_four_operands() {
        let source = Source(vec![]);
        // Six operands avoids the optional-width case and verifies that extra
        // live operands do not hide the final bchar/achar pair.
        let data = [139, 139, 139, 139, 204, 205, 14];
        let seac = run_cff1(&source, &mut (), &data, false).unwrap().seac;
        assert_eq!(seac, Some((65, 66)));
    }

    #[test]
    fn seac_truncates_fractional_character_codes() {
        let source = Source(vec![]);
        // 0 adx, 0 ady, 65.5 bchar, 66.75 achar, endchar.
        let data = [139, 139, 255, 0, 65, 128, 0, 255, 0, 66, 192, 0, 14];
        let seac = run_cff1(&source, &mut (), &data, false).unwrap().seac;
        assert_eq!(seac, Some((65, 66)));
    }

    #[test]
    fn reused_stack_is_cleared_after_an_interpreter_error() {
        let source = Source(vec![]);
        let mut stack = operand_stack::<Cff1>();
        let buffer = stack.as_ptr();
        let mut actions = ();
        // Leave one live operand when callsubr fails, so reuse must clear it.
        assert!(
            Interpreter::<Cff1, _, _>::new(&source, &mut actions, 0, false, &mut stack)
                .run(Program::Glyph(0), &[140, 139, 10])
                .is_err()
        );
        let mut interp =
            Interpreter::<Cff1, _, _>::new(&source, &mut actions, 0, false, &mut stack);
        interp
            .run(Program::Glyph(1), &[139, 139, 204, 205, 14])
            .unwrap();
        assert_eq!(interp.width, None);
        assert_eq!(interp.seac, Some((65, 66)));
        assert_eq!(stack.as_ptr(), buffer);
    }

    #[test]
    fn invalid_seac_codes_do_not_fail_interpretation() {
        let source = Source(vec![]);
        let data = [139, 139, 139, 139, 28, 3, 232, 14];
        let seac = run_cff1(&source, &mut (), &data, false).unwrap().seac;
        assert_eq!(seac, None);
    }

    #[test]
    fn transient_array_is_allocated_only_by_put() {
        let source = Source(vec![]);
        let mut actions = ();
        let mut stack = operand_stack::<Cff1>();
        let mut interp =
            Interpreter::<Cff1, _, _>::new(&source, &mut actions, 0, false, &mut stack);
        assert!(interp.transient.is_empty());

        interp.push(Value::default()).unwrap();
        interp.arithmetic(0x115).unwrap();
        assert!(interp.transient.is_empty());
        assert_eq!(interp.pop().unwrap().default, 0.0);

        interp.push(Value::default()).unwrap();
        interp.push(Value::default()).unwrap();
        interp.arithmetic(0x114).unwrap();
        assert_eq!(interp.transient.len(), 32);
    }
}
