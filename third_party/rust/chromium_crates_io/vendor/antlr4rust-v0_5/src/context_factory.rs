use std::cell::{Ref, RefCell, RefMut};
use std::marker::PhantomData;
use std::ops::{CoerceUnsized, Deref, DerefMut};
use std::rc::Rc;

use better_any::TidExt;
use qcell::{TLCell, TLCellOwner};
use typed_arena::Arena;

use crate::parser_rule_context::ParserRuleContext;

trait ContextFactory<'a, T: ?Sized> {
    type CtxRef;
    type Ref: Deref<Target = T> + 'a;
    type RefMut: DerefMut<Target = T> + 'a;

    fn new(&mut self, inner: T) -> Self::CtxRef
    where
        T: Sized;

    fn borrow(&'a self, this: &'a Self::CtxRef) -> Self::Ref;
    fn borrow_mut(&'a mut self, this: &'a mut Self::CtxRef) -> Self::RefMut;
}

struct RcFactory;

impl<'a, T: 'a + ?Sized> ContextFactory<'a, T> for RcFactory {
    type CtxRef = Rc<T>;
    type Ref = &'a T;
    type RefMut = &'a mut T;

    fn new(&mut self, inner: T) -> Self::CtxRef
    where
        T: Sized,
    {
        Rc::new(inner)
    }

    fn borrow(&'a self, this: &'a Self::CtxRef) -> Self::Ref { &*this }

    fn borrow_mut(&'a mut self, this: &'a mut Self::CtxRef) -> Self::RefMut {
        unsafe { Rc::get_mut_unchecked(this) }
    }
}

struct RefCellFactory<T: ?Sized> {
    arena: Arena<Box<RefCell<T>>>,
}

impl<'a, 'this, T, Dyn> ContextFactory<'a, T> for &'this RefCellFactory<Dyn>
where
    T: 'this + 'a + CoerceUnsized<Dyn> + ?Sized,
    Dyn: 'this + 'a + ?Sized,
    Box<RefCell<T>>: CoerceUnsized<Box<RefCell<Dyn>>>,
{
    type CtxRef = &'this RefCell<T>;
    type Ref = Ref<'a, T>;
    type RefMut = RefMut<'a, T>;

    fn new(&mut self, inner: T) -> Self::CtxRef
    where
        T: Sized,
    {
        let val = Box::new(RefCell::new(inner)) as Box<RefCell<Dyn>>;
        let res = self.arena.alloc(val).as_mut();
        unsafe { &*(res as *mut RefCell<Dyn> as *mut RefCell<T>) }
    }

    fn borrow(&'a self, this: &'a Self::CtxRef) -> Self::Ref { RefCell::borrow(this) }

    fn borrow_mut(&'a mut self, this: &'a mut Self::CtxRef) -> Self::RefMut {
        RefCell::borrow_mut(this)
    }
}

/// index that saves type info to downcast back without checks
struct Id<T: ?Sized> {
    idx: usize,
    phantom: PhantomData<Box<T>>,
}

struct IdFactory<T: ?Sized> {
    arena: Vec<Box<T>>,
}

impl<'a, T, Dyn> ContextFactory<'a, T> for IdFactory<Dyn>
where
    T: 'a + CoerceUnsized<Dyn> + ?Sized,
    Dyn: 'a + ?Sized,
    Box<T>: CoerceUnsized<Box<Dyn>>,
{
    type CtxRef = Id<T>;
    type Ref = &'a T;
    type RefMut = &'a mut T;

    fn new(&mut self, inner: T) -> Self::CtxRef
    where
        T: Sized,
    {
        let b = Box::new(inner);
        self.arena.push(b as _);
        Id {
            idx: self.arena.len() - 1,
            phantom: Default::default(),
        }
    }

    fn borrow(&'a self, this: &'a Self::CtxRef) -> Self::Ref {
        let this = &*self.arena[this.idx];
        // safe because we know that T:CoerceUnsized<Dyn>
        unsafe { std::mem::transmute_copy::<&Dyn, &T>(&this) }
    }

    fn borrow_mut(&'a mut self, this: &'a mut Self::CtxRef) -> Self::RefMut {
        let this = &mut *self.arena[this.idx];
        unsafe { std::mem::transmute_copy::<&mut Dyn, &mut T>(&this) }
    }
}

struct Owner;

struct QCellArena<Dyn: ?Sized> {
    arena: Arena<Box<TLCell<Owner, Dyn>>>,
    guard: TLCellOwner<Owner>,
}

impl<'a, 'this, T, Dyn> ContextFactory<'a, T> for &'this mut QCellArena<Dyn>
where
    T: 'a + 'this + CoerceUnsized<Dyn> + ?Sized,
    Dyn: 'a + 'this + ?Sized,
    Box<TLCell<Owner, T>>: CoerceUnsized<Box<TLCell<Owner, Dyn>>>,
{
    type CtxRef = &'this TLCell<Owner, T>;
    type Ref = &'a T;
    type RefMut = &'a mut T;

    fn new(&mut self, inner: T) -> Self::CtxRef
    where
        T: Sized,
    {
        let t = Box::new(self.guard.cell(inner));
        let r = &**self.arena.alloc(t);
        unsafe { &*(r as *const _ as *const _) }
    }

    fn borrow(&'a self, this: &'a Self::CtxRef) -> Self::Ref { self.guard.ro(this) }

    fn borrow_mut(&'a mut self, this: &'a mut Self::CtxRef) -> Self::RefMut { self.guard.rw(this) }
}

trait Cast<T> {
    type WrappedT: CoerceUnsized<Self::WrappedSelf>;
    type WrappedSelf;
    fn downcast(this: Self::WrappedSelf) -> Self::WrappedT;
}

impl<'i, T, Y: ?Sized> Cast<T> for Y
where
    Y: ParserRuleContext<'i>,
    T: ParserRuleContext<'i>,
    Rc<T>: CoerceUnsized<Rc<Y>>,
{
    type WrappedT = Rc<T>;
    type WrappedSelf = Rc<Y>;

    fn downcast(this: Self::WrappedSelf) -> Self::WrappedT { this.downcast_rc().unwrap() }
}

//
// trait Downcast<'i, Owner>: CoerceUnsized<Self::DynRef> {
//     type Inner: ParserRuleContext<'i, Ctx = Self::Dyn> + ?Sized;
//     type DynRef;
//     type Dyn;
//     fn downcast<'x>(from: Self::DynRef, owner: Owner) -> Option<Self>;
// }
//
// impl<'i, T: ParserRuleContext<'i> + ?Sized, Owner, U> Downcast<Owner> for Rc<T>
// where
//     Rc<T>: CoerceUnsized<Rc<T::Ctx>>,
// {
//     type Inner = T;
//     type DynRef = Rc<Self::Dyn>;
//     type Dyn = T::Ctx;
//
//     fn downcast<'x>(from: Rc<T::Ctx>, owner: Owner) -> Option<Self> {
//         if from.self_id() == T::id() {
//             Some(unsafe { Rc::from_raw(Rc::into_raw(from) as *const _) })
//         } else {
//             None
//         }
//     }
// }
//
// impl<'i, T: ParserRuleContext<'i> + ?Sized, Owner, U> Downcast<Owner> for &RefCell<T>
// where
//     Rc<T>: CoerceUnsized<Rc<T::Ctx>>,
// {
//     type Inner = T;
//     type DynRef = Rc<Self::Dyn>;
//     type Dyn = T::Ctx;
//
//     fn downcast<'x>(from: Self::DynRef, owner: Owner) -> Option<Self> { unimplemented!() }
// }
