//! Full parser node
use std::any::{type_name, Any};
use std::borrow::{Borrow, BorrowMut};
use std::cell::{Ref, RefCell, RefMut};
use std::fmt::{Debug, Error, Formatter};
use std::ops::{Deref, DerefMut};
use std::rc::Rc;

use better_any::{Tid, TidAble, TidExt};

use crate::errors::ANTLRError;
use crate::parser::ParserNodeType;
use crate::rule_context::{BaseRuleContext, CustomRuleContext, RuleContext};
use crate::token::Token;
use crate::token_factory::TokenFactory;
use crate::tree::{ParseTree, ParseTreeVisitor, TerminalNode, Tree, VisitableDyn};
use crate::CoerceTo;

/// Syntax tree node for particular parser rule.
///
/// Not yet good for custom implementations so currently easiest option
/// is to just copy `BaseParserRuleContext` or `BaseRuleContext` and strip/extend them
#[allow(missing_docs)]
pub trait ParserRuleContext<'input>:
    ParseTree<'input> + RuleContext<'input> + Debug + Tid<'input>
{
    fn set_exception(&self, _e: ANTLRError) {}

    fn set_start(&self, _t: Option<<Self::TF as TokenFactory<'input>>::Tok>) {}

    /// Get the initial token in this context.
    /// Note that the range from start to stop is inclusive, so for rules that do not consume anything
    /// (for example, zero length or error productions) this token may exceed stop.
    ///
    fn start<'a>(&'a self) -> Ref<'a, <Self::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        unimplemented!()
    }
    fn start_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        unimplemented!()
    }

    fn set_stop(&self, _t: Option<<Self::TF as TokenFactory<'input>>::Tok>) {}
    ///
    /// Get the final token in this context.
    /// Note that the range from start to stop is inclusive, so for rules that do not consume anything
    /// (for example, zero length or error productions) this token may precede start.
    ///
    fn stop<'a>(&'a self) -> Ref<'a, <Self::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        unimplemented!()
    }
    fn stop_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        unimplemented!()
    }

    // fn add_token_node(&self, token: TerminalNode<'input, Self::TF>) { }
    // fn add_error_node(&self, bad_token: ErrorNode<'input, Self::TF>) { }

    fn add_child(&self, _child: Rc<<Self::Ctx as ParserNodeType<'input>>::Type>) {}
    fn remove_last_child(&self) {}

    // fn enter_rule(&self, listener: &mut dyn Any);
    // fn exit_rule(&self, listener: &mut dyn Any);

    fn child_of_type<T>(&self, pos: usize) -> Option<Rc<T>>
    where
        T: ParserRuleContext<'input, TF = Self::TF, Ctx = Self::Ctx> + 'input,
        Self: Sized,
    {
        self.get_children()
            .filter(|it| it.deref().self_id() == T::id())
            .nth(pos)
            .and_then(|it| it.downcast_rc().ok())
    }

    // todo, return iterator
    fn children_of_type<T>(&self) -> Vec<Rc<T>>
    where
        T: ParserRuleContext<'input, TF = Self::TF, Ctx = Self::Ctx> + 'input,
        Self: Sized,
    {
        self.get_children()
            // .filter(|it| it.deref().self_id() == T::id())
            .filter_map(|it| it.downcast_rc().ok())
            .collect()
    }

    fn get_token(&self, ttype: i32, pos: usize) -> Option<Rc<TerminalNode<'input, Self::Ctx>>> {
        self.get_children()
            // .filter(|it| it.deref().self_id() == TerminalNode::<'input, Self::Ctx>::id())
            .filter_map(|it| it.downcast_rc::<TerminalNode<'input, Self::Ctx>>().ok())
            .filter(|it| it.symbol.borrow().get_token_type() == ttype)
            .nth(pos)
    }

    fn get_tokens(&self, ttype: i32) -> Vec<Rc<TerminalNode<'input, Self::Ctx>>> {
        self.get_children()
            // .iter()
            .filter_map(|it| it.downcast_rc::<TerminalNode<'input, Self::Ctx>>().ok())
            // .filter(|it| it.deref().self_id() == TerminalNode::<'input, Self::Ctx>::id())
            // .map(|it| cast_rc::<'input, TerminalNode<'input, Self::Ctx>>(it.clone()))
            .filter(|it| it.symbol.borrow().get_token_type() == ttype)
            .collect()
    }

    // fn upcast(&self) -> &dyn ParserRuleContext<'input, TF=Self::TF>;
}

