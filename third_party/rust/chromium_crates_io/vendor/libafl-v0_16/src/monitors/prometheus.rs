//! The [`PrometheusMonitor`] exposes fuzzer progress as a prometheus endpoint.
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
//! Pair it with a [`MultiMonitor`](crate::monitors::MultiMonitor) for console output.
//! In your fuzzer:
//!
//! ```rust
//! // First, include:
//! use libafl::monitors::{MultiMonitor, PrometheusMonitor};
//!
//! // Then, create the monitor:
//! let listener = "127.0.0.1:8080"; // point prometheus to scrape here in your prometheus.yml
//! let mon = (
//!     PrometheusMonitor::new(listener),
//!     MultiMonitor::new(|s| log::info!("{s}")),
//! );
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
use core::{fmt::Debug, net::SocketAddr, sync::atomic::AtomicU64, time::Duration};
use std::net::ToSocketAddrs;

// using axum for the HTTP server library (fast, async, modular)
use axum::{
    Router, extract::State as AxumState, http::header, response::IntoResponse, routing::get,
};
use libafl_bolts::{ClientId, Error, current_time};
// using the official rust client library for Prometheus: https://github.com/prometheus/client_rust
use prometheus_client::{
    encoding::{EncodeLabelSet, text::encode},
    metrics::{family::Family, gauge::Gauge},
    registry::Registry,
};
use tokio::net::TcpListener;

use crate::monitors::{
    Monitor,
    stats::{manager::ClientStatsManager, user_stats::UserStatsValue},
};

/// Prometheus metrics for global and each client.
#[derive(Debug, Clone, Default)]
pub struct PrometheusStats {
    corpus_count: Family<ClientLabels, Gauge>,
    objective_count: Family<ClientLabels, Gauge>,
    executions: Family<ClientLabels, Gauge>,
    exec_rate: Family<ClientLabels, Gauge<f64, AtomicU64>>,
    runtime: Family<ClientLabels, Gauge>,
    clients_count: Family<ClientLabels, Gauge>,
    custom_stat: Family<CustomStatLabels, Gauge<f64, AtomicU64>>,
}

impl PrometheusStats {
    fn init_global(&self) {
        let global = ClientLabels {
            client: Cow::from("global"),
        };
        self.corpus_count.get_or_create(&global).set(0);
        self.objective_count.get_or_create(&global).set(0);
        self.executions.get_or_create(&global).set(0);
        self.exec_rate.get_or_create(&global).set(0.0);
        self.runtime.get_or_create(&global).set(0);
        self.clients_count.get_or_create(&global).set(0);
    }
}

/// Tracking monitor during fuzzing.
#[derive(Clone, Debug)]
pub struct PrometheusMonitor {
    stats: PrometheusStats, // unified prometheus metrics
    listener: SocketAddr,   // server address
    runtime: Arc<std::sync::Mutex<Option<tokio::runtime::Runtime>>>, // background tokio runtime
}

impl Monitor for PrometheusMonitor {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        let _ = event_msg; // logging is handled by a paired monitor (e.g., MultiMonitor)

        let mut runtime_lock = self.runtime.lock().unwrap();
        if runtime_lock.is_none() {
            let runtime = tokio::runtime::Builder::new_multi_thread()
                .enable_all()
                .build()
                .unwrap();
            let listener = self.listener;
            let stats = self.stats.clone();
            runtime.spawn(async move {
                serve_metrics(listener, stats)
                    .await
                    .map_err(|err| log::error!("{err:?}"))
                    .ok();
            });
            *runtime_lock = Some(runtime);
        }
        drop(runtime_lock);

        let global_stats = client_stats_manager.global_stats();
        let global = ClientLabels {
            client: Cow::from("global"),
        };

        // Global (aggregated) metrics
        self.stats
            .corpus_count
            .get_or_create(&global)
            .set(global_stats.corpus_size.try_into().unwrap());

        self.stats
            .objective_count
            .get_or_create(&global)
            .set(global_stats.objective_size.try_into().unwrap());

        self.stats
            .executions
            .get_or_create(&global)
            .set(global_stats.total_execs.try_into().unwrap());

        self.stats
            .exec_rate
            .get_or_create(&global)
            .set(global_stats.execs_per_sec);

        self.stats
            .runtime
            .get_or_create(&global)
            .set(global_stats.run_time.as_secs().try_into().unwrap());

