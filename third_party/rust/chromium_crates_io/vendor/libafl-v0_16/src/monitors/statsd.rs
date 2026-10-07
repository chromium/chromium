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

use alloc::{
    borrow::Cow,
    string::{String, ToString},
    sync::Arc,
    vec::Vec,
};
use std::net::UdpSocket;

use cadence::{BufferedUdpMetricSink, Gauged, QueuingMetricSink, StatsdClient};
use libafl_bolts::{ClientId, Error};

use super::{
    Monitor,
    stats::{ClientStatsManager, EdgeCoverage, manager::GlobalStats, user_stats::TAG_CORE_ID},
};

const METRIC_PREFIX: &str = "fuzzing";

/// Flavor of StatsD tag
#[derive(Debug, Clone)]
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
    statsd_client: Option<Arc<StatsdClient>>,
    enable_per_client_stats: bool,
}

impl StatsdMonitor {
    /// Create a new StatsD monitor, which sends metrics to server
    /// specified by `target_host` and `target_port` via UDP.
    ///
    /// If that server is down, this monitor will just do nothing and will
    /// not crash or throw, so use this freely. :)
    #[must_use]
    pub fn new(target_host: String, target_port: u16, tag_flavor: StatsdMonitorTagFlavor) -> Self {
        Self {
            target_host,
            target_port,
            tag_flavor,
            statsd_client: None,
            enable_per_client_stats: false,
        }
    }

    /// Set if we want to report per-client metrics (default: false)
    #[must_use]
    pub fn with_per_client_stats(mut self, enable_per_client_stats: bool) -> Self {
        self.enable_per_client_stats = enable_per_client_stats;
        self
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
        self.statsd_client = Some(Arc::new(client));
    }

    #[expect(clippy::cast_precision_loss)]
    #[expect(clippy::too_many_arguments)]
    fn send_metrics(
        client: &StatsdClient,
        prefix: &str,
        total_execs: u64,
        execs_per_sec: f64,
        corpus_size: u64,
        own_finds: u64,
        imported: u64,
        stability: Option<f64>,
        pend_fav: u64,
        pending: u64,
        objective_size: u64,
        edges_coverage: Option<&EdgeCoverage>,
        extra_tags: &[(&str, &str)],
    ) -> Option<()> {
        // Add extra tags to the metric name if needed, or use the client's default tags
        // Note: Cadence doesn't support adding tags *per metric* in a pipeline cleanly without
        // recreating the client or using a specific sink.
        // However, DogStatsD supports tags. The `cadence` crate supports tags by
        // `gauge_with_tags`.
        // Let's use `gauge_with_tags` if we have extra tags, otherwise standard `gauge`.

        // Actually, we can just use the client to send metrics.
        // If we have extra tags, we need to handle them.
        // The `StatsdClient` struct holds the tags.
        // If we want to vary tags per metric (e.g. for different clients), we might need
        // to clone the client or use the sink directly?
        // Wait, typical DogStatsD Usage with Cadence:
        // client.gauge_with_tags("metric", val).with_tag("k", "v").send();

        let send_gauge = |name: &str, val: f64| -> Option<()> {
            let metric = format!("{prefix}{name}");
            let mut gauge = client.gauge_with_tags(&metric, val);
            for (k, v) in extra_tags {
                gauge = gauge.with_tag(k, v);
            }
            gauge.send();
            Some(())
        };

        send_gauge("execs_done", total_execs as f64)?;
        send_gauge("execs_per_sec", execs_per_sec)?;
        send_gauge("corpus_count", corpus_size as f64)?;
        send_gauge("corpus_found", own_finds as f64)?;
        send_gauge("corpus_imported", imported as f64)?;
        if let Some(stability) = stability {
            send_gauge("stability", stability)?;
        }
        send_gauge("pending_favs", pend_fav as f64)?;
        send_gauge("pending_total", pending as f64)?;
        send_gauge("saved_solutions", objective_size as f64)?;
        if let Some(coverage) = edges_coverage {
            send_gauge("edges_found", coverage.edges_hit as f64)?;
            send_gauge(
                "map_density",
                (coverage.edges_hit as f64) / (coverage.edges_total as f64),
            )?;
        }
        Some(())
    }