//Allows to implement generic functions on trait object as well
/// Extention trait implemented for all `ParserRuleContext`s
pub trait RuleContextExt<'input>: ParserRuleContext<'input> {
    /// Prints list of parent rules
    fn to_string<Z>(self: &Rc<Self>, rule_names: Option<&[&str]>, stop: Option<Rc<Z>>) -> String
    where
        Z: ParserRuleContext<'input, Ctx = Self::Ctx, TF = Self::TF> + ?Sized + 'input,
        Self::Ctx: ParserNodeType<'input, Type = Z>,
        Self: CoerceTo<Z>;

    #[doc(hidden)]
    fn accept_children<V>(&self, visitor: &mut V)
    where
        V: ParseTreeVisitor<'input, Self::Ctx> + ?Sized,
        <Self::Ctx as ParserNodeType<'input>>::Type: VisitableDyn<V>;
}

impl<'input, T: ParserRuleContext<'input> + ?Sized + 'input> RuleContextExt<'input> for T {
    fn to_string<Z>(self: &Rc<Self>, rule_names: Option<&[&str]>, stop: Option<Rc<Z>>) -> String
    where
        Z: ParserRuleContext<'input, Ctx = T::Ctx, TF = T::TF> + ?Sized + 'input,
        T::Ctx: ParserNodeType<'input, Type = Z>,
        T: CoerceTo<Z>,
    {
        let mut result = String::from("[");
        let mut next: Option<Rc<Z>> = Some(self.clone().coerce_rc_to());
        while let Some(ref p) = next {
            if stop.is_some() && (stop.is_none() || Rc::ptr_eq(p, stop.as_ref().unwrap())) {
                break;
            }

            if let Some(rule_names) = rule_names {
                let rule_index = p.get_rule_index();
                let rule_name = rule_names
                    .get(rule_index)
                    .map(|&it| it.to_owned())
                    .unwrap_or_else(|| rule_index.to_string());
                result.push_str(&rule_name);
                result.push(' ');
            } else if !p.is_empty() {
                result.push_str(&p.get_invoking_state().to_string());
                result.push(' ');
            }

            next = p.get_parent().clone();
        }

        if result.ends_with(' ') {
            result.pop();
        }

        result.push(']');
        result
    }

    fn accept_children<V>(&self, visitor: &mut V)
    where
        V: ParseTreeVisitor<'input, Self::Ctx> + ?Sized,
        <Self::Ctx as ParserNodeType<'input>>::Type: VisitableDyn<V>,
    {
        self.get_children()
            .for_each(|child| child.accept_dyn(visitor))
    }
}

#[inline]
#[doc(hidden)]
pub fn cast<'a, T, Result>(ctx: &T) -> &Result
where
    T: ParserRuleContext<'a> + 'a + ?Sized,
    Result: ParserRuleContext<'a, Ctx = T::Ctx> + 'a,
{
    ctx.downcast_ref().unwrap()
    // unsafe { &*(ctx as *const T as *const Result) }
}

/// Should be called from generated parser only
/// Don't call it by yourself.
#[inline]
#[doc(hidden)]
// technically should be unsafe but in order to not force unsafe into user code
// it is just #[doc(hidden)]
pub fn cast_mut<'a, T: ParserRuleContext<'a> + 'a + ?Sized, Result: 'a>(
    ctx: &mut Rc<T>,
) -> &mut Result {
    //    if Rc::strong_count(ctx) != 1 { panic!("cant mutate Rc with multiple strong ref count"); }
    // is it safe because parser does not save/move mutable references anywhere.
    // they are only used to write data immediately in the corresponding expression
    // unsafe { &mut *(Rc::get_mut_unchecked(ctx) as *mut T as *mut Result) }
    unsafe {
        let ptr = Rc::as_ptr(ctx) as *mut T as *mut Result;

        &mut *ptr
    }
}

