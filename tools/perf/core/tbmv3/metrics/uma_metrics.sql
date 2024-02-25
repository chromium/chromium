-- Copyright 2020 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

CREATE VIEW histogram_samples AS
SELECT
  EXTRACT_ARG(arg_set_id, 'chrome_histogram_sample.name') as histogram_name,
  EXTRACT_ARG(arg_set_id, 'chrome_histogram_sample.sample') as sample
FROM slice
WHERE name = 'HistogramSample'
  AND category = 'disabled-by-default-histogram_samples';

CREATE VIEW uma_metrics_output AS
SELECT UMAMetrics(
  'compositing_display_draw_to_swap', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'Compositing.Display.DrawToSwapUs'
  ),
  'compositor_latency_total_latency', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'CompositorLatency.TotalLatency'
  ),
  'event_latency_first_gesture_scroll_update_touchscreen_total_latency', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'EventLatency.FirstGestureScrollUpdate.Touchscreen.TotalLatency'
  ),
  'event_latency_gesture_scroll_update_touchscreen_total_latency', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency'
  ),
  'event_latency_first_gesture_scroll_update_wheel_total_latency', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'EventLatency.FirstGestureScrollUpdate.Wheel.TotalLatency'
  ),
  'event_latency_gesture_scroll_update_wheel_total_latency', (
    SELECT RepeatedField(sample / 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'EventLatency.GestureScrollUpdate.Wheel.TotalLatency'
  ),
  'graphics_smoothness_checkerboarding_compositor_animation', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.CompositorAnimation'
  ),
  'graphics_smoothness_checkerboarding_main_thread_animation', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.MainThreadAnimation'
  ),
  'graphics_smoothness_checkerboarding_pinch_zoom', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.PinchZoom'
  ),
  'graphics_smoothness_checkerboarding_raf', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.RAF'
  ),
  'graphics_smoothness_checkerboarding_touch_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.TouchScroll'
  ),
  'graphics_smoothness_checkerboarding_wheel_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.Checkerboarding3.WheelScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_all_animations', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllAnimations'
  ),
  'graphics_smoothness_percent_dropped_frames_all_interactions', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllInteractions'
  ),
  'graphics_smoothness_percent_dropped_frames_all_sequences', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.AllSequences'
  ),
  'graphics_smoothness_percent_dropped_frames_main_thread_main_thread_animation', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.MainThread.MainThreadAnimation'
  ),
  'graphics_smoothness_percent_dropped_frames_main_thread_raf', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.MainThread.RAF'
  ),
  'graphics_smoothness_percent_dropped_frames_main_thread_touch_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.MainThread.TouchScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_main_thread_wheel_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.MainThread.WheelScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_compositor_thread_compositor_animation', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.CompositorThread.CompositorAnimation'
  ),
  'graphics_smoothness_percent_dropped_frames_compositor_thread_pinch_zoom', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.CompositorThread.PinchZoom'
  ),
  'graphics_smoothness_percent_dropped_frames_compositor_thread_touch_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.CompositorThread.TouchScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_compositor_thread_wheel_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.CompositorThread.WheelScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_scrolling_thread_touch_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.ScrollingThread.TouchScroll'
  ),
  'graphics_smoothness_percent_dropped_frames_scrolling_thread_wheel_scroll', (
    SELECT RepeatedField(sample / 1e2)
    FROM histogram_samples
    WHERE histogram_name = 'Graphics.Smoothness.PercentDroppedFrames3.ScrollingThread.WheelScroll'
  ),
  'memory_gpu_peak_memory_usage_scroll', (
    SELECT RepeatedField(sample * 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'Memory.GPU.PeakMemoryUsage2.Scroll'
  ),
  'memory_gpu_peak_memory_usage_page_load', (
    SELECT RepeatedField(sample * 1e3)
    FROM histogram_samples
    WHERE histogram_name = 'Memory.GPU.PeakMemoryUsage2.PageLoad'
  )
);
