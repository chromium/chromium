//! StatsD monitor.
//!
//! This roughly corresponds to the [AFL++'s rpc_statsd](https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/rpc_statsd.md),
//! so you could view such documentation for detailed information.
//!
//! StatsD monitor is useful when you have multiple fuzzing instances, and this monitor
//! could help visualizing the aggregated fuzzing statistics with serveral third-party
//! statsd-related tools.

// Use this since clippy thinks we should use `StatsD` instead of StatsD.
#![allow(clippy::doc_markdown)]

use alloc::{borrow::Cow, string::String, vec::Vec};
use std::net::UdpSocket;

use cadence::{BufferedUdpMetricSink, Gauged, QueuingMetricSink, StatsdClient};
use libafl_bolts::{ClientId, Error};

use super::{
    Monitor,
    stats::{ClientStatsManager, EdgeCoverage, ItemGeometry, manager::GlobalStats},
};

const METRIC_PREFIX: &str = "fuzzing";

/// Flavor of StatsD tag
#[derive(Debug)]
pub enum StatsdMonitorTagFlavor {
    /// [Datadog](https://docs.datadoghq.com/developers/dogstatsd/) style tag
    DogStatsd {
        /// Identifier to distinguish this fuzzing instance with others.
        tag_identifier: Cow<'static, str>,
        /// Other custom tags (key, value) pairs.
        ///
        /// Key should not be one of "afl_version", "banner", "instance", "job"
        /// and "type", which are reserved for internal usage.
        custom_tags: Vec<(Cow<'static, str>, Cow<'static, str>)>,
    },
    /// No tag
    None,
}

impl Default for StatsdMonitorTagFlavor {
    fn default() -> Self {
        Self::DogStatsd {
            tag_identifier: "default".into(),
            custom_tags: vec![],
        }
    }
}

/// StatsD monitor
#[derive(Debug)]
pub struct StatsdMonitor {
    target_host: String,
    target_port: u16,
    tag_flavor: StatsdMonitorTagFlavor,
    statsd_client: Option<StatsdClient>,
}

impl StatsdMonitor {
    /// Create a new StatsD monitor, which sends metrics to server
    /// specified by `target_host` and `target_port` via UDP.
    ///
    /// If that server is down, this monitor will just do nothing and will
    /// not crash or throw, so use this freely. :)
    #[must_use]
    pub fn new(target_host: String, target_port: u16, tag_flavor: StatsdMonitorTagFlavor) -> Self {
        let mut this = Self {
            target_host,
            target_port,
            tag_flavor,
            statsd_client: None,
        };
        this.setup_statsd_client();
        this
    }

    // Call this method if self.statsd_client is None.
    fn setup_statsd_client(&mut self) {
        // This code follows https://docs.rs/cadence/latest/cadence/#queuing-asynchronous-metric-sink,
        // which is the preferred way to use Cadence in production.
        //
        // For anyone maintaining this module, please carefully read that section.

        // This bind would never fail, or something extermely unexpected happened
        let socket = UdpSocket::bind("0.0.0.0:0").unwrap();
        // This set config would never fail, or something extermely unexpected happened
        socket.set_nonblocking(true).unwrap();

        let Ok(udp_sink) =
            BufferedUdpMetricSink::from((self.target_host.as_str(), self.target_port), socket)
        else {
            log::warn!(
                "Statsd monitor failed to connect target host {}:{}",
                self.target_host,
                self.target_port
            );
            return;
        };
        let queuing_sink = QueuingMetricSink::builder()
            .with_error_handler(|e| {
                log::warn!("Statsd monitor failed to send to target host: {e:?}");
            })
            .build(udp_sink);
        let mut client_builder = StatsdClient::builder(METRIC_PREFIX, queuing_sink);
        if let StatsdMonitorTagFlavor::DogStatsd {
            tag_identifier,
            custom_tags,
        } = &self.tag_flavor
        {
            client_builder = client_builder
                .with_tag("banner", tag_identifier.as_ref())
                .with_tag("afl_version", env!("CARGO_PKG_VERSION"));
            for (tag_key, tag_value) in custom_tags {
                client_builder = client_builder.with_tag(tag_key.as_ref(), tag_value.as_ref());
            }
        }
        let client = client_builder.build();
        self.statsd_client = Some(client);
    }

    #[expect(clippy::cast_precision_loss)]
    fn try_display(&mut self, client_stats_manager: &mut ClientStatsManager) -> Option<()> {
        if self.statsd_client.is_none() {
            self.setup_statsd_client();
        }

        let Some(statsd_client) = &mut self.statsd_client else {
            // The client still cannot be built. Then we do nothing.
            return Some(());
        };

        let GlobalStats {
            total_execs,
            execs_per_sec,
            corpus_size,
            objective_size,
            ..
        } = client_stats_manager.global_stats();
        let total_execs = *total_execs;
        let execs_per_sec = *execs_per_sec;
        let corpus_size = *corpus_size;
        let objective_size = *objective_size;
        let ItemGeometry {
            pending,
            pend_fav,
            own_finds,
            imported,
            stability,
        } = client_stats_manager.item_geometry();
        let edges_coverage = client_stats_manager.edges_coverage();

        // In the following codes, we immediate throw if the statsd client failed
        // to send metrics. The caller should clear the statsd client when error occurred.
        //
        // The error generated by sending metrics will be handled by the error handler
        // registered when creating queuing_sink.
        //
        // The following metrics are taken from AFLplusplus/src/afl-fuzz-statsd.c
        // Metrics followed by "Newly added" mean they are not in AFL++.

        statsd_client.gauge("execs_done", total_execs).ok()?;
        statsd_client.gauge("execs_per_sec", execs_per_sec).ok()?;
        statsd_client.gauge("corpus_count", corpus_size).ok()?;
        statsd_client.gauge("corpus_found", own_finds).ok()?;
        statsd_client.gauge("corpus_imported", imported).ok()?;
        if let Some(stability) = stability {
            statsd_client.gauge("stability", stability).ok()?; // Newly added
        }
        statsd_client.gauge("pending_favs", pend_fav).ok()?;
        statsd_client.gauge("pending_total", pending).ok()?;
        statsd_client
            .gauge("saved_solutions", objective_size)
            .ok()?; // Newly added
        if let Some(EdgeCoverage {
            edges_hit,
            edges_total,
        }) = edges_coverage
        {
            statsd_client.gauge("edges_found", edges_hit).ok()?;
            statsd_client
                .gauge("map_density", (edges_hit as f64) / (edges_total as f64))
                .ok()?; // Newly added
        }

        Some(())
    }
}

impl Monitor for StatsdMonitor {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        _event_msg: &str,
        _sender_id: ClientId,
    ) -> Result<(), Error> {
        if self.try_display(client_stats_manager).is_none() {
            // The client failed to send metrics, which means the server is down
            // or something else happened. We then de-initialize the client, and
            // when the `display` is called next time, it will be re-initialized
            // and try to connect the server then.
            self.statsd_client = None;
        }
        Ok(())
    }
}
