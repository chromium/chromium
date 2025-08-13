use crate::syntax::{FnKind, Receiver, Signature};
use proc_macro2::Ident;

impl Signature {
    pub fn receiver(&self) -> Option<&Receiver> {
        match &self.kind {
            FnKind::Method(receiver) => Some(receiver),
            FnKind::Assoc(_) | FnKind::Free => None,
        }
    }

    pub fn receiver_mut(&mut self) -> Option<&mut Receiver> {
        match &mut self.kind {
            FnKind::Method(receiver) => Some(receiver),
            FnKind::Assoc(_) | FnKind::Free => None,
        }
    }

    pub fn self_type(&self) -> Option<&Ident> {
        match &self.kind {
            FnKind::Method(receiver) => Some(&receiver.ty.rust),
            FnKind::Assoc(self_type) => Some(self_type),
            FnKind::Free => None,
        }
    }
}
