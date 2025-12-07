-- Copyright 2020 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

-- A view of all histogram samples from a trace.
CREATE VIEW histogram_samples AS
SELECT
  EXTRACT_ARG(arg_set_id, 'chrome_histogram_sample.name') as histogram_name,
  EXTRACT_ARG(arg_set_id, 'chrome_histogram_sample.sample') as sample
FROM slice
WHERE name = 'HistogramSample'
  AND category = 'disabled-by-default-histogram_samples';

-- A view that extracts UMA metrics from histogram samples.
-- The values are scaled to match the units in the proto definition.
CREATE VIEW uma_metrics_output AS
SELECT UMAMetrics(
  'browser', UMAMetrics_Browser(
    'main_threads_congestion', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Browser.MainThreadsCongestion'
    )
  ),
  'compositing', UMAMetrics_Compositing(
    'display_draw_to_swap', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Compositing.Display.DrawToSwapUs'
    )
  ),
  'compositor_latency', UMAMetrics_CompositorLatency(
    'total_latency', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'CompositorLatency.TotalLatency'
    ),
    'type', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'CompositorLatency.Type'
    )
  ),
  'event_jank', UMAMetrics_EventJank(
    'predictor_janky_frame_percentage2', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Event.Jank.PredictorJankyFramePercentage2'
    ),
    'scroll_update_fast_scroll_missed_vsync_frame_above_janky_threshold2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.Jank.ScrollUpdate.FastScroll.MissedVsync.FrameAboveJankyThreshold2'
    ),
    'scroll_update_fast_scroll_no_missed_vsync_frame_above_janky_threshold2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.Jank.ScrollUpdate.FastScroll.NoMissedVsync.FrameAboveJankyThreshold2'
    ),
    'scroll_update_slow_scroll_missed_vsync_frame_above_janky_threshold2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.Jank.ScrollUpdate.SlowScroll.MissedVsync.FrameAboveJankyThreshold2'
    ),
    'scroll_update_slow_scroll_no_missed_vsync_frame_above_janky_threshold2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.Jank.ScrollUpdate.SlowScroll.NoMissedVsync.FrameAboveJankyThreshold2'
    )
  ),
  'event_scroll_jank', UMAMetrics_EventScrollJank(
    'delayed_frames_percentage_fixed_window', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.DelayedFramesPercentage.FixedWindow'
    ),
    'delayed_frames_percentage_per_scroll', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.DelayedFramesPercentage.PerScroll'
    ),
    'missed_vsyncs_percentage_fixed_window', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.MissedVsyncsPercentage.FixedWindow'
    ),
    'missed_vsyncs_percentage_per_scroll', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.MissedVsyncsPercentage.PerScroll'
    ),
    'missed_vsyncs_sum_fixed_window', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.MissedVsyncsSum.FixedWindow'
    ),
    'missed_vsyncs_sum_per_scroll', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Event.ScrollJank.MissedVsyncsSum.PerScroll'
    )
  ),
  'event_latency', UMAMetrics_EventLatency(
    'first_gesture_scroll_update_total_latency2', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'EventLatency.FirstGestureScrollUpdate.TotalLatency2'
    ),
    'gesture_scroll_update_total_latency2', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'EventLatency.GestureScrollUpdate.TotalLatency2'
    ),
    'gesture_scroll_update_touchscreen_total_latency', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency'
    )
  ),
  'graphics_smoothness', UMAMetrics_GraphicsSmoothness(
    'percent_dropped_frames3', UMAMetrics_GraphicsSmoothness_PercentDroppedFrames(
      'all_animations', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllAnimations'
      ),
      'all_interactions', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllInteractions'
      ),
      'all_sequences', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllSequences'
      )
    ),
    'percent_dropped_frames4', UMAMetrics_GraphicsSmoothness_PercentDroppedFrames(
      'all_animations', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames4.AllAnimations'
      ),
      'all_interactions', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames4.AllInteractions'
      ),
      'all_sequences', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames4.AllSequences'
      )
    ),
    'checkerboarding3', UMAMetrics_GraphicsSmoothness_Checkerboarding(
      'all_interactions', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.AllInteractions'
      )
    ),
    'checkerboarding4', UMAMetrics_GraphicsSmoothness_Checkerboarding(
      'all_animations', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding4.AllAnimations'
      ),
      'all_interactions', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding4.AllInteractions'
      ),
      'all_sequences', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding4.AllSequences'
      ),
      'compositor_thread_scrollbar_scroll', (
        SELECT RepeatedField(sample / 100.0)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding4.CompositorThread.ScrollbarScroll'
      )
    ),
    'jank3', UMAMetrics_GraphicsSmoothness_Jank(
      'all_animations', (
        SELECT RepeatedField(sample)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Jank3.AllAnimations'
      ),
      'all_interactions', (
        SELECT RepeatedField(sample)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Jank3.AllInteractions'
      ),
      'all_sequences', (
        SELECT RepeatedField(sample)
        FROM histogram_samples
        WHERE histogram_name = 'Graphics.Smoothness.Jank3.AllSequences'
      )
    ),
    'checkerboarding_need_raster4_all_sequences', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Graphics.Smoothness.CheckerboardingNeedRaster4.AllSequences'
    ),
    'checkerboarding_need_record4_all_sequences', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'Graphics.Smoothness.CheckerboardingNeedRecord4.AllSequences'
    )
  ),
  'memory', UMAMetrics_Memory(
    'experimental_renderer2_small_malloc_brp_quarantined', (
      SELECT RepeatedField(sample * 1024)
      FROM histogram_samples
      WHERE histogram_name = 'Memory.Experimental.Renderer2.Small.Malloc.BRPQuarantined'
    ),
    'gpu_peak_memory_usage2_page_load', (
      SELECT RepeatedField(sample * 1024 * 1024)
      FROM histogram_samples
      WHERE histogram_name = 'Memory.GPU.PeakMemoryUsage2.PageLoad'
    ),
    'gpu_peak_memory_usage2_scroll', (
      SELECT RepeatedField(sample * 1024 * 1024)
      FROM histogram_samples
      WHERE histogram_name = 'Memory.GPU.PeakMemoryUsage2.Scroll'
    )
  ),
  'new_tab_page', UMAMetrics_NewTabPage(
    'load_time_web_ui_ntp', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'NewTabPage.LoadTime.WebUINTP'
    ),
    'main_ui_shown_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'NewTabPage.MainUi.ShownTime'
    ),
    'modules_shown_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'NewTabPage.Modules.ShownTime'
    )
  ),
  'omnibox', UMAMetrics_Omnibox(
    'char_typed_to_repaint_latency_to_paint', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.CharTypedToRepaintLatency.ToPaint'
    ),
    'char_typed_to_repaint_latency', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.CharTypedToRepaintLatency'
    ),
    'paint_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.PaintTime'
    ),
    'query_time2_0', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.QueryTime2.0'
    ),
    'query_time2_1', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.QueryTime2.1'
    ),
    'query_time2_2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Omnibox.QueryTime2.2'
    )
  ),
  'page_load', UMAMetrics_PageLoad(
    'clients_ads_all_pages_non_ad_network_bytes', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.AllPages.NonAdNetworkBytes'
    ),
    'clients_ads_all_pages_percent_network_bytes_ads', (
      SELECT RepeatedField(sample / 100.0)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.AllPages.PercentNetworkBytesAds'
    ),
    'clients_ads_bytes_ad_frames_aggregate_total2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2'
    ),
    'clients_ads_cpu_ad_frames_aggregate_total_usage2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.Cpu.AdFrames.Aggregate.TotalUsage2'
    ),
    'clients_ads_cpu_full_page_total_usage2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.Cpu.FullPage.TotalUsage2'
    ),
    'clients_ads_frame_counts_ad_frames_total', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.FrameCounts.AdFrames.Total'
    ),
    'clients_ads_resources_bytes_ads2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.Ads.Resources.Bytes.Ads2'
    ),
    'clients_fenced_frames_paint_timing_navigation_to_first_contentful_paint', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.FencedFrames.PaintTiming.NavigationToFirstContentfulPaint'
    ),
    'clients_third_party_frames_navigation_to_first_contentful_paint3', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3'
    ),
    'cpu_total_usage', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.Cpu.TotalUsage'
    ),
    'layout_instability_cumulative_shift_score', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.LayoutInstability.CumulativeShiftScore'
    ),
    'paint_timing_navigation_to_first_contentful_paint', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.PaintTiming.NavigationToFirstContentfulPaint'
    ),
    'paint_timing_navigation_to_largest_contentful_paint', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'PageLoad.PaintTiming.NavigationToLargestContentfulPaint'
    )
  ),
  'subresource_filter', UMAMetrics_SubresourceFilter(
    'page_load_num_subresource_loads_matched_rules', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules'
    )
  ),
  'tab_search', UMAMetrics_TabSearch(
    'close_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.CloseAction'
    ),
    'mojo_switch_to_tab_is_overlap', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.Mojo.SwitchToTab.IsOverlap'),
    'mojo_switch_to_tab', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.Mojo.SwitchToTab'),
    'mojo_tab_updated_is_overlap', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.Mojo.TabUpdated.IsOverlap'),
    'mojo_tab_updated', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.Mojo.TabUpdated'),
    'num_tabs_closed_per_instance', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.NumTabsClosedPerInstance'),
    'num_tabs_on_open', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.NumTabsOnOpen'),
    'num_windows_on_open', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.NumWindowsOnOpen'),
    'open_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.OpenAction'),
    'page_handler_construction_delay', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.PageHandlerConstructionDelay'),
    'web_ui_initial_tabs_render_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.InitialTabsRenderTime'),
    'web_ui_load_completed_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.LoadCompletedTime'),
    'web_ui_load_document_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.LoadDocumentTime'),
    'web_ui_search_algorithm_duration', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.SearchAlgorithmDuration'),
    'web_ui_tab_list_data_received', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.TabListDataReceived'),
    'web_ui_tab_list_data_received2_is_overlap', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.TabListDataReceived2.IsOverlap'),
    'web_ui_tab_list_data_received2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.TabListDataReceived2'),
    'web_ui_tab_switch_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WebUI.TabSwitchAction'),
    'window_displayed_duration2', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WindowDisplayedDuration2'),
    'window_time_to_show_cached_web_view', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WindowTimeToShowCachedWebView'),
    'window_time_to_show_uncached_web_view', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'Tabs.TabSearch.WindowTimeToShowUncachedWebView')
  ),
  'tab_strip', UMAMetrics_TabStrip(
    'tab_views_activation_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'TabStrip.Tab.Views.ActivationAction'
    ),
    'tab_web_ui_activation_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'TabStrip.Tab.WebUI.ActivationAction'
    )
  ),
  'web_ui_tab_strip', UMAMetrics_WebUITabStrip(
    'close_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.CloseAction'),
    'close_tab_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.CloseTabAction'),
    'load_completed_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.LoadCompletedTime'),
    'load_document_time', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.LoadDocumentTime'),
    'open_action', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.OpenAction'),
    'open_duration', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.OpenDuration'),
    'tab_activation', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.TabActivation'),
    'tab_creation', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.TabCreation'),
    'tab_data_received', (
      SELECT RepeatedField(sample)
      FROM histogram_samples
      WHERE histogram_name = 'WebUITabStrip.TabDataReceived')
  ),
  'shared_storage', UMAMetrics_SharedStorage(
    'document_timing_add_module', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.AddModule'),
    'document_timing_append', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Append'),
    'document_timing_clear', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Clear'),
    'document_timing_delete', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Delete'),
    'document_timing_run_executed_in_worklet', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet'),
    'document_timing_run', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Run'),
    'document_timing_select_url_executed_in_worklet', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet'),
    'document_timing_select_url', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.SelectURL'),
    'document_timing_set', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Document.Timing.Set'),
    'worklet_timing_append', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Append'),
    'worklet_timing_clear', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Clear'),
    'worklet_timing_delete', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Delete'),
    'worklet_timing_entries_next', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Entries.Next'),
    'worklet_timing_get', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Get'),
    'worklet_timing_keys_next', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Keys.Next'),
    'worklet_timing_length', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Length'),
    'worklet_timing_remaining_budget', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.RemainingBudget'),
    'worklet_timing_set', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Set'),
    'worklet_timing_values_next', (
      SELECT RepeatedField(sample / 1000.0)
      FROM histogram_samples
      WHERE histogram_name = 'Storage.SharedStorage.Worklet.Timing.Values.Next')
  )
);