// workaround newtype for cycle in trait definition
// i.e. you can't have `trait ParserRuleContext:BaseTrait<dyn ParserRuleContext>`
// #[derive(Clone)]
// pub struct ParseTreeNode<'input,TF:TokenFactory<'input>>(pub Rc<dyn ParserRuleContext<'input,TF=TF>>);
//
// impl<'input,TF:TokenFactory<'input>> Deref for ParseTreeNode<'input,TF>{
//     type Target = dyn ParserRuleContext<'input,TF=TF>;
//
//     fn deref(&self) -> &Self::Target {
//         self.0.deref()
//     }
// }

// pub type ParserRuleContextType<'input, T> = Rc<dyn ParserRuleContext<'input, Ctx=T> + 'input>;
// pub type ParserRuleContextType<'input,T> = ParseTreeNode<'input,T>;

/// Default rule context implementation that keeps everything provided by parser
pub struct BaseParserRuleContext<'input, Ctx: CustomRuleContext<'input>> {
    base: BaseRuleContext<'input, Ctx>,

    start: RefCell<<Ctx::TF as TokenFactory<'input>>::Tok>,
    stop: RefCell<<Ctx::TF as TokenFactory<'input>>::Tok>,
    /// error if there was any in this node
    pub exception: Option<Box<ANTLRError>>,
    /// List of children of current node
    pub(crate) children: RefCell<Vec<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>>>,
}

better_any::tid! { impl<'i,Ctx> TidAble<'i> for BaseParserRuleContext<'i,Ctx> where Ctx:CustomRuleContext<'i> }

impl<'input, Ctx: CustomRuleContext<'input>> Debug for BaseParserRuleContext<'input, Ctx> {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        f.write_str(type_name::<Self>())
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> RuleContext<'input>
    for BaseParserRuleContext<'input, Ctx>
{
    fn get_invoking_state(&self) -> i32 {
        self.base.get_invoking_state()
    }

    fn set_invoking_state(&self, t: i32) {
        self.base.set_invoking_state(t)
    }

    fn get_parent_ctx(&self) -> Option<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>> {
        self.base.get_parent_ctx()
    }

    fn set_parent(&self, parent: &Option<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>>) {
        self.base.set_parent(parent)
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> CustomRuleContext<'input>
    for BaseParserRuleContext<'input, Ctx>
{
    type TF = Ctx::TF;
    type Ctx = Ctx::Ctx;

    fn get_rule_index(&self) -> usize {
        self.base.ext.get_rule_index()
    }
}

// unsafe impl<'input, Ctx: CustomRuleContext<'input>> Tid for BaseParserRuleContext<'input, Ctx> {
//     fn self_id(&self) -> TypeId { self.base.ext.self_id() }
//
//     fn id() -> TypeId
//     where
//         Self: Sized,
//     {
//         Ctx::id()
//     }
// }

impl<'input, Ctx: CustomRuleContext<'input>> Deref for BaseParserRuleContext<'input, Ctx> {
    type Target = Ctx;

    fn deref(&self) -> &Self::Target {
        &self.base.ext
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> DerefMut for BaseParserRuleContext<'input, Ctx> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base.ext
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> Borrow<Ctx> for BaseParserRuleContext<'input, Ctx> {
    fn borrow(&self) -> &Ctx {
        &self.base.ext
    }
}

impl<'input, Ctx: CustomRuleContext<'input>> BorrowMut<Ctx> for BaseParserRuleContext<'input, Ctx> {
    fn borrow_mut(&mut self) -> &mut Ctx {
        &mut self.base.ext
    }
}

impl<'input, Ctx: CustomRuleContext<'input> + TidAble<'input>> ParserRuleContext<'input>
    for BaseParserRuleContext<'input, Ctx>
{
    fn set_exception(&self, _e: ANTLRError) { /*self.exception = Some(Box::new(e));*/
    }

    fn set_start(&self, t: Option<<Ctx::TF as TokenFactory<'input>>::Tok>) {
        *self.start.borrow_mut() = t.unwrap_or(Ctx::TF::create_invalid().clone());
    }

    fn start<'a>(&'a self) -> Ref<'a, <Ctx::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        Ref::map(self.start.borrow(), |t| t.borrow())
    }

    fn start_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        self.start.borrow_mut()
    }

    fn set_stop(&self, t: Option<<Ctx::TF as TokenFactory<'input>>::Tok>) {
        *self.stop.borrow_mut() = t.unwrap_or(Ctx::TF::create_invalid().clone());
    }

    fn stop<'a>(&'a self) -> Ref<'a, <Ctx::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        Ref::map(self.stop.borrow(), |t| t.borrow())
    }

    fn stop_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        self.stop.borrow_mut()
    }

    //     fn add_token_node(&self, token: TerminalNode<'input, Ctx::TF>) -> ParserRuleContextType<'input, Ctx::TF> {
    //         let node: ParserRuleContextType<'input, Ctx::TF> = Rc::new(token);
    //         self.children.borrow_mut().push(node.clone());
    //         node
    //     }
    //
    //     fn add_error_node(&self, bad_token: ErrorNode<'input, Ctx::TF>) -> ParserRuleContextType<'input, Ctx::TF> {
    // //        bad_token.base.parent_ctx =
    //         let node: ParserRuleContextType<'input, Ctx::TF> = Rc::new(bad_token);
    // //        Backtrace::new().frames()[0].symbols()[0];
    //
    //         self.children.borrow_mut().push(node.clone());
    //         node
    //     }

    fn add_child(&self, child: Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>) {
        self.children.borrow_mut().push(child);
    }

    fn remove_last_child(&self) {
        self.children.borrow_mut().pop();
    }

    // fn enter_rule(&self, listener: &mut dyn Any) {
    //     Ctx::enter(self, listener)
    // }
    //
    // fn exit_rule(&self, listener: &mut dyn Any) {
    //     Ctx::exit(self, listener)
    // }
    //
    // fn upcast(&self) -> &dyn ParserRuleContext<'input, TF=Ctx::TF> {
    //     self
    // }
}

