//! Minimal parser node
use std::borrow::{Borrow, BorrowMut};
use std::cell::{Cell, RefCell};
use std::fmt::{Debug, Formatter};
use std::iter::from_fn;
use std::marker::PhantomData;
use std::rc::{Rc, Weak};

use crate::atn::INVALID_ALT;
use crate::parser::ParserNodeType;
use crate::parser_rule_context::ParserRuleContext;
use crate::token_factory::TokenFactory;
use crate::tree::{ParseTree, Tree};
use better_any::TidAble;
use std::any::type_name;

//pub trait RuleContext:RuleNode {
/// Minimal rule context functionality required for parser to work properly
pub trait RuleContext<'input>: CustomRuleContext<'input> {
    /// Internal parser state
    fn get_invoking_state(&self) -> i32 {
        -1
    }

    /// Sets internal parser state
    fn set_invoking_state(&self, _t: i32) {}

    /// A context is empty if there is no invoking state; meaning nobody called
    /// current context. Which is usually true for the root of the syntax tree
    fn is_empty(&self) -> bool {
        self.get_invoking_state() == -1
    }

    /// Get parent context
    fn get_parent_ctx(&self) -> Option<Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> {
        None
    }

    /// Set parent context
    fn set_parent(&self, _parent: &Option<Rc<<Self::Ctx as ParserNodeType<'input>>::Type>>) {}
}

pub(crate) fn states_stack<'input, T: ParserRuleContext<'input> + ?Sized + 'input>(
    mut ctx: Rc<T>,
) -> impl Iterator<Item = i32>
where
    T::Ctx: ParserNodeType<'input, Type = T>,
{
    from_fn(move || {
        if ctx.get_invoking_state() < 0 {
            None
        } else {
            let state = ctx.get_invoking_state();
            ctx = ctx.get_parent_ctx().unwrap();
            Some(state)
        }
    })
}

// #[doc(hidden)]
// pub unsafe trait Tid {
//     fn self_id(&self) -> TypeId;
//     fn id() -> TypeId
//     where
//         Self: Sized;
// }

#[derive(Debug)]
#[doc(hidden)]
pub struct EmptyCustomRuleContext<'a, TF: TokenFactory<'a> + 'a>(
    pub(crate) PhantomData<&'a TF::Tok>,
);

better_any::tid! { impl <'a,TF> TidAble<'a> for EmptyCustomRuleContext<'a,TF> where TF:TokenFactory<'a> + 'a}

impl<'a, TF: TokenFactory<'a> + 'a> CustomRuleContext<'a> for EmptyCustomRuleContext<'a, TF> {
    type TF = TF;
    type Ctx = EmptyContextType<'a, TF>;

    fn get_rule_index(&self) -> usize {
        usize::max_value()
    }
}

// unsafe impl<'a, TF: TokenFactory<'a> + 'a> Tid for EmptyCustomRuleContext<'a, TF> {
//     fn self_id(&self) -> TypeId {
//         TypeId::of::<EmptyCustomRuleContext<'static, CommonTokenFactory>>()
//     }
//
//     fn id() -> TypeId
//     where
//         Self: Sized,
//     {
//         TypeId::of::<EmptyCustomRuleContext<'static, CommonTokenFactory>>()
//     }
// }
#[doc(hidden)] // public for implementation reasons
pub type EmptyContext<'a, TF> =
    dyn ParserRuleContext<'a, TF = TF, Ctx = EmptyContextType<'a, TF>> + 'a;

#[derive(Debug)]
#[doc(hidden)] // public for implementation reasons
pub struct EmptyContextType<'a, TF: TokenFactory<'a>>(pub PhantomData<&'a TF>);

better_any::tid! { impl <'a,TF> TidAble<'a> for EmptyContextType<'a,TF> where TF:TokenFactory<'a> }

impl<'a, TF: TokenFactory<'a>> ParserNodeType<'a> for EmptyContextType<'a, TF> {
    type TF = TF;
    type Type = dyn ParserRuleContext<'a, TF = Self::TF, Ctx = Self> + 'a;
    // type Visitor = dyn ParseTreeVisitor<'a, Self> + 'a;
}

/// Implemented by generated parser for context extension for particular rule
#[allow(missing_docs)]
pub trait CustomRuleContext<'input> {
    type TF: TokenFactory<'input> + 'input;
    /// Type that describes type of context nodes, stored in this context
    type Ctx: ParserNodeType<'input, TF = Self::TF>;
    //const RULE_INDEX:usize;
    /// Rule index that corresponds to this context type
    fn get_rule_index(&self) -> usize;

    fn get_alt_number(&self) -> i32 {
        INVALID_ALT
    }
    fn set_alt_number(&self, _alt_number: i32) {}

    /// Returns text representation of current node type,
    /// rule name for context nodes and token text for terminal nodes
    fn get_node_text(&self, rule_names: &[&str]) -> String {
        let rule_index = self.get_rule_index();
        let rule_name = rule_names[rule_index];
        let alt_number = self.get_alt_number();
        if alt_number != INVALID_ALT {
            return format!("{}:{}", rule_name, alt_number);
        }
        rule_name.to_owned()
    }
    // fn enter(_ctx: &dyn Tree<'input, Node=Self>, _listener: &mut dyn Any) where Self: Sized {}
    // fn exit(_ctx: &dyn Tree<'input, Node=Self>, _listener: &mut dyn Any) where Self: Sized {}
}

