//! General AST
use std::any::Any;
use std::borrow::Borrow;

use std::fmt::{Debug, Formatter};
use std::iter::from_fn;
use std::marker::PhantomData;
use std::ops::Deref;
use std::rc::Rc;

use crate::char_stream::InputData;
use crate::errors::ANTLRError;
use crate::int_stream::EOF;
use crate::interval_set::Interval;
use crate::parser::ParserNodeType;
use crate::parser_rule_context::ParserRuleContext;
use crate::recognizer::Recognizer;
use crate::rule_context::{CustomRuleContext, RuleContext};
use crate::token::Token;
use crate::token_factory::TokenFactory;
use crate::{interval_set, trees, CoerceTo};
use std::mem;

//todo try to make in more generic
#[allow(missing_docs)]
pub trait Tree<'input>: RuleContext<'input> {
    fn get_parent(&self) -> Option<Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> {
        None
    }
    fn has_parent(&self) -> bool {
        false
    }
    fn get_payload(&self) -> Box<dyn Any> {
        unimplemented!()
    }
    fn get_child(&self, _i: usize) -> Option<Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> {
        None
    }
    fn get_child_count(&self) -> usize {
        0
    }
    fn get_children<'a>(
        &'a self,
    ) -> Box<dyn Iterator<Item = Rc<<Self::Ctx as ParserNodeType<'input>>::Type>> + 'a>
    where
        'input: 'a,
    {
        let mut index = 0;
        let iter = from_fn(move || {
            if index < self.get_child_count() {
                index += 1;
                self.get_child(index - 1)
            } else {
                None
            }
        });

        Box::new(iter)
    }
    // fn get_children_full(&self) -> &RefCell<Vec<Rc<<Self::Ctx as ParserNodeType<'input, Self::TF>>::Type>>> { unimplemented!() }
}

/// Tree that knows about underlying text
pub trait ParseTree<'input>: Tree<'input> {
    /// Return an {@link Interval} indicating the index in the
    /// {@link TokenStream} of the first and last token associated with this
    /// subtree. If this node is a leaf, then the interval represents a single
    /// token and has interval i..i for token index i.
    fn get_source_interval(&self) -> Interval {
        interval_set::INVALID
    }

    /// Return combined text of this AST node.
    /// To create resulting string it does traverse whole subtree,
    /// also it includes only tokens added to the parse tree
    ///
    /// Since tokens on hidden channels (e.g. whitespace or comments) are not
    ///	added to the parse trees, they will not appear in the output of this
    ///	method.
    fn get_text(&self) -> String {
        String::new()
    }

    /// Print out a whole tree, not just a node, in LISP format
    /// (root child1 .. childN). Print just a node if this is a leaf.
    /// We have to know the recognizer so we can get rule names.
    fn to_string_tree(
        &self,
        r: &dyn Recognizer<'input, TF = Self::TF, Node = Self::Ctx>,
    ) -> String {
        trees::string_tree(self, r.get_rule_names())
    }
}

/// text of the node.
/// Already implemented for all rule contexts
// pub trait NodeText {
//     fn get_node_text(&self, rule_names: &[&str]) -> String;
// }
//
// impl<T> NodeText for T {
//     default fn get_node_text(&self, _rule_names: &[&str]) -> String { "<unknown>".to_owned() }
// }
//
// impl<'input, T: CustomRuleContext<'input>> NodeText for T {
//     default fn get_node_text(&self, rule_names: &[&str]) -> String {
//         let rule_index = self.get_rule_index();
//         let rule_name = rule_names[rule_index];
//         let alt_number = self.get_alt_number();
//         if alt_number != INVALID_ALT {
//             return format!("{}:{}", rule_name, alt_number);
//         }
//         return rule_name.to_owned();
//     }
// }

#[doc(hidden)]
#[derive(Debug)]
pub struct NoError;

#[doc(hidden)]
#[derive(Debug)]
pub struct IsError;