impl<'input, Ctx: CustomRuleContext<'input>> Tree<'input> for BaseParserRuleContext<'input, Ctx> {
    fn get_parent(&self) -> Option<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>> {
        self.get_parent_ctx()
    }

    fn has_parent(&self) -> bool {
        self.base.parent_ctx.borrow().is_some()
    }

    fn get_payload(&self) -> Box<dyn Any> {
        unimplemented!()
    }

    fn get_child(&self, i: usize) -> Option<Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> {
        self.children.borrow().get(i).cloned()
    }

    fn get_child_count(&self) -> usize {
        self.children.borrow().len()
    }

    // fn get_children<'a>(&'a self) -> Box<dyn ExactSizeIterator<Item=Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> + 'a> where 'input:'a{
    //     let len = self.children.borrow().len();
    //
    //     Box::new(IndexIter::new(self.children.borrow(),len))
    // }

    // fn get_children_full(&self) -> &RefCell<Vec<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>>> {
    //     &self.children
    // }
}

impl<'input, Ctx: CustomRuleContext<'input> + TidAble<'input>> ParseTree<'input>
    for BaseParserRuleContext<'input, Ctx>
{


    fn get_text(&self) -> String {
        let children = self.get_children();
        let mut result = String::new();

        for child in children {
            result += &child.get_text()
        }

        result
    }
}
#[allow(missing_docs)]
impl<'input, Ctx: CustomRuleContext<'input> + 'input> BaseParserRuleContext<'input, Ctx> {
    pub fn new_parser_ctx(
        parent_ctx: Option<Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>>,
        invoking_state: i32,
        ext: Ctx,
    ) -> Self {
        Self {
            base: BaseRuleContext::new_parser_ctx(parent_ctx, invoking_state, ext),
            start: RefCell::new(Ctx::TF::create_invalid()),
            stop: RefCell::new(Ctx::TF::create_invalid()),
            exception: None,
            children: RefCell::new(vec![]),
        }
    }
    pub fn copy_from<T: ParserRuleContext<'input, TF = Ctx::TF, Ctx = Ctx::Ctx> + ?Sized>(
        ctx: &T,
        ext: Ctx,
    ) -> Self {
        Self {
            base: BaseRuleContext::new_parser_ctx(
                ctx.get_parent_ctx(),
                ctx.get_invoking_state(),
                ext,
            ),
            start: RefCell::new(ctx.start_mut().clone()),
            stop: RefCell::new(ctx.stop_mut().clone()),
            exception: None,
            children: RefCell::new(ctx.get_children().collect()),
        }
    }

    // pub fn to_string(self: Rc<Self>, rule_names: Option<&[&str]>, stop: Option<Rc<Ctx::Ctx::Type>>) -> String {
    //     (self as Rc<<Ctx::Ctx as ParserNodeType<'input>>::Type>).to_string(rule_names, stop)
    // }
}

