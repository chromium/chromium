use std::collections::HashSet;
use std::fmt::Write;

use crate::compiler::ast;

struct AssignmentTracker<'a> {
    out: HashSet<&'a str>,
    nested_out: Option<HashSet<String>>,
    assigned: Vec<HashSet<&'a str>>,
}

impl<'a> AssignmentTracker<'a> {
    fn is_assigned(&self, name: &str) -> bool {
        self.assigned.iter().any(|x| x.contains(name))
    }

    fn assign(&mut self, name: &'a str) {
        self.assigned.last_mut().unwrap().insert(name);
    }

    fn assign_nested(&mut self, name: String) {
        if let Some(ref mut nested_out) = self.nested_out {
            if !nested_out.contains(&name) {
                nested_out.insert(name);
            }
        }
    }

    fn push(&mut self) {
        self.assigned.push(Default::default());
    }

    fn pop(&mut self) {
        self.assigned.pop();
    }
}

/// Finds all variables that need to be captured as closure for a macro.
#[cfg(feature = "macros")]
pub fn find_macro_closure<'a>(m: &ast::Macro<'a>) -> HashSet<&'a str> {
    let mut state = AssignmentTracker {
        out: HashSet::new(),
        nested_out: None,
        assigned: vec![Default::default()],
    };
    tracker_visit_macro(m, &mut state, false);
    state.out
}

/// Finds all variables that are undeclared in a template.
pub fn find_undeclared(t: &ast::Stmt<'_>, track_nested: bool) -> HashSet<String> {
    let mut state = AssignmentTracker {
        out: HashSet::new(),
        nested_out: if track_nested {
            Some(HashSet::new())
        } else {
            None
        },
        assigned: vec![Default::default()],
    };
    track_walk(t, &mut state);
    if let Some(nested) = state.nested_out {
        nested
    } else {
        state.out.into_iter().map(|x| x.to_string()).collect()
    }
}

fn tracker_visit_expr_opt<'a>(expr: &Option<ast::Expr<'a>>, state: &mut AssignmentTracker<'a>) {
    if let Some(expr) = expr {
        tracker_visit_expr(expr, state);
    }
}

#[cfg(feature = "macros")]
fn tracker_visit_macro<'a>(
    m: &ast::Macro<'a>,
    state: &mut AssignmentTracker<'a>,
    declare_caller: bool,
) {
    if declare_caller {
        // this is not completely correct as caller is actually only defined
        // if the macro was used in the context of a call block.  However it
        // is impossible to determine this at compile time so we err on the
        // side of assuming caller is there.
        state.assign("caller");
    }
    m.args.iter().for_each(|arg| track_assign(arg, state));
    m.defaults
        .iter()
        .for_each(|expr| tracker_visit_expr(expr, state));
    m.body.iter().for_each(|node| track_walk(node, state));
}

fn tracker_visit_callarg<'a>(callarg: &ast::CallArg<'a>, state: &mut AssignmentTracker<'a>) {
    match callarg {
        ast::CallArg::Pos(expr)
        | ast::CallArg::Kwarg(_, expr)
        | ast::CallArg::PosSplat(expr)
        | ast::CallArg::KwargSplat(expr) => tracker_visit_expr(expr, state),
    }
}

fn tracker_visit_expr<'a>(expr: &ast::Expr<'a>, state: &mut AssignmentTracker<'a>) {
    match expr {
        ast::Expr::Var(var) => {
            if !state.is_assigned(var.id) {
                state.out.insert(var.id);
                // if we are not tracking nested assignments, we can consider a variable
                // to be assigned the first time we perform a lookup.
                if state.nested_out.is_none() {
                    state.assign(var.id);
                } else {
                    state.assign_nested(var.id.to_string());
                }
            }
        }
        ast::Expr::Const(_) => {}
        ast::Expr::UnaryOp(expr) => tracker_visit_expr(&expr.expr, state),
        ast::Expr::BinOp(expr) => {
            tracker_visit_expr(&expr.left, state);
            tracker_visit_expr(&expr.right, state);
        }
        ast::Expr::IfExpr(expr) => {
            tracker_visit_expr(&expr.test_expr, state);
            tracker_visit_expr(&expr.true_expr, state);
            tracker_visit_expr_opt(&expr.false_expr, state);
        }
        ast::Expr::Filter(expr) => {
            tracker_visit_expr_opt(&expr.expr, state);
            expr.args
                .iter()
                .for_each(|x| tracker_visit_callarg(x, state));
        }
        ast::Expr::Test(expr) => {
            tracker_visit_expr(&expr.expr, state);
            expr.args
                .iter()
                .for_each(|x| tracker_visit_callarg(x, state));
        }
        ast::Expr::GetAttr(expr) => {
            // if we are tracking nested, we check if we have a chain of attribute
            // lookups that terminate in a variable lookup.  In that case we can
            // assign the nested lookup.
            if state.nested_out.is_some() {
                let mut attrs = vec![expr.name];
                let mut ptr = &expr.expr;
                loop {
                    match ptr {
                        ast::Expr::Var(var) => {
                            if !state.is_assigned(var.id) {
                                let mut rv = var.id.to_string();
                                for attr in attrs.iter().rev() {
                                    write!(rv, ".{attr}").ok();
                                }
                                state.assign_nested(rv);
                                return;
                            } else {
                                break;
                            }
                        }
                        ast::Expr::GetAttr(expr) => {
                            attrs.push(expr.name);
                            ptr = &expr.expr;
                            continue;
                        }
                        _ => break,
                    }
                }
            }
            tracker_visit_expr(&expr.expr, state)
        }
        ast::Expr::GetItem(expr) => {
            tracker_visit_expr(&expr.expr, state);
            tracker_visit_expr(&expr.subscript_expr, state);
        }
        ast::Expr::Slice(slice) => {
            tracker_visit_expr_opt(&slice.start, state);
            tracker_visit_expr_opt(&slice.stop, state);
            tracker_visit_expr_opt(&slice.step, state);
        }
        ast::Expr::Call(expr) => {
            tracker_visit_expr(&expr.expr, state);
            expr.args
                .iter()
                .for_each(|x| tracker_visit_callarg(x, state));
        }
        ast::Expr::List(expr) => expr.items.iter().for_each(|x| tracker_visit_expr(x, state)),
        ast::Expr::Map(expr) => expr.keys.iter().zip(expr.values.iter()).for_each(|(k, v)| {
            tracker_visit_expr(k, state);
            tracker_visit_expr(v, state);
        }),
    }
}