/// Generic leaf AST node
pub struct LeafNode<'input, Node: ParserNodeType<'input>, T: 'static> {
    /// Token, this leaf consist of
    pub symbol: <Node::TF as TokenFactory<'input>>::Tok,
    iserror: PhantomData<T>,
}
better_any::tid! { impl <'input, Node, T:'static> TidAble<'input> for LeafNode<'input, Node, T> where Node:ParserNodeType<'input> }

impl<'input, Node: ParserNodeType<'input>, T: 'static> CustomRuleContext<'input>
    for LeafNode<'input, Node, T>
{
    type TF = Node::TF;
    type Ctx = Node;

    fn get_rule_index(&self) -> usize {
        usize::max_value()
    }

    fn get_node_text(&self, _rule_names: &[&str]) -> String {
        self.symbol.borrow().get_text().to_display()
    }
}

impl<'input, Node: ParserNodeType<'input>, T: 'static> ParserRuleContext<'input>
    for LeafNode<'input, Node, T>
{
}

impl<'input, Node: ParserNodeType<'input>, T: 'static> Tree<'input> for LeafNode<'input, Node, T> {}

impl<'input, Node: ParserNodeType<'input>, T: 'static> RuleContext<'input>
    for LeafNode<'input, Node, T>
{
}

// impl<'input, Node: ParserNodeType<'input>, T: 'static> NodeText for LeafNode<'input, Node, T> {
//     fn get_node_text(&self, _rule_names: &[&str]) -> String {
//         self.symbol.borrow().get_text().to_display()
//     }
// }

impl<'input, Node: ParserNodeType<'input>, T: 'static> ParseTree<'input>
    for LeafNode<'input, Node, T>
{
    fn get_text(&self) -> String {
        self.symbol.borrow().get_text().to_display()
    }
}

impl<'input, Node: ParserNodeType<'input>, T: 'static> Debug for LeafNode<'input, Node, T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        if self.symbol.borrow().get_token_type() == EOF {
            f.write_str("<EOF>")
        } else {
            let a = self.symbol.borrow().get_text().to_display();
            f.write_str(&a)
        }
    }
}

impl<'input, Node: ParserNodeType<'input>, T: 'static> LeafNode<'input, Node, T> {
    /// creates new leaf node
    pub fn new(symbol: <Node::TF as TokenFactory<'input>>::Tok) -> Self {
        Self {
            symbol,
            iserror: Default::default(),
        }
    }
}

/// non-error AST leaf node
pub type TerminalNode<'input, NodeType> = LeafNode<'input, NodeType, NoError>;

impl<'input, Node: ParserNodeType<'input>, Listener: ParseTreeListener<'input, Node> + ?Sized>
    Listenable<Listener> for TerminalNode<'input, Node>
{
    fn enter(&self, listener: &mut Listener) -> Result<(), ANTLRError> {
        listener.visit_terminal(self);
        Ok(())
    }
}

impl<'input, Node: ParserNodeType<'input>, Visitor: ParseTreeVisitor<'input, Node> + ?Sized>
    Visitable<Visitor> for TerminalNode<'input, Node>
{
    fn accept(&self, visitor: &mut Visitor) {
        visitor.visit_terminal(self)
    }
}

/// # Error Leaf
/// Created for each token created or consumed during recovery
pub type ErrorNode<'input, NodeType> = LeafNode<'input, NodeType, IsError>;

impl<'input, Node: ParserNodeType<'input>, Listener: ParseTreeListener<'input, Node> + ?Sized>
    Listenable<Listener> for ErrorNode<'input, Node>
{
    fn enter(&self, listener: &mut Listener) -> Result<(), ANTLRError> {
        listener.visit_error_node(self);
        Ok(())
    }
}

impl<'input, Node: ParserNodeType<'input>, Visitor: ParseTreeVisitor<'input, Node> + ?Sized>
    Visitable<Visitor> for ErrorNode<'input, Node>
{
    fn accept(&self, visitor: &mut Visitor) {
        visitor.visit_error_node(self)
    }
}

pub trait ParseTreeVisitorCompat<'input>: VisitChildren<'input, Self::Node> {
    type Node: ParserNodeType<'input>;
    type Return: Default;

