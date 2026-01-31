use std::borrow::Cow;
use std::collections::{BTreeMap, HashSet};
use std::fmt;

#[cfg(feature = "macros")]
use std::sync::Arc;

use crate::environment::Environment;
use crate::error::{Error, ErrorKind};
use crate::value::Value;
use crate::vm::loop_object::LoopState;

#[cfg(feature = "macros")]
use crate::vm::closure_object::Closure;

type Locals<'env> = BTreeMap<&'env str, Value>;

pub(crate) struct Frame<'env> {
    pub(crate) locals: Locals<'env>,
    pub(crate) ctx: Value,
    pub(crate) current_loop: Option<LoopState>,

    // normally a frame does not carry a closure, but it can when a macro is
    // declared.  Once that happens, all writes to the frames locals are also
    // duplicated into the closure.  Macros declared on that level, then share
    // the closure object to enclose the parent values.  This emulates the
    // behavior of closures in Jinja2.
    #[cfg(feature = "macros")]
    pub(crate) closure: Option<Arc<Closure>>,
}

impl<'env> Default for Frame<'env> {
    fn default() -> Frame<'env> {
        Frame::new(Value::UNDEFINED)
    }
}

impl<'env> Frame<'env> {
    /// Creates a new frame with the given context and no validation
    pub fn new(ctx: Value) -> Frame<'env> {
        Frame {
            locals: Locals::new(),
            ctx,
            current_loop: None,
            #[cfg(feature = "macros")]
            closure: None,
        }
    }

    /// Creates a new frame with the given context and validates the value is not invalid
    pub fn new_checked(root: Value) -> Result<Frame<'env>, Error> {
        Ok(Frame::new(ok!(root.validate())))
    }
}

#[cfg(feature = "internal_debug")]
impl fmt::Debug for Frame<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut m = f.debug_map();
        m.entry(&"locals", &self.locals);
        if let Some(LoopState {
            object: ref controller,
            ..
        }) = self.current_loop
        {
            m.entry(&"loop", controller);
        }
        if !self.ctx.is_undefined() {
            m.entry(&"ctx", &self.ctx);
        }
        m.finish()
    }
}

#[cfg_attr(feature = "internal_debug", derive(Debug))]
pub(crate) struct Stack {
    values: Vec<Value>,
}

impl Default for Stack {
    fn default() -> Stack {
        Stack {
            values: Vec::with_capacity(16),
        }
    }
}

impl Stack {
    pub fn push(&mut self, arg: Value) {
        self.values.push(arg);
    }

    #[track_caller]
    pub fn pop(&mut self) -> Value {
        self.values.pop().unwrap()
    }

    pub fn reverse_top(&mut self, n: usize) {
        let start = self.values.len() - n;
        self.values[start..].reverse();
    }

    pub fn get_call_args(&mut self, n: Option<u16>) -> &[Value] {
        let n = match n {
            Some(n) => n as usize,
            None => self.pop().as_usize().unwrap(),
        };
        &self.values[self.values.len() - n..]
    }

    pub fn drop_top(&mut self, n: usize) {
        self.values.truncate(self.values.len() - n);
    }

    pub fn try_pop(&mut self) -> Option<Value> {
        self.values.pop()
    }

    #[track_caller]
    pub fn peek(&self) -> &Value {
        self.values.last().unwrap()
    }
}

impl From<Vec<Value>> for Stack {
    fn from(values: Vec<Value>) -> Stack {
        Stack { values }
    }
}

pub(crate) struct Context<'env> {
    env: &'env Environment<'env>,
    stack: Vec<Frame<'env>>,
    outer_stack_depth: usize,
    recursion_limit: usize,
}

impl fmt::Debug for Context<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut vars = Vec::from_iter(self.known_variables(false));
        vars.sort();
        f.debug_map()
            .entries(vars.into_iter().map(|key| {
                let value = self.load(&key).unwrap_or_default();
                (key, value)
            }))
            .finish()
    }
}