        let total_clients: i64 = global_stats.client_stats_count.try_into().unwrap();
        self.stats
            .clients_count
            .get_or_create(&global)
            .set(total_clients);

        // Global aggregated custom stats
        for (key, val) in client_stats_manager.aggregated() {
            #[expect(clippy::cast_precision_loss)]
            let value: f64 = match val {
                UserStatsValue::Number(n) => *n as f64,
                UserStatsValue::Float(f) => *f,
                UserStatsValue::String(_s) => 0.0,
                UserStatsValue::Ratio(a, b) => {
                    if key == "edges" {
                        self.stats
                            .custom_stat
                            .get_or_create(&CustomStatLabels {
                                client: Cow::from("global"),
                                stat: Cow::from("edges_total"),
                            })
                            .set(*b as f64);
                        self.stats
                            .custom_stat
                            .get_or_create(&CustomStatLabels {
                                client: Cow::from("global"),
                                stat: Cow::from("edges_hit"),
                            })
                            .set(*a as f64);
                    }
                    (*a as f64 / *b as f64) * 100.0
                }
                UserStatsValue::Percent(p) => *p * 100.0,
            };
            self.stats
                .custom_stat
                .get_or_create(&CustomStatLabels {
                    client: Cow::from("global"),
                    stat: key.clone(),
                })
                .set(value);
        }

        // Client-specific metrics

        client_stats_manager.client_stats_insert(sender_id)?;
        let client = client_stats_manager.client_stats_for(sender_id)?;
        let mut cur_client_clone = client.clone();

        let client_label = ClientLabels {
            client: Cow::from(sender_id.0.to_string()),
        };

        self.stats
            .corpus_count
            .get_or_create(&client_label)
            .set(cur_client_clone.corpus_size().try_into().unwrap());

        self.stats
            .objective_count
            .get_or_create(&client_label)
            .set(cur_client_clone.objective_size().try_into().unwrap());

        self.stats
            .executions
            .get_or_create(&client_label)
            .set(cur_client_clone.executions().try_into().unwrap());

        self.stats
            .exec_rate
            .get_or_create(&client_label)
            .set(cur_client_clone.execs_per_sec(current_time()));

        let client_run_time = current_time()
            .saturating_sub(cur_client_clone.start_time())
            .as_secs();
        self.stats
            .runtime
            .get_or_create(&client_label)
            .set(client_run_time.try_into().unwrap()); // run time in seconds per-client, which can be converted to a time format by Grafana or similar

        self.stats
            .clients_count
            .get_or_create(&client_label)
            .set(total_clients);

        // Per-client custom stats
        for (key, val) in cur_client_clone.user_stats() {
            #[expect(clippy::cast_precision_loss)]
            let value: f64 = match val.value() {
                UserStatsValue::Number(n) => *n as f64,
                UserStatsValue::Float(f) => *f,
                UserStatsValue::String(_s) => 0.0,
                UserStatsValue::Ratio(a, b) => {
                    if key == "edges" {
                        self.stats
                            .custom_stat
                            .get_or_create(&CustomStatLabels {
                                client: Cow::from(sender_id.0.to_string()),
                                stat: Cow::from("edges_total"),
                            })
                            .set(*b as f64);
                        self.stats
                            .custom_stat
                            .get_or_create(&CustomStatLabels {
                                client: Cow::from(sender_id.0.to_string()),
                                stat: Cow::from("edges_hit"),
                            })
                            .set(*a as f64);
                    }
                    (*a as f64 / *b as f64) * 100.0
                }
                UserStatsValue::Percent(p) => *p * 100.0,
            };
            self.stats
                .custom_stat
                .get_or_create(&CustomStatLabels {
                    client: Cow::from(sender_id.0.to_string()),
                    stat: key.clone(),
                })
                .set(value);
        }
        Ok(())
    }
}

impl PrometheusMonitor {
    /// Create a new [`PrometheusMonitor`].
    /// The `listener` is the address to bind the metrics HTTP endpoint to.
    /// Pair with a [`MultiMonitor`](crate::monitors::MultiMonitor) for console output.
    pub fn new<T>(listener: T) -> Self
    where
        T: ToSocketAddrs,
    {
        let addr = listener
            .to_socket_addrs()
            .expect("Failed to resolve socket address")
            .next()
            .expect("No socket addresses resolved");
        let stats = PrometheusStats::default();
        stats.init_global();

        Self {
            stats,
            listener: addr,
            runtime: Arc::new(std::sync::Mutex::new(None)),
        }
    }

