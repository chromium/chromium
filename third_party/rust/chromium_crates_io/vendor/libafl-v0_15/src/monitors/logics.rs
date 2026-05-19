//! Monitor wrappers that add logics to monitor

use libafl_bolts::{ClientId, Error};

use crate::monitors::{Monitor, stats::ClientStatsManager};

/// The wrapped monitor will keep displaying until the closure evaluates to false
#[derive(Debug)]
pub struct WhileMonitor<CB, M> {
    closure: CB,
    monitor: M,
}

impl<CB, M> Monitor for WhileMonitor<CB, M>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M: Monitor,
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        while (self.closure)(client_stats_manager, event_msg, sender_id) {
            self.monitor
                .display(client_stats_manager, event_msg, sender_id)?;
        }
        Ok(())
    }
}

impl<CB, M> WhileMonitor<CB, M>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M: Monitor,
{
    /// Create a new [`WhileMonitor`].
    ///
    /// The `closure` will be evaluated at each `display` call
    #[must_use]
    pub fn new(closure: CB, monitor: M) -> Self {
        Self { closure, monitor }
    }
}

/// The wrapped monitor will display if the closure evaluates to true
#[derive(Debug)]
pub struct IfMonitor<CB, M> {
    closure: CB,
    monitor: M,
}

impl<CB, M> Monitor for IfMonitor<CB, M>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M: Monitor,
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        if (self.closure)(client_stats_manager, event_msg, sender_id) {
            self.monitor
                .display(client_stats_manager, event_msg, sender_id)?;
        }
        Ok(())
    }
}

impl<CB, M> IfMonitor<CB, M>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M: Monitor,
{
    /// Create a new [`IfMonitor`].
    ///
    /// The `closure` will be evaluated at each `display` call
    #[must_use]
    pub fn new(closure: CB, monitor: M) -> Self {
        Self { closure, monitor }
    }
}

/// Either wrapped monitors will display based on the closure evaluation
#[derive(Debug)]
pub struct IfElseMonitor<CB, M1, M2> {
    closure: CB,
    if_monitor: M1,
    else_monitor: M2,
}

impl<CB, M1, M2> Monitor for IfElseMonitor<CB, M1, M2>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M1: Monitor,
    M2: Monitor,
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        if (self.closure)(client_stats_manager, event_msg, sender_id) {
            self.if_monitor
                .display(client_stats_manager, event_msg, sender_id)?;
        } else {
            self.else_monitor
                .display(client_stats_manager, event_msg, sender_id)?;
        }
        Ok(())
    }
}

impl<CB, M1, M2> IfElseMonitor<CB, M1, M2>
where
    CB: FnMut(&mut ClientStatsManager, &str, ClientId) -> bool,
    M1: Monitor,
    M2: Monitor,
{
    /// Create a new [`IfElseMonitor`].
    ///
    /// The `closure` will be evaluated at each `display` call
    #[must_use]
    pub fn new(closure: CB, if_monitor: M1, else_monitor: M2) -> Self {
        Self {
            closure,
            if_monitor,
            else_monitor,
        }
    }
}

/// A monitor wrapper where the monitor does not need to be initialized, but can be [`None`].
#[derive(Debug)]
pub struct OptionalMonitor<M> {
    monitor: Option<M>,
}

impl<M> Monitor for OptionalMonitor<M>
where
    M: Monitor,
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        if let Some(monitor) = self.monitor.as_mut() {
            monitor.display(client_stats_manager, event_msg, sender_id)?;
        }
        Ok(())
    }
}

impl<M> OptionalMonitor<M>
where
    M: Monitor,
{
    /// Create a new [`OptionalMonitor`]
    #[must_use]
    pub fn new(monitor: Option<M>) -> Self {
        Self { monitor }
    }

    /// Create a new [`OptionalMonitor`] with a monitor
    #[must_use]
    pub fn some(monitor: M) -> Self {
        Self {
            monitor: Some(monitor),
        }
    }

    /// Create a new [`OptionalMonitor`] without a monitor
    #[must_use]
    pub fn none() -> Self {
        Self { monitor: None }
    }
}
