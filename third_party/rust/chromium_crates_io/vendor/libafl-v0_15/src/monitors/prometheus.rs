//! The [`PrometheusMonitor`] logs fuzzer progress to a prometheus endpoint.
//!
//! ## Overview
//!
//! The client (i.e., the fuzzer) sets up an HTTP endpoint (/metrics).
//! The endpoint contains metrics such as execution rate.
//!
//! A prometheus server (can use a precompiled binary or docker) then scrapes
//! the endpoint at regular intervals (configurable via prometheus.yml file).
//!
//! ## How to use it
//!
//! Create a [`PrometheusMonitor`] and plug it into any fuzzer similar to other monitors.
//! In your fuzzer:
//!
//! ```rust
//! // First, include:
//! use libafl::monitors::PrometheusMonitor;
//!
//! // Then, create the monitor:
//! let listener = "127.0.0.1:8080".to_string(); // point prometheus to scrape here in your prometheus.yml
//! let mon = PrometheusMonitor::new(listener, |s| log::info!("{s}"));
//!
//! // and finally, like with any other monitor, pass it into the event manager like so:
//! // let mgr = SimpleEventManager::new(mon);
//! ```
//!
//! When using docker, you may need to point `prometheus.yml` to the `docker0` interface or `host.docker.internal`

use alloc::{
    borrow::Cow,
    string::{String, ToString},
    sync::Arc,
};
use core::{
    fmt,
    fmt::{Debug, Write},
    sync::atomic::AtomicU64,
    time::Duration,
};
use std::thread;

// using thread in order to start the HTTP server in a separate thread
use futures::executor::block_on;
use libafl_bolts::{ClientId, Error, current_time};
// using the official rust client library for Prometheus: https://github.com/prometheus/client_rust
use prometheus_client::{
    encoding::{EncodeLabelSet, text::encode},
    metrics::{family::Family, gauge::Gauge},
    registry::Registry,
};
// using tide for the HTTP server library (fast, async, simple)
use tide::Request;

use crate::monitors::{
    Monitor,
    stats::{manager::ClientStatsManager, user_stats::UserStatsValue},
};

/// Prometheus metrics for global and each client.
#[derive(Debug, Clone, Default)]
pub struct PrometheusStats {
    corpus_count: Family<Labels, Gauge>,
    objective_count: Family<Labels, Gauge>,
    executions: Family<Labels, Gauge>,
    exec_rate: Family<Labels, Gauge<f64, AtomicU64>>,
    runtime: Family<Labels, Gauge>,
    clients_count: Family<Labels, Gauge>,
    custom_stat: Family<Labels, Gauge<f64, AtomicU64>>,
}

/// Tracking monitor during fuzzing.
#[derive(Clone)]
pub struct PrometheusMonitor<F>
where
    F: FnMut(&str),
{
    print_fn: F,
    prometheus_global_stats: PrometheusStats, // global prometheus metrics
    prometheus_client_stats: PrometheusStats, // per-client prometheus metrics
}

impl<F> Debug for PrometheusMonitor<F>
where
    F: FnMut(&str),
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PrometheusMonitor").finish_non_exhaustive()
    }
}