/// Minimal parse tree node implementation, that stores only data required for correct parsing
pub struct BaseRuleContext<'input, ExtCtx: CustomRuleContext<'input>> {
    pub(crate) parent_ctx: RefCell<Option<Weak<<ExtCtx::Ctx as ParserNodeType<'input>>::Type>>>,
    invoking_state: Cell<i32>,
    pub(crate) ext: ExtCtx,
}

better_any::tid! { impl <'input,Ctx> TidAble<'input> for BaseRuleContext<'input,Ctx> where Ctx:CustomRuleContext<'input>}

#[allow(missing_docs)]
impl<'input, ExtCtx: CustomRuleContext<'input>> BaseRuleContext<'input, ExtCtx> {
    pub fn new_parser_ctx(
        parent_ctx: Option<Rc<<ExtCtx::Ctx as ParserNodeType<'input>>::Type>>,
        invoking_state: i32,
        ext: ExtCtx,
    ) -> Self {
        Self {
            parent_ctx: RefCell::new(parent_ctx.as_ref().map(Rc::downgrade)),
            invoking_state: Cell::new(invoking_state),
            ext,
        }
    }

    pub fn copy_from<T: ParserRuleContext<'input, TF = ExtCtx::TF, Ctx = ExtCtx::Ctx> + ?Sized>(
        ctx: &T,
        ext: ExtCtx,
    ) -> Self {
        Self::new_parser_ctx(ctx.get_parent_ctx(), ctx.get_invoking_state(), ext)
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> Borrow<Ctx> for BaseRuleContext<'input, Ctx> {
    fn borrow(&self) -> &Ctx {
        &self.ext
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> BorrowMut<Ctx> for BaseRuleContext<'input, Ctx> {
    fn borrow_mut(&mut self) -> &mut Ctx {
        &mut self.ext
    }
}

impl<'input, ExtCtx: CustomRuleContext<'input>> CustomRuleContext<'input>
    for BaseRuleContext<'input, ExtCtx>
{
    type TF = ExtCtx::TF;
    type Ctx = ExtCtx::Ctx;

    fn get_rule_index(&self) -> usize {
        self.ext.get_rule_index()
    }
}

// unsafe impl<'input, Ctx: CustomRuleContext<'input>> Tid for BaseRuleContext<'input, Ctx> {
//     fn self_id(&self) -> TypeId { self.ext.self_id() }
//
//     fn id() -> TypeId
//     where
//         Self: Sized,
//     {
//         Ctx::id()
//     }
// }

impl<'input, ExtCtx: CustomRuleContext<'input>> RuleContext<'input>
    for BaseRuleContext<'input, ExtCtx>
{
    fn get_invoking_state(&self) -> i32 {
        self.invoking_state.get()
    }

    fn set_invoking_state(&self, t: i32) {
        self.invoking_state.set(t)
    }

    fn get_parent_ctx(&self) -> Option<Rc<<ExtCtx::Ctx as ParserNodeType<'input>>::Type>> {
        self.parent_ctx.borrow().as_ref().and_then(Weak::upgrade)
    }

    //    fn get_parent_ctx(&self) -> Option<ParserRuleContextType> {
    //        self.parent_ctx.borrow().as_ref().map(Weak::upgrade).map(Option::unwrap)
    //    }

    fn set_parent(&self, parent: &Option<Rc<<ExtCtx::Ctx as ParserNodeType<'input>>::Type>>) {
        *self.parent_ctx.borrow_mut() = parent.as_ref().map(Rc::downgrade);
    }
}

impl<'input, ExtCtx: CustomRuleContext<'input>> Debug for BaseRuleContext<'input, ExtCtx> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct(type_name::<Self>())
            .field("invoking_state", &self.invoking_state)
            .field("..", &"..")
            .finish()
    }
}

impl<'input, ExtCtx: CustomRuleContext<'input>> Tree<'input> for BaseRuleContext<'input, ExtCtx> {}

impl<'input, ExtCtx: CustomRuleContext<'input>> ParseTree<'input>
    for BaseRuleContext<'input, ExtCtx>
{
}

impl<'input, ExtCtx: CustomRuleContext<'input> + TidAble<'input>> ParserRuleContext<'input>
    for BaseRuleContext<'input, ExtCtx>
{
}
