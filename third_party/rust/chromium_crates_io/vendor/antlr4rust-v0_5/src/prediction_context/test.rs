#![allow(non_snake_case)]

use std::collections::VecDeque;

use super::*;

fn root_is_wildcard() -> bool {
    true
}

fn full_ctx() -> bool {
    false
}

#[test]
fn test_e_e() {
    let r = PredictionContext::merge(
        &EMPTY_PREDICTION_CONTEXT,
        &EMPTY_PREDICTION_CONTEXT,
        root_is_wildcard(),
        &mut None,
    );
    let expecting = "digraph G {
rankdir=LR;
  s0[label=\"*\"];
}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_e_e_fullctx() {
    let r = PredictionContext::merge(
        &EMPTY_PREDICTION_CONTEXT,
        &EMPTY_PREDICTION_CONTEXT,
        full_ctx(),
        &mut None,
    );
    let expecting = "digraph G {
rankdir=LR;
  s0[label=\"$\"];
}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_x_e() {
    let r = PredictionContext::merge(
        &x(),
        &EMPTY_PREDICTION_CONTEXT,
        root_is_wildcard(),
        &mut None,
    );
    let expecting =
        String::new() + "digraph G {\n" + "rankdir=LR;\n" + "  s0[label=\"*\"];\n" + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_x_e_fullctx() {
    let r = PredictionContext::merge(&x(), &EMPTY_PREDICTION_CONTEXT, full_ctx(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s1[label=\"$\"];\n"
        + "  s0:p0->s1[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_e_x() {
    let r = PredictionContext::merge(
        &EMPTY_PREDICTION_CONTEXT,
        &x(),
        root_is_wildcard(),
        &mut None,
    );
    let expecting =
        String::new() + "digraph G {\n" + "rankdir=LR;\n" + "  s0[label=\"*\"];\n" + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_e_x_fullctx() {
    let r = PredictionContext::merge(&EMPTY_PREDICTION_CONTEXT, &x(), full_ctx(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s1[label=\"$\"];\n"
        + "  s0:p0->s1[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_a_a() {
    let r = PredictionContext::merge(&a(), &a(), root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ae_ax() {
    let a1 = a();
    let x = x();
    let a2 = PredictionContext::new_singleton(Some(x), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ae_ax_fullctx() {
    let a1 = a();
    let x = x();
    let a2 = PredictionContext::new_singleton(Some(x), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, full_ctx(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s2[label=\"$\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1:p0->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_axe_ae() {
    let x = x();
    let a1 = PredictionContext::new_singleton(Some(x), 1).alloc();
    let a2 = a();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_aae_ae_e_fullctx() {
    let empty = EMPTY_PREDICTION_CONTEXT.clone();
    let child1 = PredictionContext::new_singleton(Some(empty.clone()), 8).alloc();
    let right = PredictionContext::merge(&empty, &child1, false, &mut None);
    let left = PredictionContext::new_singleton(Some(right.clone()), 8).alloc();
    let r = PredictionContext::merge(&left, &right, false, &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s2[label=\"$\"];\n"
        + "  s0:p0->s1[label=\"8\"];\n"
        + "  s1:p0->s2[label=\"8\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, false))
}

#[test]
fn test_axe_ae_fullctx() {
    let x = x();
    let a1 = PredictionContext::new_singleton(Some(x), 1).alloc();
    let a2 = a();
    let r = PredictionContext::merge(&a1, &a2, full_ctx(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>$\"];\n"
        + "  s2[label=\"$\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1:p0->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_a_b() {
    let r = PredictionContext::merge(&a(), &b(), root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ax_ax_same() {
    let x = x();
    let a1 = PredictionContext::new_singleton(Some(x.clone()), 1).alloc();
    let a2 = PredictionContext::new_singleton(Some(x.clone()), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ax_ax() {
    let a1 = PredictionContext::new_singleton(Some(x()), 1).alloc();
    let a2 = PredictionContext::new_singleton(Some(x()), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_abx_abx() {
    let b1 = PredictionContext::new_singleton(Some(x()), 2).alloc();
    let b2 = PredictionContext::new_singleton(Some(x()), 2).alloc();
    let a1 = PredictionContext::new_singleton(Some(b1), 1).alloc();
    let a2 = PredictionContext::new_singleton(Some(b2), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s3[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1->s2[label=\"2\"];\n"
        + "  s2->s3[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_abx_acx() {
    let b1 = PredictionContext::new_singleton(Some(x()), 2).alloc();
    let c = PredictionContext::new_singleton(Some(x()), 3).alloc();
    let a1 = PredictionContext::new_singleton(Some(b1), 1).alloc();
    let a2 = PredictionContext::new_singleton(Some(c), 1).alloc();
    let r = PredictionContext::merge(&a1, &a2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s3[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1:p0->s2[label=\"2\"];\n"
        + "  s1:p1->s2[label=\"3\"];\n"
        + "  s2->s3[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ax_bx_same() {
    let x = x();
    let a = PredictionContext::new_singleton(Some(x.clone()), 1).alloc();
    let b = PredictionContext::new_singleton(Some(x.clone()), 2).alloc();
    let r = PredictionContext::merge(&a, &b, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s1->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ax_bx() {
    let a = PredictionContext::new_singleton(Some(x()), 1).alloc();
    let b = PredictionContext::new_singleton(Some(x()), 2).alloc();
    let r = PredictionContext::merge(&a, &b, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s1->s2[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ae_bx() {
    let x2 = x();
    let a = a();
    let b = PredictionContext::new_singleton(Some(x2), 2).alloc();
    let r = PredictionContext::merge(&a, &b, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s2->s1[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_ae_bx_fullctx() {
    let x2 = x();
    let a = a();
    let b = PredictionContext::new_singleton(Some(x2), 2).alloc();
    let r = PredictionContext::merge(&a, &b, full_ctx(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s1[label=\"$\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s2->s1[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[ignore] // see org/antlr/v4/test/tool/TestGraphNodes.java:405
#[test]
fn test_aex_bfx() {
    let x1 = x();
    let x2 = x();
    let e = PredictionContext::new_singleton(Some(x1), 5).alloc();
    let f = PredictionContext::new_singleton(Some(x2), 6).alloc();
    let a = PredictionContext::new_singleton(Some(e), 1).alloc();
    let b = PredictionContext::new_singleton(Some(f), 2).alloc();
    let r = PredictionContext::merge(&a, &b, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s3[label=\"3\"];\n"
        + "  s4[label=\"*\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s2->s3[label=\"6\"];\n"
        + "  s3->s4[label=\"9\"];\n"
        + "  s1->s3[label=\"5\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Ae_Ae_fullctx() {
    let A1 = array(vec![EMPTY_PREDICTION_CONTEXT.clone()]);
    let A2 = array(vec![EMPTY_PREDICTION_CONTEXT.clone()]);
    let r = PredictionContext::merge(&A1, &A2, full_ctx(), &mut None);
    let expecting =
        String::new() + "digraph G {\n" + "rankdir=LR;\n" + "  s0[label=\"$\"];\n" + "}\n";
    assert_eq!(expecting, to_dot_string(r, full_ctx()))
}

#[test]
fn test_Aab_Ac() {
    let A1 = array(vec![a(), b()]);
    let A2 = array(vec![c()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s0:p2->s1[label=\"3\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aa_Aa() {
    let A1 = array(vec![a()]);
    let A2 = array(vec![a()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aa_Abc() {
    let A1 = array(vec![a()]);
    let A2 = array(vec![b(), c()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s0:p2->s1[label=\"3\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aac_Ab() {
    let A1 = array(vec![a(), c()]);
    let A2 = array(vec![b()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s0:p2->s1[label=\"3\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aab_Aa() {
    let A1 = array(vec![a(), b()]);
    let A2 = array(vec![a()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aab_Ab() {
    let A1 = array(vec![a(), b()]);
    let A2 = array(vec![b()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aax_Aby() {
    let a = PredictionContext::new_singleton(x().into(), 1).alloc();
    let b = PredictionContext::new_singleton(y().into(), 2).alloc();
    let A1 = array(vec![a]);
    let A2 = array(vec![b]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s3[label=\"*\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s2->s3[label=\"10\"];\n"
        + "  s1->s3[label=\"9\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aax_Aay() {
    let a1 = PredictionContext::new_singleton(x().into(), 1).alloc();
    let a2 = PredictionContext::new_singleton(y().into(), 1).alloc();
    let A1 = array(vec![a1]);
    let A2 = array(vec![a2]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[label=\"0\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0->s1[label=\"1\"];\n"
        + "  s1:p0->s2[label=\"9\"];\n"
        + "  s1:p1->s2[label=\"10\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaxc_Aayd() {
    let a1 = PredictionContext::new_singleton(x().into(), 1).alloc();
    let a2 = PredictionContext::new_singleton(y().into(), 1).alloc();
    let A1 = array(vec![a1, c()]);
    let A2 = array(vec![a2, d()]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s1[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"3\"];\n"
        + "  s0:p2->s2[label=\"4\"];\n"
        + "  s1:p0->s2[label=\"9\"];\n"
        + "  s1:p1->s2[label=\"10\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaubv_Acwdx() {
    let a = PredictionContext::new_singleton(u().into(), 1).alloc();
    let b = PredictionContext::new_singleton(v().into(), 2).alloc();
    let c = PredictionContext::new_singleton(w().into(), 3).alloc();
    let d = PredictionContext::new_singleton(x().into(), 4).alloc();
    let A1 = array(vec![a, b]);
    let A2 = array(vec![c, d]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>|<p3>\"];\n"
        + "  s4[label=\"4\"];\n"
        + "  s5[label=\"*\"];\n"
        + "  s3[label=\"3\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s0:p2->s3[label=\"3\"];\n"
        + "  s0:p3->s4[label=\"4\"];\n"
        + "  s4->s5[label=\"9\"];\n"
        + "  s3->s5[label=\"8\"];\n"
        + "  s2->s5[label=\"7\"];\n"
        + "  s1->s5[label=\"6\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaubv_Abvdx() {
    let a = PredictionContext::new_singleton(u().into(), 1).alloc();
    let b1 = PredictionContext::new_singleton(v().into(), 2).alloc();
    let b2 = PredictionContext::new_singleton(v().into(), 2).alloc();
    let d = PredictionContext::new_singleton(x().into(), 4).alloc();
    let A1 = array(vec![a, b1]);
    let A2 = array(vec![b2, d]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s3[label=\"3\"];\n"
        + "  s4[label=\"*\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s0:p2->s3[label=\"4\"];\n"
        + "  s3->s4[label=\"9\"];\n"
        + "  s2->s4[label=\"7\"];\n"
        + "  s1->s4[label=\"6\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaubv_Abwdx() {
    let a = PredictionContext::new_singleton(u().into(), 1).alloc();
    let b1 = PredictionContext::new_singleton(v().into(), 2).alloc();
    let b2 = PredictionContext::new_singleton(w().into(), 2).alloc();
    let d = PredictionContext::new_singleton(x().into(), 4).alloc();
    let A1 = array(vec![a, b1]);
    let A2 = array(vec![b2, d]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s3[label=\"3\"];\n"
        + "  s4[label=\"*\"];\n"
        + "  s2[shape=record, label=\"<p0>|<p1>\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s0:p2->s3[label=\"4\"];\n"
        + "  s3->s4[label=\"9\"];\n"
        + "  s2:p0->s4[label=\"7\"];\n"
        + "  s2:p1->s4[label=\"8\"];\n"
        + "  s1->s4[label=\"6\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaubv_Abvdu() {
    let a = PredictionContext::new_singleton(u().into(), 1).alloc();
    let b1 = PredictionContext::new_singleton(v().into(), 2).alloc();
    let b2 = PredictionContext::new_singleton(v().into(), 2).alloc();
    let d = PredictionContext::new_singleton(u().into(), 4).alloc();
    let A1 = array(vec![a, b1]);
    let A2 = array(vec![b2, d]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>\"];\n"
        + "  s2[label=\"2\"];\n"
        + "  s3[label=\"*\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s2[label=\"2\"];\n"
        + "  s0:p2->s1[label=\"4\"];\n"
        + "  s2->s3[label=\"7\"];\n"
        + "  s1->s3[label=\"6\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

#[test]
fn test_Aaubu_Acudu() {
    let a = PredictionContext::new_singleton(u().into(), 1).alloc();
    let b = PredictionContext::new_singleton(u().into(), 2).alloc();
    let c = PredictionContext::new_singleton(u().into(), 3).alloc();
    let d = PredictionContext::new_singleton(u().into(), 4).alloc();
    let A1 = array(vec![a, b]);
    let A2 = array(vec![c, d]);
    let r = PredictionContext::merge(&A1, &A2, root_is_wildcard(), &mut None);
    let expecting = String::new()
        + "digraph G {\n"
        + "rankdir=LR;\n"
        + "  s0[shape=record, label=\"<p0>|<p1>|<p2>|<p3>\"];\n"
        + "  s1[label=\"1\"];\n"
        + "  s2[label=\"*\"];\n"
        + "  s0:p0->s1[label=\"1\"];\n"
        + "  s0:p1->s1[label=\"2\"];\n"
        + "  s0:p2->s1[label=\"3\"];\n"
        + "  s0:p3->s1[label=\"4\"];\n"
        + "  s1->s2[label=\"6\"];\n"
        + "}\n";
    assert_eq!(expecting, to_dot_string(r, root_is_wildcard()))
}

fn array(nodes: Vec<Arc<PredictionContext>>) -> Arc<PredictionContext> {
    let mut parents = Vec::with_capacity(nodes.len());
    let mut invoking_states = Vec::with_capacity(nodes.len());
    for node in nodes {
        parents.push(node.get_parent(0).cloned());
        invoking_states.push(node.get_return_state(0));
    }

    PredictionContext::new_array(parents, invoking_states).alloc()
}

fn y() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 10).alloc()
}

fn x() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 9).alloc()
}

fn w() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 8).alloc()
}

fn v() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 7).alloc()
}

fn u() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 6).alloc()
}

fn d() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 4).alloc()
}

fn c() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 3).alloc()
}

fn b() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 2).alloc()
}

fn a() -> Arc<PredictionContext> {
    PredictionContext::new_singleton(Some(EMPTY_PREDICTION_CONTEXT.clone()), 1).alloc()
}

fn to_dot_string(context: Arc<PredictionContext>, is_root_wildcard: bool) -> String {
    let mut nodes = String::new();
    let mut edges = String::new();
    let mut visited = HashMap::<*const PredictionContext, Arc<PredictionContext>>::new();
    let mut context_ids = HashMap::<*const PredictionContext, usize>::new();
    let mut work_list = VecDeque::<Arc<PredictionContext>>::new();
    visited.insert(context.deref(), context.clone());
    context_ids.insert(context.deref(), context_ids.len());
    work_list.push_back(context);
    while !work_list.is_empty() {
        let current = work_list.pop_back().unwrap();
        let current_ptr = current.deref() as *const PredictionContext;
        nodes.extend(format!("  s{}[", context_ids.get(&current_ptr).unwrap()).chars());

        if current.length() > 1 {
            nodes.push_str("shape=record, ");
        }

        nodes.push_str("label=\"");

        if current.is_empty() {
            nodes.push(if is_root_wildcard { '*' } else { '$' });
        } else if current.length() > 1 {
            for i in 0..current.length() {
                if i > 0 {
                    nodes.push('|');
                }

                nodes.extend(format!("<p{}>", i).chars());
                if current.get_return_state(i) == PREDICTION_CONTEXT_EMPTY_RETURN_STATE {
                    nodes.push(if is_root_wildcard { '*' } else { '$' });
                }
            }
        } else {
            nodes.push_str(&context_ids.get(&current_ptr).unwrap().to_string());
        }

        nodes.push_str("\"];\n");

        if current.is_empty() {
            continue;
        }

        for i in 0..current.length() {
            if current.get_return_state(i) == PREDICTION_CONTEXT_EMPTY_RETURN_STATE {
                continue;
            }

            let parent = current.get_parent(i).unwrap();
            if visited.insert(parent.deref(), parent.clone()).is_none() {
                context_ids.insert(parent.deref(), context_ids.len());
                work_list.push_back(parent.clone());
            }

            edges.extend(format!("  s{}", context_ids.get(&current_ptr).unwrap()).chars());
            if current.length() > 1 {
                edges += ":p";
                edges += &i.to_string();
            }

            edges += &format!(
                "->s{}[label=\"{}\"];\n",
                context_ids
                    .get(&(current.get_parent(i).unwrap().deref() as *const PredictionContext))
                    .unwrap(),
                current.get_return_state(i)
            );
        }
    }

    format!("digraph G {{\nrankdir=LR;\n{}{}}}\n", nodes, edges)
}