    /// Temporary storage for `ParseTreeVisitor` blanket implementation to work
    ///
    /// If you have `()` as a return value
    /// either use `YourGrammarParseTreeVisitor` directly
    /// or make
    /// ```rust
    /// Box::leak(Box::new(()))
    /// # ;
    /// ```
    /// as an implementation of that method so that there is no need to create dummy field in your visitor
    fn temp_result(&mut self) -> &mut Self::Return;

    fn visit(&mut self, node: &<Self::Node as ParserNodeType<'input>>::Type) -> Self::Return {
        self.visit_node(node);
        mem::take(self.temp_result())
    }

    /// Called on terminal(leaf) node
    fn visit_terminal(&mut self, _node: &TerminalNode<'input, Self::Node>) -> Self::Return {
        Self::Return::default()
    }
    /// Called on error node
    fn visit_error_node(&mut self, _node: &ErrorNode<'input, Self::Node>) -> Self::Return {
        Self::Return::default()
    }

    fn visit_children(
        &mut self,
        node: &<Self::Node as ParserNodeType<'input>>::Type,
    ) -> Self::Return {
        let mut result = Self::Return::default();
        for node in node.get_children() {
            if !self.should_visit_next_child(&node, &result) {
                break;
            }

            let child_result = self.visit(&node);
            result = self.aggregate_results(result, child_result);
        }
        result
    }

    fn aggregate_results(&self, _aggregate: Self::Return, next: Self::Return) -> Self::Return {
        next
    }

    fn should_visit_next_child(
        &self,
        _node: &<Self::Node as ParserNodeType<'input>>::Type,
        _current: &Self::Return,
    ) -> bool {
        true
    }
}

// struct VisitorAdapter<'input, T: ParseTreeVisitorCompat<'input>> {
//     visitor: T,
//     pub curr_value: T::Return,
//     _pd: PhantomData<&'input str>,
// }

impl<'input, Node, T> ParseTreeVisitor<'input, Node> for T
where
    Node: ParserNodeType<'input>,
    Node::Type: VisitableDyn<Self>,
    T: ParseTreeVisitorCompat<'input, Node = Node>,
{
    fn visit_terminal(&mut self, node: &TerminalNode<'input, Node>) {
        let result = <Self as ParseTreeVisitorCompat>::visit_terminal(self, node);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
    }

    fn visit_error_node(&mut self, node: &ErrorNode<'input, Node>) {
        let result = <Self as ParseTreeVisitorCompat>::visit_error_node(self, node);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
    }

    fn visit_children(&mut self, node: &Node::Type) {
        let result = <Self as ParseTreeVisitorCompat>::visit_children(self, node);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
    }
}