fn track_assign<'a>(expr: &ast::Expr<'a>, state: &mut AssignmentTracker<'a>) {
    match expr {
        ast::Expr::Var(var) => state.assign(var.id),
        ast::Expr::List(list) => list.items.iter().for_each(|x| track_assign(x, state)),
        _ => {}
    }
}

fn track_walk<'a>(node: &ast::Stmt<'a>, state: &mut AssignmentTracker<'a>) {
    match node {
        ast::Stmt::Template(stmt) => {
            state.assign("self");
            stmt.children.iter().for_each(|x| track_walk(x, state));
        }
        ast::Stmt::EmitExpr(expr) => tracker_visit_expr(&expr.expr, state),
        ast::Stmt::EmitRaw(_) => {}
        ast::Stmt::ForLoop(stmt) => {
            state.push();
            state.assign("loop");
            tracker_visit_expr(&stmt.iter, state);
            track_assign(&stmt.target, state);
            tracker_visit_expr_opt(&stmt.filter_expr, state);
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
            state.push();
            stmt.else_body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        ast::Stmt::IfCond(stmt) => {
            tracker_visit_expr(&stmt.expr, state);
            state.push();
            stmt.true_body.iter().for_each(|x| track_walk(x, state));
            state.pop();
            state.push();
            stmt.false_body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        ast::Stmt::WithBlock(stmt) => {
            state.push();
            for (target, expr) in &stmt.assignments {
                track_assign(target, state);
                tracker_visit_expr(expr, state);
            }
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        ast::Stmt::Set(stmt) => {
            track_assign(&stmt.target, state);
            tracker_visit_expr(&stmt.expr, state);
        }
        ast::Stmt::AutoEscape(stmt) => {
            state.push();
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        ast::Stmt::FilterBlock(stmt) => {
            state.push();
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        ast::Stmt::SetBlock(stmt) => {
            track_assign(&stmt.target, state);
            state.push();
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        #[cfg(feature = "multi_template")]
        ast::Stmt::Block(stmt) => {
            state.push();
            state.assign("super");
            stmt.body.iter().for_each(|x| track_walk(x, state));
            state.pop();
        }
        #[cfg(feature = "multi_template")]
        ast::Stmt::Extends(_) | ast::Stmt::Include(_) => {}
        #[cfg(feature = "multi_template")]
        ast::Stmt::Import(stmt) => {
            track_assign(&stmt.name, state);
        }
        #[cfg(feature = "multi_template")]
        ast::Stmt::FromImport(stmt) => stmt.names.iter().for_each(|(arg, alias)| {
            track_assign(alias.as_ref().unwrap_or(arg), state);
        }),
        #[cfg(feature = "macros")]
        ast::Stmt::Macro(stmt) => {
            state.assign(stmt.name);
            state.push();
            tracker_visit_macro(stmt, state, true);
            state.pop();
        }
        #[cfg(feature = "macros")]
        ast::Stmt::CallBlock(stmt) => {
            tracker_visit_expr(&stmt.call.expr, state);
            stmt.call
                .args
                .iter()
                .for_each(|x| tracker_visit_callarg(x, state));
            state.push();
            tracker_visit_macro(&stmt.macro_decl, state, true);
            state.pop();
        }
        #[cfg(feature = "loop_controls")]
        ast::Stmt::Continue(_) | ast::Stmt::Break(_) => {}
        ast::Stmt::Do(stmt) => {
            tracker_visit_expr(&stmt.call.expr, state);
            stmt.call
                .args
                .iter()
                .for_each(|x| tracker_visit_callarg(x, state));
        }
    }
}