impl<'env> Context<'env> {
    /// Creates an empty context.
    pub fn new(env: &'env Environment<'env>) -> Context<'env> {
        Context {
            env,
            stack: Vec::with_capacity(32),
            outer_stack_depth: 0,
            recursion_limit: env.recursion_limit(),
        }
    }

    /// Creates a context
    pub fn new_with_frame(env: &'env Environment<'env>, frame: Frame<'env>) -> Context<'env> {
        let mut rv = Context::new(env);
        rv.stack.push(frame);
        rv
    }

    /// The env
    #[inline(always)]
    pub fn env(&self) -> &'env Environment<'env> {
        self.env
    }

    /// Stores a variable in the context.
    pub fn store(&mut self, key: &'env str, value: Value) {
        let top = self.stack.last_mut().unwrap();
        #[cfg(feature = "macros")]
        {
            if let Some(ref closure) = top.closure {
                closure.store(key, value.clone());
            }
        }
        top.locals.insert(key, value);
    }

    /// Adds a value to a closure if missing.
    ///
    /// All macros declare on a certain level reuse the same closure.  This is done
    /// to emulate the behavior of how scopes work in Jinja2 in Python.  The
    /// unfortunate downside is that this has to be done with a `Mutex`.
    #[cfg(feature = "macros")]
    pub fn enclose(&mut self, key: &str) {
        self.stack
            .last_mut()
            .unwrap()
            .closure
            .as_mut()
            .unwrap()
            .clone()
            .store_if_missing(key, || self.load(key).unwrap_or(Value::UNDEFINED));
    }

    /// Loads the closure and returns it.
    #[cfg(feature = "macros")]
    pub fn closure(&mut self) -> Option<&Arc<Closure>> {
        self.stack.last_mut().unwrap().closure.as_ref()
    }

    /// Temporarily takes the closure.
    ///
    /// This is done because includes are in the same scope as the module that
    /// triggers the import, but we do not want to allow closures to be modified
    /// from another file as this would be very confusing.
    ///
    /// This means that if you override a variable referenced by a macro after
    /// including in the parent template, it will not override the value seen by
    /// the macro.
    #[cfg(all(feature = "multi_template", feature = "macros"))]
    pub fn take_closure(&mut self) -> Option<Arc<Closure>> {
        self.stack.last_mut().unwrap().closure.take()
    }

    /// Puts the closure back.
    #[cfg(feature = "macros")]
    pub fn reset_closure(&mut self, closure: Option<Arc<Closure>>) {
        self.stack.last_mut().unwrap().closure = closure;
    }

    /// Return the base context value
    #[cfg(feature = "macros")]
    pub fn clone_base(&self) -> Value {
        self.stack
            .first()
            .map(|x| x.ctx.clone())
            .unwrap_or_default()
    }

    /// Looks up a variable in the context.
    pub fn load(&self, key: &str) -> Option<Value> {
        for frame in self.stack.iter().rev() {
            // look at locals first
            if let Some(value) = frame.locals.get(key) {
                return Some(value.clone());
            }

            // if we are a loop, check if we are looking up the special loop var.
            if let Some(ref l) = frame.current_loop {
                if l.with_loop_var && key == "loop" {
                    return Some(Value::from_dyn_object(l.object.clone()));
                }
            }

            // perform a fast lookup.  This one will not produce errors if the
            // context is undefined or of the wrong type.
            if let Some(rv) = frame.ctx.get_attr_fast(key) {
                return Some(rv);
            }
        }

        self.env.get_global(key)
    }

    /// Returns an iterable of all declared variables.
    pub fn known_variables(&self, with_globals: bool) -> HashSet<Cow<'_, str>> {
        let mut seen = HashSet::<Cow<'_, str>>::new();
        for frame in self.stack.iter().rev() {
            for key in frame.locals.keys() {
                if !seen.contains(&Cow::Borrowed(*key)) {
                    seen.insert(Cow::Borrowed(key));
                }
            }

            if let Some(ref l) = frame.current_loop {
                if l.with_loop_var && !seen.contains("loop") {
                    seen.insert(Cow::Borrowed("loop"));
                }
            }

            if let Ok(iter) = frame.ctx.try_iter() {
                for key in iter {
                    if let Some(str_key) = key.as_str() {
                        if !seen.contains(&Cow::Borrowed(str_key))
                            && frame.ctx.get_item(&key).is_ok()
                        {
                            seen.insert(Cow::Owned(str_key.to_owned()));
                        }
                    }
                }
            }
        }
        if with_globals {
            seen.extend(self.env.globals().map(|x| Cow::Borrowed(x.0)));
        }
        seen
    }

    /// Pushes a new layer.
    pub fn push_frame(&mut self, layer: Frame<'env>) -> Result<(), Error> {
        ok!(self.check_depth());
        self.stack.push(layer);
        Ok(())
    }

    /// Pops the topmost layer.
    #[track_caller]
    pub fn pop_frame(&mut self) -> Frame<'env> {
        self.stack.pop().unwrap()
    }

    /// Returns the root locals (exports)
    #[track_caller]
    pub fn exports(&self) -> &Locals<'env> {
        &self.stack.first().unwrap().locals
    }

    /// Returns the current locals mutably.
    #[track_caller]
    #[cfg(feature = "multi_template")]
    pub fn current_locals_mut(&mut self) -> &mut Locals<'env> {
        &mut self.stack.last_mut().unwrap().locals
    }

    /// Returns the current innermost loop state.
    pub fn current_loop(&mut self) -> Option<&mut LoopState> {
        self.stack
            .iter_mut()
            .rev()
            .filter_map(|x| x.current_loop.as_mut())
            .next()
    }

    /// The real depth of the context.
    pub fn depth(&self) -> usize {
        self.outer_stack_depth + self.stack.len()
    }

    /// Increase the stack depth.
    #[allow(unused)]
    pub fn incr_depth(&mut self, delta: usize) -> Result<(), Error> {
        self.outer_stack_depth += delta;
        ok!(self.check_depth());
        Ok(())
    }

    /// Decrease the stack depth.
    #[allow(unused)]
    pub fn decr_depth(&mut self, delta: usize) {
        self.outer_stack_depth -= delta;
    }

    fn check_depth(&self) -> Result<(), Error> {
        if self.depth() > self.recursion_limit {
            return Err(Error::new(
                ErrorKind::InvalidOperation,
                "recursion limit exceeded",
            ));
        }
        Ok(())
    }
}
