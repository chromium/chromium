-- WebView is embedded in the hosting app's main process, which means it shares some threads
-- with the host app's work. We approximate WebView-related power usage
-- by selecting user slices that belong to WebView and estimating their power use
-- through the CPU time they consume at different core frequencies.
-- This metric requires the power_profile table to be filled with the device power
-- profile data.
-- Output values are in milliampere-seconds.

SELECT RUN_METRIC('webview/webview_power_usage.sql');

CREATE VIEW webview_power_usage_metric_output AS
SELECT WebViewPowerUsageMetric(
  'estimated_webview_app_power_usage',
  (SELECT RepeatedField(
      EstimatedWebViewAppPowerUsage(
        'app_name', app_name,
        'webview_power_mas', webview_power_mas,
        'total_app_power_mas', total_app_power_mas,
        'webview_power_little_cores_mas', webview_power_little_cores_mas,
        'webview_power_big_cores_mas', webview_power_big_cores_mas,
        'webview_power_bigger_cores_mas', webview_power_bigger_cores_mas
       )
   )
   FROM webview_power_summary
  ),
  'total_device_power_mas',
  (SELECT power_mas FROM total_device_power)
);
