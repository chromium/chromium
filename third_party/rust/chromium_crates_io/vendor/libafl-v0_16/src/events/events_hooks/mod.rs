//! Hooks for event managers, especifically these are used to hook before `try_receive`.
//!
//! This will allow user to define pre/post-processing code when the event manager receives any message from
//! other clients
use libafl_bolts::ClientId;

use crate::{Error, events::EventWithStats};

/// The `broker_hooks` that are run before and after the event manager calls `try_receive`
pub trait EventManagerHook<I, S> {
    /// The hook that runs before `try_receive`
    /// Return false if you want to cancel the subsequent event handling
    fn pre_receive(
        &mut self,
        state: &mut S,
        client_id: ClientId,
        event: &EventWithStats<I>,
    ) -> Result<bool, Error>;
}

/// The tuples contains `broker_hooks` to be executed for `try_receive`
pub trait EventManagerHooksTuple<I, S> {
    /// The hook that runs before `try_receive`
    fn pre_receive_all(
        &mut self,
        state: &mut S,
        client_id: ClientId,
        event: &EventWithStats<I>,
    ) -> Result<bool, Error>;
}

impl<I, S> EventManagerHooksTuple<I, S> for () {
    /// The hook that runs before `try_receive`
    fn pre_receive_all(
        &mut self,
        _state: &mut S,
        _client_id: ClientId,
        _event: &EventWithStats<I>,
    ) -> Result<bool, Error> {
        Ok(true)
    }
}

impl<Head, Tail, I, S> EventManagerHooksTuple<I, S> for (Head, Tail)
where
    Head: EventManagerHook<I, S>,
    Tail: EventManagerHooksTuple<I, S>,
{
    /// The hook that runs before `try_receive`
    fn pre_receive_all(
        &mut self,
        state: &mut S,
        client_id: ClientId,
        event: &EventWithStats<I>,
    ) -> Result<bool, Error> {
        let first = self.0.pre_receive(state, client_id, event)?;
        let second = self.1.pre_receive_all(state, client_id, event)?;
        Ok(first & second)
    }
}
