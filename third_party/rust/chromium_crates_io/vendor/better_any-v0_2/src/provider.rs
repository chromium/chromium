use crate::Tid;


pub trait Provider<'x>: Tid<'x> {
    fn provide<'a>(&self, client: &mut Demand<'x>);
    fn provide_mut<'a>(&'a mut self, client: &mut Demand<'a, 'x>);
}

struct TypedOption<'a, 'x, T:Tid<'x>>(Option<T>);

trait Demander<'a,'x>{
    fn requests_for(&mut self,)
}


pub struct Demand<'a: 'x, 'x>(dyn Demander<'a, 'x> + 'a);