///////////////////////////////////////////////
// Needed to significantly reduce boilerplate in the generated code,
// because there is no simple way to delegate trait for enum.
// Will not be necessary if some kind of variant types RFC will be merged
//////////////////////////////////////////////
/// workaround trait to overcome conflicting implementations error
#[doc(hidden)]
pub trait DerefSeal: Deref {}

impl<'input, T, I> ParserRuleContext<'input> for T
where
    T: DerefSeal<Target = I> + 'input + Debug + Tid<'input>,
    I: ParserRuleContext<'input> + 'input + ?Sized,
{
    fn set_exception(&self, e: ANTLRError) {
        self.deref().set_exception(e)
    }

    fn set_start(&self, t: Option<<Self::TF as TokenFactory<'input>>::Tok>) {
        self.deref().set_start(t)
    }

    fn start<'a>(&'a self) -> Ref<'a, <Self::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        self.deref().start()
    }

    fn start_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        self.deref().start_mut()
    }

    fn set_stop(&self, t: Option<<Self::TF as TokenFactory<'input>>::Tok>) {
        self.deref().set_stop(t)
    }

    fn stop<'a>(&'a self) -> Ref<'a, <Self::TF as TokenFactory<'input>>::Inner>
    where
        'input: 'a,
    {
        self.deref().stop()
    }

    fn stop_mut<'a>(&'a self) -> RefMut<'a, <Self::TF as TokenFactory<'input>>::Tok>
    where
        'input: 'a,
    {
        self.deref().stop_mut()
    }

    fn add_child(&self, child: Rc<<I::Ctx as ParserNodeType<'input>>::Type>) {
        self.deref().add_child(child)
    }

    fn remove_last_child(&self) {
        self.deref().remove_last_child()
    }

    // fn enter_rule(&self, listener: &mut dyn Any) { self.deref().enter_rule(listener) }
    //
    // fn exit_rule(&self, listener: &mut dyn Any) { self.deref().exit_rule(listener) }
    //
    // fn upcast(&self) -> &dyn ParserRuleContext<'input, TF=Self::TF> { self.deref().upcast() }
}

impl<'input, T, I> RuleContext<'input> for T
where
    T: DerefSeal<Target = I> + 'input + Debug + Tid<'input>,
    I: ParserRuleContext<'input> + 'input + ?Sized,
{
    fn get_invoking_state(&self) -> i32 {
        self.deref().get_invoking_state()
    }

    fn set_invoking_state(&self, t: i32) {
        self.deref().set_invoking_state(t)
    }

    fn is_empty(&self) -> bool {
        self.deref().is_empty()
    }

    fn get_parent_ctx(&self) -> Option<Rc<<I::Ctx as ParserNodeType<'input>>::Type>> {
        self.deref().get_parent_ctx()
    }

    fn set_parent(&self, parent: &Option<Rc<<I::Ctx as ParserNodeType<'input>>::Type>>) {
        self.deref().set_parent(parent)
    }
}

impl<'input, T, I> ParseTree<'input> for T
where
    T: DerefSeal<Target = I> + 'input + Debug + Tid<'input>,
    I: ParserRuleContext<'input> + 'input + ?Sized,
{

    fn get_text(&self) -> String {
        self.deref().get_text()
    }
}

