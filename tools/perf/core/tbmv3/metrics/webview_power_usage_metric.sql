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
        'app_name', webview_browser_slices_power_summary.app_name,
        'webview_browser_slices_mas', webview_browser_slices_power_summary.power_mas,
        'webview_only_usage',
        (SELECT UsageByCoreType(
          'little_cores_mas', webview_only_power_output.little_cores_mas,
          'big_cores_mas', webview_only_power_output.big_cores_mas,
          'bigger_cores_mas', webview_only_power_output.bigger_cores_mas,
          'total_mas', webview_only_power_output.total_mas
        )),
        'total_app_usage',
        (SELECT UsageByCoreType(
          'little_cores_mas', total_app_power_output.little_cores_mas,
          'big_cores_mas', total_app_power_output.big_cores_mas,
          'bigger_cores_mas', total_app_power_output.bigger_cores_mas,
          'total_mas', total_app_power_output.total_mas
        )),
        'renderer_usage',
        (SELECT UsageByCoreType(
          'little_cores_mas', webview_renderer_power_output.little_cores_mas,
          'big_cores_mas', webview_renderer_power_output.big_cores_mas,
          'bigger_cores_mas', webview_renderer_power_output.bigger_cores_mas,
          'total_mas', webview_renderer_power_output.total_mas
        ))
      )
   )
   FROM webview_browser_slices_power_summary
     INNER JOIN webview_only_power_output
       ON webview_browser_slices_power_summary.app_name = webview_only_power_output.app_name
     INNER JOIN total_app_power_output
       ON webview_browser_slices_power_summary.app_name = total_app_power_output.app_name
     INNER JOIN webview_renderer_power_output
       ON webview_browser_slices_power_summary.app_name = webview_renderer_power_output.app_name
  ),
  'total_device_power_mas',
  (SELECT power_mas FROM total_device_power)
);