    /// Creates the monitor with a given `start_time`.
    #[deprecated(
        since = "0.16.0",
        note = "Please use new to create. start_time is useless here."
    )]
    pub fn with_time<T>(listener: T, _start_time: Duration) -> Self
    where
        T: ToSocketAddrs,
    {
        Self::new(listener)
    }
}

pub(crate) async fn serve_metrics(
    listener: SocketAddr,
    stats: PrometheusStats,
) -> Result<(), std::io::Error> {
    let mut registry = Registry::default();

    registry.register(
        "corpus_count",
        "Number of test cases in the corpus",
        stats.corpus_count,
    );
    registry.register(
        "objective_count",
        "Number of times the objective has been achieved (e.g., crashes)",
        stats.objective_count,
    );
    registry.register(
        "executions_total",
        "Total number of executions",
        stats.executions,
    );
    registry.register(
        "execution_rate",
        "Rate of executions per second",
        stats.exec_rate,
    );
    registry.register(
        "runtime_seconds",
        "How long the fuzzer has been running for (seconds)",
        stats.runtime,
    );
    registry.register(
        "clients_count",
        "How many clients have been spawned for the fuzzing job",
        stats.clients_count,
    );
    registry.register(
        "custom_stat",
        "A metric to contain custom stats returned by feedbacks, filterable by label",
        stats.custom_stat,
    );

    let state = State {
        registry: Arc::new(registry),
    };

    let app = Router::new()
        .route("/", get(get_root))
        .route("/metrics", get(get_metrics))
        .with_state(state);

    let listener = TcpListener::bind(&listener).await?;
    axum::serve(listener, app).await?;

    Ok(())
}

/// Labels for standard per-client metrics (corpus, executions, etc.).
#[derive(Clone, Hash, PartialEq, Eq, EncodeLabelSet, Debug)]
pub struct ClientLabels {
    /// The `sender_id` helps to differentiate between clients when multiple are spawned.
    client: Cow<'static, str>,
}

/// Labels for custom user-defined stats, adding a `stat` dimension for filtering.
#[derive(Clone, Hash, PartialEq, Eq, EncodeLabelSet, Debug)]
pub struct CustomStatLabels {
    /// The `sender_id` helps to differentiate between clients when multiple are spawned.
    client: Cow<'static, str>,
    /// The name of the custom stat (e.g. `edges`, `edges_hit`, `edges_total`).
    stat: Cow<'static, str>,
}

/// The state for this monitor.
#[derive(Clone)]
struct State {
    registry: Arc<Registry>,
}

async fn get_root() -> &'static str {
    "LibAFL Prometheus Monitor"
}

async fn get_metrics(AxumState(state): AxumState<State>) -> impl IntoResponse {
    let mut encoded = String::new();
    encode(&mut encoded, &state.registry).unwrap();
    (
        [(
            header::CONTENT_TYPE,
            "application/openmetrics-text; version=1.0.0; charset=utf-8",
        )],
        encoded,
    )
}

#[cfg(test)]
mod tests {
    use alloc::string::String;
    use core::time::Duration;
    use std::{
        io::{Read, Write},
        net::TcpStream,
        thread::sleep,
    };

    use libafl_bolts::ClientId;

    use crate::monitors::{Monitor, PrometheusMonitor, stats::ClientStatsManager};

    #[test]
    fn test_prometheus_monitor() {
        let mut client_stats = ClientStatsManager::new();
        let mut mon = PrometheusMonitor::new("127.0.0.1:18081");
        mon.display(&mut client_stats, "test", ClientId(0)).unwrap();

        // Give the server a moment to start up in the background thread
        sleep(Duration::from_millis(500));

        let mut stream =
            TcpStream::connect("127.0.0.1:18081").expect("Failed to connect to prometheus monitor");
        stream
            .set_read_timeout(Some(Duration::from_millis(500)))
            .unwrap();
        stream
            .write_all(b"GET /metrics HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            .unwrap();

        let mut response = String::new();
        stream.read_to_string(&mut response).unwrap();

        assert!(response.contains("executions_total"));
        assert!(response.contains("execution_rate"));
    }
}