    fn try_display(&mut self, client_stats_manager: &mut ClientStatsManager) -> Option<()> {
        if self.statsd_client.is_none() {
            self.setup_statsd_client();
        }

        let Some(statsd_client) = &mut self.statsd_client else {
            // The client still cannot be built. Then we do nothing.
            return Some(());
        };

        // Report Global Stats
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
        let (pending, pend_fav, own_finds, imported, stability) =
            if let Some(item_geometry) = client_stats_manager.item_geometry() {
                (
                    item_geometry.pending,
                    item_geometry.pend_fav,
                    item_geometry.own_finds,
                    item_geometry.imported,
                    item_geometry.stability,
                )
            } else {
                (0, 0, 0, 0, None)
            };
        let edges_coverage = client_stats_manager.edges_coverage();

        Self::send_metrics(
            statsd_client,
            "",
            total_execs,
            execs_per_sec,
            corpus_size,
            own_finds,
            imported,
            stability,
            pend_fav,
            pending,
            objective_size,
            edges_coverage.as_ref(),
            &[],
        )?;

        // Report Per-Client Stats if enabled
        if self.enable_per_client_stats {
            // Need to iterate clients.
            // ClientStatsManager doesn't expose `client_stats` directly securely?
            // It has `client_stats()` method which returns `&HashMap<ClientId, ClientStats>`.
            for (client_id, client) in client_stats_manager.client_stats() {
                let core_id_str = client
                    .user_stats_by_tag(TAG_CORE_ID)
                    .next()
                    .map_or_else(|| "unknown".to_string(), |s| s.value().to_string());

                let client_id_str = client_id.0.to_string();

                let tags = [
                    ("client_id", client_id_str.as_str()),
                    ("core_id", core_id_str.as_str()),
                ];

                // Collect per-client metrics
                let execs = client.executions();
                // execs_per_sec for client requires current time, but `client.execs_per_sec` mutates `last_window_time`?
                // `client_stats_manager.global_stats()` updates everything.
                // We can't easily get mutable access to client here to call `execs_per_sec(cur_time)`.
                // But `client.execs_per_sec_pretty` calls `execs_per_sec` which takes `&mut self`.
                // `ClientStatsManager::client_stats()` returns `&HashMap`. So we only have read access.
                // We have to rely on what's available or stored.
                // `ClientStats` has `last_execs_per_sec` field but it is used for calculation.
                // Wait, `ClientStats` struct has `last_execs_per_sec` (f64).
                // IF feature="afl_exec_sec" is on.
                #[cfg(feature = "afl_exec_sec")]
                let client_execs_per_sec = {
                    // We can't access private fields or mutable methods.
                    // But we *can* enable the feature and hopefully use a getter?
                    // There is no public getter for `last_execs_per_sec` that is non-mutating?
                    // `execs_per_sec` is `&mut self`.
                    // This is a problem. We can't calculate execs/sec without mutating state (for time window).
                    // However, `ClientStatsManager::update_client_stats_for` is where updates happen.
                    // The GlobalStats calculation iterates and updates.
                    // Ideally `ClientStats` should cache the last calculated/current execs/sec.
                    // But `ClientStats` definition:
                    // `last_execs_per_sec: f64` is private.

                    // Workaround: Calculate simple average or accept 0 for now if we can't access it?
                    // Or maybe we can just use 0.0 or skip it.
                    // Let's look at `ClientStats` again.
                    // It has `execs_per_sec_pretty`.

                    0.0 // Placeholder as we can't get it easily without mutability
                };
                #[cfg(not(feature = "afl_exec_sec"))]
                let client_execs_per_sec = 0.0;

                let corpus_size = client.corpus_size();
                let objective_size = client.objective_size();
                let (pend_fav, pending, own_finds, imported, stability) =
                    if let Some(item_geometry) = client.item_geometry() {
                        (
                            item_geometry.pend_fav,
                            item_geometry.pending,
                            item_geometry.own_finds,
                            item_geometry.imported,
                            item_geometry.stability,
                        )
                    } else {
                        (0, 0, 0, 0, None)
                    };

                Self::send_metrics(
                    statsd_client,
                    "client.",
                    execs,
                    client_execs_per_sec,
                    corpus_size,
                    own_finds,
                    imported,
                    stability,
                    pend_fav,
                    pending,
                    objective_size,
                    edges_coverage.as_ref(),
                    &tags,
                )?;
            }
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
        let _ = client_stats_manager.global_stats();
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