/// Base interface for visiting over syntax tree
pub trait ParseTreeVisitor<'input, Node: ParserNodeType<'input>>:
    VisitChildren<'input, Node>
{
    /// Basically alias for `node.accept(self)` in visitor implementation
    /// just to make api closer to java

    /// Called on terminal(leaf) node
    fn visit_terminal(&mut self, _node: &TerminalNode<'input, Node>) {}
    /// Called on error node
    fn visit_error_node(&mut self, _node: &ErrorNode<'input, Node>) {}
    /// Implement this only if you want to change children visiting algorithm
    fn visit_children(&mut self, node: &Node::Type) {
        node.get_children()
            .for_each(|child| self.visit_node(&child))
    }
}

/// Workaround for default recursive children visiting
///
/// Already blanket implemented for all visitors.
/// To override it you would need to implement `ParseTreeVisitor::visit_children`
pub trait VisitChildren<'input, Node: ParserNodeType<'input>> {
    // fn visit_children_inner(&mut self, node: &Node::Type);
    fn visit_node(&mut self, node: &Node::Type);
}

impl<'input, Node, T> VisitChildren<'input, Node> for T
where
    Node: ParserNodeType<'input>,
    T: ParseTreeVisitor<'input, Node> + ?Sized,
    // for<'a> &'a mut Self: CoerceUnsized<&'a mut Node::Visitor>,
    Node::Type: VisitableDyn<T>,
{
    // #[inline(always)]
    // fn visit_children_inner(&mut self, node: &Node::Type) {
    //     // node.accept_children(self)
    //
    // }

    fn visit_node(&mut self, node: &Node::Type) {
        node.accept_dyn(self)
    }
}

/// Types that can accept particular visitor
/// ** Usually implemented only in generated parser **
pub trait Visitable<Vis: ?Sized> {
    /// Calls corresponding visit callback on visitor`Vis`
    fn accept(&self, _visitor: &mut Vis) {
        if cfg!(feature = "debug") {
            unreachable!("should have been properly implemented by generated context when reachable")
        }
    }
}

// workaround trait for accepting sized visitor on rule context trait object
#[doc(hidden)]
pub trait VisitableDyn<Vis: ?Sized> {
    fn accept_dyn(&self, _visitor: &mut Vis) {
        if cfg!(feature = "debug") {
            unreachable!("should have been properly implemented by generated context when reachable")
        }
    }
}

/// Base parse listener interface
pub trait ParseTreeListener<'input, Node: ParserNodeType<'input>> {
    /// Called when parser creates terminal node
    fn visit_terminal(&mut self, _node: &TerminalNode<'input, Node>) {}
    /// Called when parser creates error node
    fn visit_error_node(&mut self, _node: &ErrorNode<'input, Node>) {}
    /// Called when parser enters any rule node
    fn enter_every_rule(&mut self, _ctx: &Node::Type) -> Result<(), ANTLRError> {
        Ok(())
    }
    /// Called when parser exits any rule node
    fn exit_every_rule(&mut self, _ctx: &Node::Type) -> Result<(), ANTLRError> {
        Ok(())
    }
}

/// Types that can accept particular listener
/// ** Usually implemented only in generated parser **
pub trait Listenable<T: ?Sized> {
    /// Calls corresponding enter callback on listener `T`
    fn enter(&self, _listener: &mut T) -> Result<(), ANTLRError> {
        Ok(())
    }
    /// Calls corresponding exit callback on listener `T`
    fn exit(&self, _listener: &mut T) -> Result<(), ANTLRError> {
        Ok(())
    }
}

// #[inline]
// pub fn temp_to_trait<Z,TraitObject>(mut input: Z, f:impl FnOnce(&mut TraitObject)) -> Z where &mut Z:CoerceUnsized<&mut TraitObject>{
//     let a = &mut input as &mut TraitObject;
//     f(a)
// }

/// Helper struct to accept parse listener on already generated tree
#[derive(Debug)]
pub struct ParseTreeWalker<'input, 'a, Node, T = dyn ParseTreeListener<'input, Node> + 'a>(
    PhantomData<fn(&'a T) -> &'input Node::Type>,
)
where
    Node: ParserNodeType<'input>,
    T: ParseTreeListener<'input, Node> + ?Sized;

impl<'input, 'a, Node, T> ParseTreeWalker<'input, 'a, Node, T>
where
    Node: ParserNodeType<'input>,
    T: ParseTreeListener<'input, Node> + 'a + ?Sized,
    Node::Type: Listenable<T>,
{
    /// Walks recursively over tree `t` with `listener`
    pub fn walk<Listener, Ctx>(
        mut listener: Box<Listener>,
        t: &Ctx,
    ) -> Result<Box<Listener>, ANTLRError>
    where
        // for<'x> &'x mut Listener: CoerceUnsized<&'x mut T>,
        // for<'x> &'x Ctx: CoerceUnsized<&'x Node::Type>,
        Listener: CoerceTo<T>,
        Ctx: CoerceTo<Node::Type>,
    {
        // let mut listener = listener as Box<T>;
        Self::walk_inner(listener.as_mut().coerce_mut_to(), t.coerce_ref_to())?;

        // just cast back
        // unsafe { Box::<Listener>::from_raw(Box::into_raw(listener) as *mut _) }
        Ok(listener)
    }

    fn walk_inner(listener: &mut T, t: &Node::Type) -> Result<(), ANTLRError> {
        t.enter(listener)?;

        for child in t.get_children() {
            Self::walk_inner(listener, child.deref())?;
        }

        t.exit(listener)?;
        Ok(())
    }
}