impl<F> Monitor for PrometheusMonitor<F>
where
    F: FnMut(&str),
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        // Update the prometheus metrics
        // The gauges must take signed i64's, with max value of 2^63-1 so it is
        // probably fair to error out at a count of nine quintillion across any
        // of these counts.
        // realistically many of these metrics should be counters but would
        // require a fair bit of logic to handle "amount to increment given
        // time since last observation"

        let global_stats = client_stats_manager.global_stats();
        // Global (aggregated) metrics
        let corpus_size = global_stats.corpus_size;
        self.prometheus_global_stats
            .corpus_count
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(corpus_size.try_into().unwrap());

        let objective_size = global_stats.objective_size;
        self.prometheus_global_stats
            .objective_count
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(objective_size.try_into().unwrap());

        let total_execs = global_stats.total_execs;
        self.prometheus_global_stats
            .executions
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(total_execs.try_into().unwrap());

        let execs_per_sec = global_stats.execs_per_sec;
        self.prometheus_global_stats
            .exec_rate
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(execs_per_sec);

        let run_time = global_stats.run_time.as_secs();
        self.prometheus_global_stats
            .runtime
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(run_time.try_into().unwrap()); // run time in seconds, which can be converted to a time format by Grafana or similar

        let total_clients = global_stats.client_stats_count.try_into().unwrap(); // convert usize to u64 (unlikely that # of clients will be > 2^64 -1...)
        self.prometheus_global_stats
            .clients_count
            .get_or_create(&Labels {
                client: Cow::from("global"),
                stat: Cow::from(""),
            })
            .set(total_clients);

        // display stats in a SimpleMonitor format
        let mut global_fmt = format!(
            "[Prometheus] [{} #GLOBAL] run time: {}, clients: {}, corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            event_msg,
            global_stats.run_time_pretty,
            global_stats.client_stats_count,
            global_stats.corpus_size,
            global_stats.objective_size,
            global_stats.total_execs,
            global_stats.execs_per_sec_pretty
        );
        for (key, val) in client_stats_manager.aggregated() {
            // print global aggregated custom stats
            write!(global_fmt, ", {key}: {val}").unwrap();
            #[expect(clippy::cast_precision_loss)]
            let value: f64 = match val {
                UserStatsValue::Number(n) => *n as f64,
                UserStatsValue::Float(f) => *f,
                UserStatsValue::String(_s) => 0.0,
                UserStatsValue::Ratio(a, b) => {
                    if key == "edges" {
                        self.prometheus_global_stats
                            .custom_stat
                            .get_or_create(&Labels {
                                client: Cow::from("global"),
                                stat: Cow::from("edges_total"),
                            })
                            .set(*b as f64);
                        self.prometheus_global_stats
                            .custom_stat
                            .get_or_create(&Labels {
                                client: Cow::from("global"),
                                stat: Cow::from("edges_hit"),
                            })
                            .set(*a as f64);
                    }
                    (*a as f64 / *b as f64) * 100.0
                }
                UserStatsValue::Percent(p) => *p * 100.0,
            };
            self.prometheus_global_stats
                .custom_stat
                .get_or_create(&Labels {
                    client: Cow::from("global"),
                    stat: key.clone(),
                })
                .set(value);
        }

        (self.print_fn)(&global_fmt);

        // Client-specific metrics

        client_stats_manager.client_stats_insert(sender_id)?;
        let client = client_stats_manager.client_stats_for(sender_id)?;
        let mut cur_client_clone = client.clone();

        self.prometheus_client_stats
            .corpus_count
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(cur_client_clone.corpus_size().try_into().unwrap());

        self.prometheus_client_stats
            .objective_count
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(cur_client_clone.objective_size().try_into().unwrap());

        self.prometheus_client_stats
            .executions
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(cur_client_clone.executions().try_into().unwrap());

        self.prometheus_client_stats
            .exec_rate
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(cur_client_clone.execs_per_sec(current_time()));

        let client_run_time = current_time()
            .checked_sub(cur_client_clone.start_time())
            .unwrap_or_default()
            .as_secs();
        self.prometheus_client_stats
            .runtime
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(client_run_time.try_into().unwrap()); // run time in seconds per-client, which can be converted to a time format by Grafana or similar

        self.prometheus_global_stats
            .clients_count
            .get_or_create(&Labels {
                client: Cow::from(sender_id.0.to_string()),
                stat: Cow::from(""),
            })
            .set(total_clients);

        let mut fmt = format!(
            "[Prometheus] [{} #{}] corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            event_msg,
            sender_id.0,
            client.corpus_size(),
            client.objective_size(),
            client.executions(),
            cur_client_clone.execs_per_sec_pretty(current_time())
        );

        for (key, val) in cur_client_clone.user_stats() {
            // print the custom stats for each client
            write!(fmt, ", {key}: {val}").unwrap();
            // Update metrics added to the user_stats hashmap by feedback event-fires
            // You can filter for each custom stat in promQL via labels of both the stat name and client id
            #[expect(clippy::cast_precision_loss)]
            let value: f64 = match val.value() {
                UserStatsValue::Number(n) => *n as f64,
                UserStatsValue::Float(f) => *f,
                UserStatsValue::String(_s) => 0.0,
                UserStatsValue::Ratio(a, b) => {
                    if key == "edges" {
                        self.prometheus_client_stats
                            .custom_stat
                            .get_or_create(&Labels {
                                client: Cow::from(sender_id.0.to_string()),
                                stat: Cow::from("edges_total"),
                            })
                            .set(*b as f64);
                        self.prometheus_client_stats
                            .custom_stat
                            .get_or_create(&Labels {
                                client: Cow::from(sender_id.0.to_string()),
                                stat: Cow::from("edges_hit"),
                            })
                            .set(*a as f64);
                    }
                    (*a as f64 / *b as f64) * 100.0
                }
                UserStatsValue::Percent(p) => *p * 100.0,
            };
            self.prometheus_client_stats
                .custom_stat
                .get_or_create(&Labels {
                    client: Cow::from(sender_id.0.to_string()),
                    stat: key.clone(),
                })
                .set(value);
        }
        (self.print_fn)(&fmt);
        Ok(())
    }
}

