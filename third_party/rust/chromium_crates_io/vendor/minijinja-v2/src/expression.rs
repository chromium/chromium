use std::collections::{BTreeMap, HashSet};
use std::fmt;

use serde::Serialize;

use crate::compiler::ast;
use crate::compiler::instructions::Instructions;
use crate::compiler::meta::find_undeclared;
use crate::compiler::parser::parse_expr;
use crate::environment::Environment;
use crate::error::Error;
use crate::output::Output;
use crate::value::Value;
use crate::vm::Vm;

/// A handle to a compiled expression.
///
/// An expression is created via the
/// [`compile_expression`](Environment::compile_expression) method.  It provides
/// a method to evaluate the expression and return the result as value object.
/// This for instance can be used to evaluate simple expressions from user
/// provided input to implement features such as dynamic filtering.
///
/// This is usually best paired with [`context`](crate::context!) to pass
/// a single value to it.
///
/// # Example
///
/// ```rust
/// # use minijinja::{Environment, context};
/// let env = Environment::new();
/// let expr = env.compile_expression("number > 10 and number < 20").unwrap();
/// let rv = expr.eval(context!(number => 15)).unwrap();
/// assert!(rv.is_true());
/// ```
pub struct Expression<'env, 'source> {
    env: &'env Environment<'source>,
    instr: ExpressionBacking<'source>,
}

enum ExpressionBacking<'source> {
    Borrowed(Instructions<'source>),
    #[cfg(feature = "loader")]
    Owned(crate::loader::OwnedInstructions),
}

impl fmt::Debug for Expression<'_, '_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Expression")
            .field("env", &self.env)
            .finish()
    }
}

impl<'env, 'source> Expression<'env, 'source> {
    pub(crate) fn new(
        env: &'env Environment<'source>,
        instructions: Instructions<'source>,
    ) -> Expression<'env, 'source> {
        Expression {
            env,
            instr: ExpressionBacking::Borrowed(instructions),
        }
    }

    #[cfg(feature = "loader")]
    pub(crate) fn new_owned(
        env: &'env Environment<'source>,
        instructions: crate::loader::OwnedInstructions,
    ) -> Expression<'env, 'source> {
        Expression {
            env,
            instr: ExpressionBacking::Owned(instructions),
        }
    }

    fn instructions(&self) -> &Instructions<'_> {
        match self.instr {
            ExpressionBacking::Borrowed(ref x) => x,
            #[cfg(feature = "loader")]
            ExpressionBacking::Owned(ref x) => x.borrow_dependent(),
        }
    }

    /// Evaluates the expression with some context.
    ///
    /// The result of the expression is returned as [`Value`].
    pub fn eval<S: Serialize>(&self, ctx: S) -> Result<Value, Error> {
        // reduce total amount of code falling under mono morphization into
        // this function, and share the rest in _eval.
        self._eval(Value::from_serialize(&ctx))
    }

    /// Returns a set of all undeclared variables in the expression.
    ///
    /// This works the same as
    /// [`Template::undeclared_variables`](crate::Template::undeclared_variables).
    pub fn undeclared_variables(&self, nested: bool) -> HashSet<String> {
        match parse_expr(self.instructions().source()) {
            Ok(expr) => find_undeclared(
                &ast::Stmt::EmitExpr(ast::Spanned::new(
                    ast::EmitExpr { expr },
                    Default::default(),
                )),
                nested,
            ),
            Err(_) => HashSet::new(),
        }
    }

    fn _eval(&self, root: Value) -> Result<Value, Error> {
        Ok(ok!(Vm::new(self.env).eval(
            self.instructions(),
            root,
            &BTreeMap::new(),
            &mut Output::null(),
            crate::AutoEscape::None,
        ))
        .0
        .expect("expression evaluation did not leave value on stack"))
    }
}