impl<'input, T, I> Tree<'input> for T
where
    T: DerefSeal<Target = I> + 'input + Debug + Tid<'input>,
    I: ParserRuleContext<'input> + 'input + ?Sized,
{
    fn get_parent(&self) -> Option<Rc<<I::Ctx as ParserNodeType<'input>>::Type>> {
        self.deref().get_parent()
    }

    fn has_parent(&self) -> bool {
        self.deref().has_parent()
    }

    fn get_payload(&self) -> Box<dyn Any> {
        self.deref().get_payload()
    }

    fn get_child(&self, i: usize) -> Option<Rc<<I::Ctx as ParserNodeType<'input>>::Type>> {
        self.deref().get_child(i)
    }

    fn get_child_count(&self) -> usize {
        self.deref().get_child_count()
    }

    fn get_children<'a>(
        &'a self,
    ) -> Box<dyn Iterator<Item = Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> + 'a>
    where
        'input: 'a,
    {
        self.deref().get_children()
    }

    // fn get_children_full(&self) -> &RefCell<Vec<Rc<<I::Ctx as ParserNodeType<'input>>::Type>>> { self.deref().get_children_full() }
}

impl<'input, T, I> CustomRuleContext<'input> for T
where
    T: DerefSeal<Target = I> + 'input + Debug + Tid<'input>,
    I: ParserRuleContext<'input> + 'input + ?Sized,
{
    type TF = I::TF;
    type Ctx = I::Ctx;

    fn get_rule_index(&self) -> usize {
        self.deref().get_rule_index()
    }

    // fn type_rule_index() -> usize where Self: Sized { unimplemented!() }

    fn get_alt_number(&self) -> i32 {
        self.deref().get_alt_number()
    }

    fn set_alt_number(&self, _alt_number: i32) {
        self.deref().set_alt_number(_alt_number)
    }
}

//
//    fn get_text(&self) -> String { unimplemented!() }
//
//    fn add_terminal_node_child(&self, child: TerminalNode) -> TerminalNode { unimplemented!() }
//
//    fn get_child_of_type(&self, i: i32, childType: reflect.Type) -> RuleContext { unimplemented!() }
//
//    fn to_string_tree(&self, ruleNames Vec<String>, recog: Recognizer) -> String { unimplemented!() }
//
//    fn get_rule_context(&self) -> RuleContext { unimplemented!() }
//
//    fn accept(&self, visitor: ParseTreeVisitor) -> interface { unimplemented!() } {
//    return visitor.VisitChildren(prc)
//    }
//
//    fn get_token(&self, ttype: i32, i: i32) -> TerminalNode { unimplemented!() }
//
//    fn get_tokens(&self, ttype: i32) -> Vec<TerminalNode> { unimplemented!() }
//
//    fn get_payload(&self) -> interface { unimplemented!() } {
//    return: prc,
//    }
//
//    fn get_child(&self, ctxType: reflect.Type, i: i32) -> RuleContext { unimplemented!() }
//
//
//    fn get_typed_rule_context(&self, ctxType: reflect.Type, i: i32) -> RuleContext { unimplemented!() }
//
//    fn get_typed_rule_contexts(&self, ctxType: reflect.Type) -> Vec<RuleContext> { unimplemented!() }
//
//    fn get_child_count(&self) -> int { unimplemented!() }
//
//    fn get_source_interval(&self) -> * Interval { unimplemented!() }
//
//
//    fn String(&self, ruleNames Vec<String>, stop: RuleContext) -> String { unimplemented!() }
//
//    var RuleContextEmpty = NewBaseParserRuleContext(nil, - 1)
//
//    pub trait InterpreterRuleContext {
//    parser_rule_context
//    }
//
//    pub struct BaseInterpreterRuleContext {
//    base: BaseParserRuleContext,
//    }
//
//    fn new_base_interpreter_rule_context(parent BaseInterpreterRuleContext, invokingStateNumber: i32, ruleIndex: i32) -> * BaseInterpreterRuleContext { unimplemented!() }
