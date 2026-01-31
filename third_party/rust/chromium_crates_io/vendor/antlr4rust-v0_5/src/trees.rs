/*!
A set of utility routines useful for all kinds of ANTLR trees.
*/

use std::ops::Deref;

use crate::tree::Tree;
use crate::utils;

/// Print out a whole tree, not just a node, in LISP format
/// {@code (root child1 .. childN)}. Print just a node if this is a leaf.
pub fn string_tree<'a, T: Tree<'a> + ?Sized>(tree: &T, rule_names: &[&str]) -> String {
    let s = utils::escape_whitespaces(get_node_text(tree, rule_names), false);
    if tree.get_child_count() == 0 {
        return s;
    }
    let mut result = String::new();
    result.push('(');
    result.push_str(&s);
    result = tree
        .get_children()
        // .iter()
        .map(|child| string_tree(child.deref(), rule_names))
        .fold(result, |mut acc, text| {
            acc.push(' ');
            acc.push_str(&text);
            acc
        });
    result.push(')');
    result
}

/// Print out tree node text representation (rule name or token text)
pub fn get_node_text<'a>(t: &(impl Tree<'a> + ?Sized), rule_names: &[&str]) -> String {
    t.get_node_text(rule_names)
}

//pub fn get_children(t: impl Tree) -> Vec<Rc<dyn Tree>> { unimplemented!() }
//
//pub fn get_ancestors(t: impl Tree) -> Vec<Rc<dyn Tree>> { unimplemented!() }
//
//pub fn find_all_token_nodes(t: impl ParseTree, ttype: i32) -> Vec<Rc<dyn ParseTree>> { unimplemented!() }
//
//pub fn find_all_rule_nodes(t: impl ParseTree, rule_index: i32) -> Vec<Rc<dyn ParseTree>> { unimplemented!() }
//
//pub fn find_all_nodes(t: impl ParseTree, index: i32, find_tokens: bool) -> Vec<Rc<dyn ParseTree>> { unimplemented!() }
//
////fn trees_find_all_nodes(t: ParseTree, index: i32, findTokens: bool, nodes: * Vec<ParseTree>) { unimplemented!() }
//
//pub fn descendants(t: impl ParseTree) -> Vec<dyn ParseTree> { unimplemented!() }
