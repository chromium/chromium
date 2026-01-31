#[cfg(feature = "internal_debug")]
use std::fmt;

use crate::compiler::tokens::Span;
use crate::output::CaptureMode;
use crate::value::Value;

/// This loop has the loop var.
pub const LOOP_FLAG_WITH_LOOP_VAR: u8 = 1;

/// This loop is recursive.
pub const LOOP_FLAG_RECURSIVE: u8 = 2;

/// This macro uses the caller var.
#[cfg(feature = "macros")]
pub const MACRO_CALLER: u8 = 2;

/// Rust type to represent locals.
pub type LocalId = u8;

/// The maximum number of filters/tests that can be cached.
pub const MAX_LOCALS: usize = 50;

/// Represents an instruction for the VM.
#[cfg_attr(feature = "internal_debug", derive(Debug))]
#[cfg_attr(
    feature = "unstable_machinery_serde",
    derive(serde::Serialize),
    serde(tag = "op", content = "arg")
)]
#[derive(Clone)]
pub enum Instruction<'source> {
    /// Emits raw source
    EmitRaw(&'source str),

    /// Stores a variable (only possible in for loops)
    StoreLocal(&'source str),

    /// Load a variable,
    Lookup(&'source str),

    /// Looks up an attribute.
    GetAttr(&'source str),

    /// Sets an attribute.
    SetAttr(&'source str),

    /// Looks up an item.
    GetItem,

    /// Performs a slice operation.
    Slice,

    /// Loads a constant value.
    LoadConst(Value),

    /// Builds a map of the last n pairs on the stack.
    BuildMap(usize),

    /// Builds a kwargs map of the last n pairs on the stack.
    BuildKwargs(usize),

    /// Merges N kwargs maps on the list into one.
    MergeKwargs(usize),

    /// Builds a list of the last n pairs on the stack.
    BuildList(Option<usize>),

    /// Unpacks a list into N stack items.
    UnpackList(usize),

    /// Unpacks N lists onto the stack and pushes the number of items there were unpacked.
    UnpackLists(usize),

    /// Add the top two values
    Add,

    /// Subtract the top two values
    Sub,

    /// Multiply the top two values
    Mul,

    /// Divide the top two values
    Div,

    /// Integer divide the top two values as "integer".
    ///
    /// Note that in MiniJinja this currently uses an euclidean
    /// division to match the rem implementation.  In Python this
    /// instead uses a flooring division and a flooring remainder.
    IntDiv,

    /// Calculate the remainder the top two values
    Rem,

    /// x to the power of y.
    Pow,

    /// Negates the value.
    Neg,

    /// `=` operator
    Eq,

    /// `!=` operator
    Ne,

    /// `>` operator
    Gt,

    /// `>=` operator
    Gte,

    /// `<` operator
    Lt,

    /// `<=` operator
    Lte,

    /// Unary not
    Not,

    /// String concatenation operator
    StringConcat,

    /// Performs a containment check
    In,

    /// Apply a filter.
    ApplyFilter(&'source str, Option<u16>, LocalId),

    /// Perform a filter.
    PerformTest(&'source str, Option<u16>, LocalId),

    /// Emit the stack top as output
    Emit,

    /// Starts a loop
    ///
    /// The argument are loop flags.
    PushLoop(u8),

    /// Starts a with block.
    PushWith,

    /// Does a single loop iteration
    ///
    /// The argument is the jump target for when the loop
    /// ends and must point to a `PopFrame` instruction.
    Iterate(u32),

    /// Push a bool that indicates that the loop iterated.
    PushDidNotIterate,

    /// Pops the topmost frame
    PopFrame,

    /// Pops the topmost frame and runs loop logic
    PopLoopFrame,

    /// Jump to a specific instruction
    Jump(u32),

    /// Jump if the stack top evaluates to false
    JumpIfFalse(u32),

    /// Jump if the stack top evaluates to false or pops the value
    JumpIfFalseOrPop(u32),

    /// Jump if the stack top evaluates to true or pops the value
    JumpIfTrueOrPop(u32),

    /// Sets the auto escape flag to the current value.
    PushAutoEscape,

    /// Resets the auto escape flag to the previous value.
    PopAutoEscape,

    /// Begins capturing of output (false) or discard (true).
    BeginCapture(CaptureMode),

    /// Ends capturing of output.
    EndCapture,

    /// Calls a global function
    CallFunction(&'source str, Option<u16>),

    /// Calls a method
    CallMethod(&'source str, Option<u16>),

    /// Calls an object
    CallObject(Option<u16>),

    /// Duplicates the top item
    DupTop,

    /// Discards the top item
    DiscardTop,

    /// A fast super instruction without intermediate capturing.
    FastSuper,

    /// A fast loop recurse instruction without intermediate capturing.
    FastRecurse,

    /// Swaps the top two items in the stack.
    Swap,

    /// Call into a block.
    #[cfg(feature = "multi_template")]
    CallBlock(&'source str),

    /// Loads block from a template with name on stack ("extends")
    #[cfg(feature = "multi_template")]
    LoadBlocks,

    /// Includes another template.
    #[cfg(feature = "multi_template")]
    Include(bool),

    /// Builds a module
    #[cfg(feature = "multi_template")]
    ExportLocals,

    /// Builds a macro on the stack.
    #[cfg(feature = "macros")]
    BuildMacro(&'source str, u32, u8),

    /// Breaks from the interpreter loop (exists a function)
    #[cfg(feature = "macros")]
    Return,

    /// True if the value is undefined
    #[cfg(feature = "macros")]
    IsUndefined,

    /// Encloses a variable.
    #[cfg(feature = "macros")]
    Enclose(&'source str),

    /// Returns the closure of this context level.
    #[cfg(feature = "macros")]
    GetClosure,
}

#[derive(Copy, Clone)]
struct LineInfo {
    first_instruction: u32,
    line: u16,
}

#[cfg(feature = "debug")]
#[derive(Copy, Clone)]
struct SpanInfo {
    first_instruction: u32,
    span: Span,
}

/// Wrapper around instructions to help with location management.
pub struct Instructions<'source> {
    pub(crate) instructions: Vec<Instruction<'source>>,
    line_infos: Vec<LineInfo>,
    #[cfg(feature = "debug")]
    span_infos: Vec<SpanInfo>,
    name: &'source str,
    source: &'source str,
}

pub(crate) static EMPTY_INSTRUCTIONS: Instructions<'static> = Instructions {
    instructions: Vec::new(),
    line_infos: Vec::new(),
    #[cfg(feature = "debug")]
    span_infos: Vec::new(),
    name: "<unknown>",
    source: "",
};

impl<'source> Instructions<'source> {
    /// Creates a new instructions object.
    pub fn new(name: &'source str, source: &'source str) -> Instructions<'source> {
        Instructions {
            instructions: Vec::with_capacity(128),
            line_infos: Vec::with_capacity(128),
            #[cfg(feature = "debug")]
            span_infos: Vec::with_capacity(128),
            name,
            source,
        }
    }

    /// Returns the name of the template.
    pub fn name(&self) -> &'source str {
        self.name
    }

    /// Returns the source reference.
    pub fn source(&self) -> &'source str {
        self.source
    }

    /// Returns an instruction by index
    #[inline(always)]
    pub fn get(&self, idx: u32) -> Option<&Instruction<'source>> {
        self.instructions.get(idx as usize)
    }

    /// Returns an instruction by index mutably
    pub fn get_mut(&mut self, idx: u32) -> Option<&mut Instruction<'source>> {
        self.instructions.get_mut(idx as usize)
    }

    /// Adds a new instruction
    pub fn add(&mut self, instr: Instruction<'source>) -> u32 {
        let rv = self.instructions.len();
        self.instructions.push(instr);
        rv as u32
    }

    fn add_line_record(&mut self, instr: u32, line: u16) {
        let same_loc = self
            .line_infos
            .last()
            .is_some_and(|last_loc| last_loc.line == line);
        if !same_loc {
            self.line_infos.push(LineInfo {
                first_instruction: instr,
                line,
            });
        }
    }

    /// Adds a new instruction with line number.
    pub fn add_with_line(&mut self, instr: Instruction<'source>, line: u16) -> u32 {
        let rv = self.add(instr);
        self.add_line_record(rv, line);

        // if we follow up to a valid span with no more span, clear it out
        #[cfg(feature = "debug")]
        {
            if self
                .span_infos
                .last()
                .is_some_and(|x| x.span != Span::default())
            {
                self.span_infos.push(SpanInfo {
                    first_instruction: rv,
                    span: Span::default(),
                });
            }
        }
        rv
    }

    /// Adds a new instruction with span.
    pub fn add_with_span(&mut self, instr: Instruction<'source>, span: Span) -> u32 {
        let rv = self.add(instr);
        #[cfg(feature = "debug")]
        {
            let same_loc = self
                .span_infos
                .last()
                .is_some_and(|last_loc| last_loc.span == span);
            if !same_loc {
                self.span_infos.push(SpanInfo {
                    first_instruction: rv,
                    span,
                });
            }
        }
        self.add_line_record(rv, span.start_line);
        rv
    }

    /// Looks up the line for an instruction
    pub fn get_line(&self, idx: u32) -> Option<usize> {
        let loc = match self
            .line_infos
            .binary_search_by_key(&idx, |x| x.first_instruction)
        {
            Ok(idx) => &self.line_infos[idx],
            Err(0) => return None,
            Err(idx) => &self.line_infos[idx - 1],
        };
        Some(loc.line as usize)
    }

    /// Looks up a span for an instruction.
    pub fn get_span(&self, idx: u32) -> Option<Span> {
        #[cfg(feature = "debug")]
        {
            let loc = match self
                .span_infos
                .binary_search_by_key(&idx, |x| x.first_instruction)
            {
                Ok(idx) => &self.span_infos[idx],
                Err(0) => return None,
                Err(idx) => &self.span_infos[idx - 1],
            };
            (loc.span != Span::default()).then_some(loc.span)
        }
        #[cfg(not(feature = "debug"))]
        {
            let _ = idx;
            None
        }
    }

    /// Returns a list of all names referenced in the current block backwards
    /// from the given pc.
    #[cfg(feature = "debug")]
    pub fn get_referenced_names(&self, idx: u32) -> Vec<&'source str> {
        let mut rv = Vec::new();
        // make sure we don't crash on empty instructions
        if self.instructions.is_empty() {
            return rv;
        }
        let idx = (idx as usize).min(self.instructions.len() - 1);
        for instr in self.instructions[..=idx].iter().rev() {
            let name = match instr {
                Instruction::Lookup(name)
                | Instruction::StoreLocal(name)
                | Instruction::CallFunction(name, _) => *name,
                Instruction::PushLoop(flags) if flags & LOOP_FLAG_WITH_LOOP_VAR != 0 => "loop",
                Instruction::PushLoop(_) | Instruction::PushWith => break,
                _ => continue,
            };
            if !rv.contains(&name) {
                rv.push(name);
            }
        }
        rv
    }

    /// Returns the number of instructions
    pub fn len(&self) -> usize {
        self.instructions.len()
    }

    /// Do we have any instructions?
    #[allow(unused)]
    pub fn is_empty(&self) -> bool {
        self.instructions.is_empty()
    }
}

#[cfg(feature = "internal_debug")]
impl fmt::Debug for Instructions<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct InstructionWrapper<'a>(usize, &'a Instruction<'a>, Option<usize>);

        impl fmt::Debug for InstructionWrapper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                ok!(write!(f, "{:>05} | {:?}", self.0, self.1,));
                if let Some(line) = self.2 {
                    ok!(write!(f, "  [line {line}]"));
                }
                Ok(())
            }
        }

        let mut list = f.debug_list();
        let mut last_line = None;
        for (idx, instr) in self.instructions.iter().enumerate() {
            let line = self.get_line(idx as u32);
            list.entry(&InstructionWrapper(
                idx,
                instr,
                if line != last_line { line } else { None },
            ));
            last_line = line;
        }
        list.finish()
    }
}

#[test]
#[cfg(target_pointer_width = "64")]
fn test_sizes() {
    assert_eq!(std::mem::size_of::<Instruction>(), 32);
}