impl<F> PrometheusMonitor<F>
where
    F: FnMut(&str),
{
    /// Create a new [`PrometheusMonitor`].
    /// The `listener` is the address to send logs to.
    /// The `print_fn` is the printing function that can output the logs otherwise.
    pub fn new(listener: String, print_fn: F) -> Self {
        let prometheus_global_stats = PrometheusStats::default();
        let prometheus_global_stats_clone = prometheus_global_stats.clone();
        let prometheus_client_stats = PrometheusStats::default();
        let prometheus_client_stats_clone = prometheus_client_stats.clone();

        // Need to run the metrics server in a different thread to avoid blocking
        thread::spawn(move || {
            block_on(serve_metrics(
                listener,
                prometheus_global_stats_clone,
                prometheus_client_stats_clone,
            ))
            .map_err(|err| log::error!("{err:?}"))
            .ok();
        });
        Self {
            print_fn,
            prometheus_global_stats,
            prometheus_client_stats,
        }
    }
    /// Creates the monitor with a given `start_time`.
    #[deprecated(
        since = "0.16.0",
        note = "Please use new to create. start_time is useless here."
    )]
    pub fn with_time(listener: String, print_fn: F, _start_time: Duration) -> Self {
        Self::new(listener, print_fn)
    }
}

/// Set up an HTTP endpoint /metrics
pub(crate) async fn serve_metrics(
    listener: String,
    global_stats: PrometheusStats,
    client_stats: PrometheusStats,
) -> Result<(), std::io::Error> {
    let mut registry = Registry::default();

    // Register the global stats
    registry.register(
        "global_corpus_count",
        "Number of test cases in the corpus",
        global_stats.corpus_count,
    );
    registry.register(
        "global_objective_count",
        "Number of times the objective has been achieved (e.g., crashes)",
        global_stats.objective_count,
    );
    registry.register(
        "global_executions_total",
        "Total number of executions",
        global_stats.executions,
    );
    registry.register(
        "execution_rate",
        "Rate of executions per second",
        global_stats.exec_rate,
    );
    registry.register(
        "global_runtime",
        "How long the fuzzer has been running for (seconds)",
        global_stats.runtime,
    );
    registry.register(
        "global_clients_count",
        "How many clients have been spawned for the fuzzing job",
        global_stats.clients_count,
    );
    registry.register(
        "global_custom_stat",
        "A metric to contain custom stats returned by feedbacks, filterable by label (aggregated)",
        global_stats.custom_stat,
    );

    // Register the client stats
    registry.register(
        "corpus_count",
        "Number of test cases in the client's corpus",
        client_stats.corpus_count,
    );
    registry.register(
        "objective_count",
        "Number of client's objectives (e.g., crashes)",
        client_stats.objective_count,
    );
    registry.register(
        "executions_total",
        "Total number of client executions",
        client_stats.executions,
    );
    registry.register(
        "execution_rate",
        "Rate of executions per second",
        client_stats.exec_rate,
    );
    registry.register(
        "runtime",
        "How long the client has been running for (seconds)",
        client_stats.runtime,
    );
    registry.register(
        "clients_count",
        "How many clients have been spawned for the fuzzing job",
        client_stats.clients_count,
    );
    registry.register(
        "custom_stat",
        "A metric to contain custom stats returned by feedbacks, filterable by label",
        client_stats.custom_stat,
    );

    let mut app = tide::with_state(State {
        registry: Arc::new(registry),
    });

    app.at("/")
        .get(|_| async { Ok("LibAFL Prometheus Monitor") });
    app.at("/metrics").get(|req: Request<State>| async move {
        let mut encoded = String::new();
        encode(&mut encoded, &req.state().registry).unwrap();
        let response = tide::Response::builder(200)
            .body(encoded)
            .content_type("application/openmetrics-text; version=1.0.0; charset=utf-8")
            .build();
        Ok(response)
    });
    app.listen(listener).await?;

    Ok(())
}

/// Struct used to define the labels in `prometheus`.
#[derive(Clone, Hash, PartialEq, Eq, EncodeLabelSet, Debug)]
pub struct Labels {
    /// The `sender_id` helps to differentiate between clients when multiple are spawned.
    client: Cow<'static, str>,
    /// Used for `custom_stat` filtering.
    stat: Cow<'static, str>,
}

/// The state for this monitor.
#[derive(Clone)]
struct State {
    registry: Arc<Registry>,
}
