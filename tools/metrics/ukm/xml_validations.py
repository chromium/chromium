# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import setup_modules  # pylint: disable=unused-import

from chromium_src.tools.metrics.histograms import extract_histograms
from chromium_src.tools.metrics.histograms import histogram_paths
from chromium_src.tools.metrics.histograms import merge_xml

LOCAL_METRIC_RE = re.compile(r'metrics\.([^,]+)')
INVALID_LOCAL_METRIC_FIELD_ERROR = (
  'Invalid index field specification in ukm metric %(event)s:%(metric)s, the '
  'following metrics are used as index fields but are not configured to '
  'support it: [%(invalid_metrics)s]\n\n'
  'See https://chromium.googlesource.com/chromium/src.git/+/main/services/'
  'metrics/ukm_api.md#aggregation-by-metrics-in-the-same-event for '
  'instructions on how to configure them.')

INVALID_AGGREGATION_STATISTIC_ERROR = (
    'Invalid statistics field specification in ukm.xml, in metric '
    '%(event)s:%(metric)s. To have a metric aggregated, aggregation, history '
    'and statistics tags need to be added along with the type of statistic. '
    'See https://chromium.googlesource.com/chromium/src.git/+/main/services/'
    'metrics/ukm_api.md#controlling-the-aggregation-of-metrics.')

VALID_STATISTICS = {
    'enumeration': [''],
    'quantiles': ['std-percentiles'],
}

# Case-insensitive metric names that are forbidden because they collide with
# reserved UKM internal keywords.
FORBIDDEN_METRIC_NAMES = {
    'event',
    'metadata',
}

# Case-insensitive component words that suggest a metric is a time field.
# This imitates an internal Google linter for best protobuf practice, where
# violations can block the sync'ing the UKM event definitions server-side.
TIME_KEYWORDS = frozenset({
    'date', 'deadline', 'delay', 'duration', 'elapsed', 'epoch', 'expiration',
    'interval', 'latency', 'period', 'runtime', 'time', 'timeout', 'timestamp'
})

# Case-insensitive unit signifiers that must be present in a time field metric.
TIME_UNITS = frozenset({
    'day', 'days', 'hour', 'hours', 'hr', 'hrs', 'micro', 'micros', 'microsec',
    'microsecond', 'microseconds', 'microsecs', 'milli', 'millis', 'millisec',
    'millisecond', 'milliseconds', 'millisecs', 'mins', 'minute', 'minutes',
    'month', 'months', 'ms', 'msec', 'msecs', 'nano', 'nanos', 'nanosec',
    'nanosecond', 'nanoseconds', 'nanosecs', 'ns', 'nsec', 'nsecs', 'pico',
    'picos', 'picosec', 'picosecond', 'picoseconds', 'picosecs', 'ps', 'psec',
    'psecs', 'quarter', 'quarters', 'sec', 'second', 'seconds', 'secs', 'us',
    'usec', 'usecs', 'week', 'weeks', 'year', 'years'
})

MISSING_TIME_METRIC_UNIT_ERROR = (
    "Metric '%(metric)s' in event '%(event)s' appears to be related to time "
    'but does not include a unit in its name. We detected it as a time field '
    'due to the presence of one or more of the following components '
    '(%(time_keywords)s) and expect one or more unit signifiers '
    '(%(time_units)s) to be present.')

# Allowlist of existing (event, metric) names that indicate a time field but do
# not use a unit signifier. Do not add entries to this list, new metrics should
# be named appropriately to pass the time unit name check.
_TIME_UNIT_EVENT_METRIC_ALLOWLIST = frozenset({
    ('AbandonedSRPNavigation', 'AFTEndTime'),
    ('AbandonedSRPNavigation', 'AFTStartTime'),
    ('AbandonedSRPNavigation', 'BodyChunkEndTime'),
    ('AbandonedSRPNavigation', 'BodyChunkStartTime'),
    ('AbandonedSRPNavigation', 'CommitReceivedTime'),
    ('AbandonedSRPNavigation', 'CommitSentTime'),
    ('AbandonedSRPNavigation', 'DidCommitTime'),
    ('AbandonedSRPNavigation', 'DOMContentLoadedTime'),
    ('AbandonedSRPNavigation', 'FirstContentfulPaintTime'),
    ('AbandonedSRPNavigation', 'FirstRedirectedRequestStartTime'),
    ('AbandonedSRPNavigation', 'HeaderChunkEndTime'),
    ('AbandonedSRPNavigation', 'HeaderChunkStartTime'),
    ('AbandonedSRPNavigation', 'LargestContentfulPaintTime'),
    ('AbandonedSRPNavigation', 'LoaderStartTime'),
    ('AbandonedSRPNavigation', 'LoadEventStartedTime'),
    ('AbandonedSRPNavigation', 'NonRedirectedRequestStartTime'),
    ('AbandonedSRPNavigation', 'ParseStartTime'),
    ('AbandonedSRPNavigation', 'PreviousBackgroundedTime'),
    ('AbandonedSRPNavigation', 'PreviousHiddenTime'),
    ('AbandonedSRPNavigation', 'RendererProcessInitTime'),
    ('Accessibility.ReadAnything', 'MergedDistillationTime.Failure'),
    ('Accessibility.ReadAnything', 'MergedDistillationTime.Success'),
    ('Accessibility.ReadAnything', 'RulesDistillationTime.Failure'),
    ('Accessibility.ReadAnything', 'RulesDistillationTime.Success'),
    ('Accessibility.Renderer', 'CpuTime.SendPendingAccessibilityEvents'),
    ('Accessibility.ScreenAI', 'Screen2xDistillationTime.Failure'),
    ('Accessibility.ScreenAI', 'Screen2xDistillationTime.Success'),
    ('Actor.Login', 'GetCredentialsLatency'),
    ('AdFrameLoad', 'CpuTime.PeakWindowedPercent'),
    ('AdFrameLoad', 'CpuTime.PreActivation'), ('AdFrameLoad', 'CpuTime.Total'),
    ('AdPageLoad', 'AdCpuTime'),
    ('AdsInterestGroup.AuctionLatency.V2',
     'NumBidsAbortedByBuyerCumulativeTimeout'),
    ('AmpPageLoad', 'SubFrame.InteractiveTiming.FirstInputDelay4'),
    ('AmpPageLoad',
     'SubFrame.InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration'
     ),
    ('AmpPageLoad',
     'SubFrame.InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration2'
     ), ('AppListAppClickData', 'TimeSinceLastClick'),
    ('Blink.FedCm', 'AutoReauthn.TimeFromEmbargoWhenBlocked'),
    ('Blink.FedCm', 'Timing.AccountsDialogShownDuration'),
    ('Blink.FedCm', 'Timing.MismatchDialogShownDuration'),
    ('Blink.FedCm', 'Timing.TurnaroundTime'),
    ('Blink.FedCmIdp', 'Timing.AccountsDialogShownDuration'),
    ('Blink.FedCmIdp', 'Timing.MismatchDialogShownDuration'),
    ('Blink.FedCmIdp', 'Timing.TurnaroundTime'),
    ('Blink.FrameLoader', 'CommitDocumentLoaderTime'),
    ('Blink.HTMLParsing', 'FetchQueuedPreloadsTime'),
    ('Blink.HTMLParsing', 'ParsingTimeMax'),
    ('Blink.HTMLParsing', 'ParsingTimeMin'),
    ('Blink.HTMLParsing', 'ParsingTimeTotal'),
    ('Blink.HTMLParsing', 'PreloadTime'),
    ('Blink.HTMLParsing', 'PrepareToStopParsingTime'),
    ('Blink.HTMLParsing', 'PumpTokenizerTime'),
    ('Blink.HTMLParsing', 'ScanAndPreloadTime'),
    ('Blink.HTMLParsing', 'ScanTime'),
    ('Blink.HTMLParsing', 'YieldedTimeAverage'),
    ('Blink.HTMLParsing', 'YieldedTimeMax'),
    ('Blink.HTMLParsing', 'YieldedTimeMin'),
    ('Blink.PageLoad', 'VisualUpdateDelay'), ('Blink.SVGImage', 'TotalTime'),
    ('Blink.UpdateTime', 'VisualUpdateDelay'),
    ('Blink.UpdateTime', 'VisualUpdateDelayBeginMainFrame'),
    ('BTM.Redirect', 'ClientBounceDelay'),
    ('ChargeEventHistory', 'ChargeEventHistoryDuration'),
    ('ChargeEventHistory', 'ChargeEventHistoryStartTime'),
    ('ChromeOS.WebsiteUsageTime', 'Duration'),
    ('ChromeOSApp.InstalledApp', 'InstallTime'),
    ('ChromeOSApp.UsageTime', 'Duration'),
    ('ChromeOSApp.UsageTimeReusedSourceId', 'Duration'),
    ('Compose.TextElementUsage', 'EditingTime'),
    ('ContextualCueing.CueInteraction', 'ProactiveCueShownDuration'),
    ('ContextualCueing.CueShown', 'ProactiveCueLatencyAfterPageLoad'),
    ('ContextualCueing.NudgeInteraction', 'NudgeLatencyAfterPageLoad'),
    ('ContextualCueing.NudgeInteraction', 'NudgeShownDuration'),
    ('DailyChargeSummary', 'DailySummaryHoldTimeOnAc'),
    ('DailyChargeSummary', 'DailySummaryTimeFullOnAc'),
    ('DailyChargeSummary', 'DailySummaryTimeOnAc'),
    ('DIPS.Redirect', 'ClientBounceDelay'),
    ('DomDistiller.Android.DistillabilityLatency', 'Latency'),
    ('Download.Completed', 'TimeSinceStart'),
    ('Download.Interrupted', 'TimeSinceStart'),
    ('Download.Resumed', 'TimeSinceStart'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.BrowserShutdown'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.EventLogged'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.FirstRedirectReceived'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.LoaderCompleted'),
    ('FetchKeepAliveRequest.WithCategory',
     'TimeDelta.LoaderDisconnectedFromRenderer'),
    ('FetchKeepAliveRequest.WithCategory',
     'TimeDelta.RequestCancelledAfterTimeLimit'),
    ('FetchKeepAliveRequest.WithCategory',
     'TimeDelta.RequestCancelledByRenderer'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.RequestFailed'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.RequestRetried'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.RequestStarted'),
    ('FetchKeepAliveRequest.WithCategory', 'TimeDelta.ResponseReceived'),
    ('FetchKeepAliveRequest.WithCategory',
     'TimeDelta.ThirdOrLaterRedirectReceived'),
    ('Fullscreen.Exit', 'SessionDuration'),
    ('HistoryNavigation', 'FirstInputDelayAfterBackForwardCacheRestore'),
    ('HistoryNavigation', 'ForegroundDurationAfterBackForwardCacheRestore'),
    ('HistoryNavigation', 'TimeSinceNavigatedAwayFromDocument'),
    ('HistoryNavigation',
     'UserInteractionLatencyAfterBackForwardCacheRestore.HighPercentile2.MaxEventDuration'
     ),
    ('HistoryNavigation',
     'WorstUserInteractionLatencyAfterBackForwardCacheRestore.MaxEventDuration2'
     ), ('IOS.ReaderMode.Distiller.Latency', 'Latency'),
    ('IOS.ReaderMode.Heuristic.Latency', 'Latency'),
    ('Lens.Overlay.SessionEnd', 'SessionDuration'),
    ('Lens.Overlay.SessionEnd', 'SessionForegroundDuration'),
    ('Media.BasicPlayback', 'CompletedRebuffersDuration'),
    ('Media.BasicPlayback', 'Duration'),
    ('Media.BasicPlayback', 'MeanTimeBetweenRebuffers'),
    ('Media.BasicPlayback', 'WatchTime'),
    ('Media.BasicPlayback', 'WatchTime.AC'),
    ('Media.BasicPlayback', 'WatchTime.AutoPip'),
    ('Media.BasicPlayback', 'WatchTime.Battery'),
    ('Media.BasicPlayback', 'WatchTime.DisplayFullscreen'),
    ('Media.BasicPlayback', 'WatchTime.DisplayInline'),
    ('Media.BasicPlayback', 'WatchTime.DisplayPictureInPicture'),
    ('Media.BasicPlayback', 'WatchTime.NativeControlsOff'),
    ('Media.BasicPlayback', 'WatchTime.NativeControlsOn'),
    ('Media.EME.CdmMetrics', 'KeySystemDataTime1'),
    ('Media.EME.CdmMetrics', 'KeySystemDataTime2'),
    ('Media.EME.CdmMetrics', 'KeySystemDataTime3'),
    ('Media.WebAudio.AudioContext.AudibleTime', 'AudibleTime'),
    ('Media.WebMediaPlayerState', 'TimeToFirstFrame'),
    ('Media.WebMediaPlayerState', 'TimeToMetadata'),
    ('Media.WebMediaPlayerState', 'TimeToPlayReady'),
    ('Memory.Experimental', 'TimeSinceLastNavigation'),
    ('Memory.Experimental', 'TimeSinceLastVisibilityChange'),
    ('Navigation.FromGoogleSearch.Abandoned', 'PreviousBackgroundedTime'),
    ('Navigation.FromGoogleSearch.Abandoned', 'PreviousHiddenTime'),
    ('Navigation.FromGoogleSearch.Abandoned', 'RendererProcessInitTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'CommitReceivedTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'CommitSentTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'DidCommitTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'DOMContentLoadedTime'),
    ('Navigation.FromGoogleSearch.TimingInformation',
     'FirstContentfulPaintTime'),
    ('Navigation.FromGoogleSearch.TimingInformation',
     'FirstRedirectedRequestStartTime'),
    ('Navigation.FromGoogleSearch.TimingInformation',
     'LargestContentfulPaintTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'LoaderStartTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'LoadEventStartedTime'),
    ('Navigation.FromGoogleSearch.TimingInformation',
     'NonRedirectedRequestStartTime'),
    ('Navigation.FromGoogleSearch.TimingInformation', 'ParseStartTime'),
    ('Navigation.ReceivedResponse', 'NavigationFirstResponseLatency'),
    ('NavigationTimeline', 'BeforeUnloadPhase1Duration'),
    ('NavigationTimeline',
     'BeforeUnloadPhase1ToNavigationRequestCreationDuration'),
    ('NavigationTimeline', 'BeforeUnloadPhase2Duration'),
    ('NavigationTimeline', 'BeforeUnloadPhase2ToBeginNavigationDuration'),
    ('NavigationTimeline', 'BeginNavigationToCommitDuration'),
    ('NavigationTimeline', 'BeginNavigationToLoaderStartDuration'),
    ('NavigationTimeline', 'CommitToDidCommitDuration'),
    ('NavigationTimeline', 'DidCommitToFinishDuration'),
    ('NavigationTimeline', 'FetchStartToReceiveHeadersDuration'),
    ('NavigationTimeline', 'IgnoredCorrectlyDuration'),
    ('NavigationTimeline', 'IgnoredIncorrectlyDuration'),
    ('NavigationTimeline', 'LoaderStartToFetchStartDuration'),
    ('NavigationTimeline', 'LoaderStartToReceiveResponseDuration'),
    ('NavigationTimeline', 'NavigationRequestToBeforeUnloadPhase2Duration'),
    ('NavigationTimeline', 'NavigationRequestToBeginNavigationDuration'),
    ('NavigationTimeline', 'ReceiveHeadersToReceiveResponseDuration'),
    ('NavigationTimeline', 'ReceiveResponseToCommitDuration'),
    ('NavigationTimeline', 'RendererCommitToDidCommitDuration'),
    ('NavigationTimeline', 'StartToBeforeUnloadPhase1Duration'),
    ('NavigationTimeline', 'StartToNavigationRequestCreationDuration'),
    ('NavigationTimeline', 'StartToSyncRendererCommitDuration'),
    ('NavigationTimeline', 'TotalDuration'),
    ('NavigationTimeline', 'TotalExcludingBeforeUnloadDuration'),
    ('Network.DataUrls', 'ParseTime'), ('Notification', 'TimeUntilClose'),
    ('Notification', 'TimeUntilFirstClick'),
    ('Notification', 'TimeUntilLastClick'),
    ('OptimizationGuide', 'NavigationHintsFetchRequestLatency'),
    ('OptimizationGuide.AnnotatedPageContent', 'ExtractionLatency'),
    ('PageForegroundSession', 'ForegroundDuration'),
    ('PageForegroundSession', 'ForegroundTotalInputDelay'),
    ('PageLifecycleMetricsOnNewPageCommit',
     'PageLifecycleEventsTotalProcessingTime'), ('PageLoad', 'CpuTime'),
    ('PageLoad', 'Experimental.TotalForegroundDuration'),
    ('PageLoad', 'InteractiveTiming.FirstInputDelay4'),
    ('PageLoad', 'InteractiveTiming.FirstInputTimestamp4'),
    ('PageLoad', 'InteractiveTiming.FirstScrollDelay'),
    ('PageLoad', 'InteractiveTiming.FirstScrollTimestamp'),
    ('PageLoad', 'InteractiveTiming.INPTime'),
    ('PageLoad',
     'InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration'
     ),
    ('PageLoad',
     'InteractiveTiming.UserInteractionLatencyAtFirstOnHidden.HighPercentile2.MaxEventDuration'
     ),
    ('PageLoad',
     'InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration'),
    ('PageLoad', 'InteractiveTimingBeforeSoftNavigation.INPTime'),
    ('PageLoad',
     'InteractiveTimingBeforeSoftNavigation.UserInteractionLatency.HighPercentile2.MaxEventDuration'
     ), ('PageLoad', 'MainFrameResource.ConnectDelay'),
    ('PageLoad', 'MainFrameResource.DNSDelay'),
    ('PageLoad', 'PageTiming.ForegroundDuration'),
    ('PageLoad', 'PageTiming.TotalForegroundDuration'),
    ('PageLoad', 'PaintTiming.LargestContentfulPaintImageDiscoveryTime'),
    ('PaintPreviewCapture', 'BlinkCaptureTime'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.DelayDefer'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.EffectiveBlocking'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.LongScript.JSCompilation'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.LongScript.JSExecution'),
    ('PerformanceAPI.LongAnimationFrame',
     'Duration.LongScript.JSExecution.EventListeners'),
    ('PerformanceAPI.LongAnimationFrame',
     'Duration.LongScript.JSExecution.PromiseHandlers'),
    ('PerformanceAPI.LongAnimationFrame',
     'Duration.LongScript.JSExecution.ScriptBlocks'),
    ('PerformanceAPI.LongAnimationFrame',
     'Duration.LongScript.JSExecution.UserCallbacks'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.StyleAndLayout.Forced'),
    ('PerformanceAPI.LongAnimationFrame',
     'Duration.StyleAndLayout.RenderPhase'),
    ('PerformanceAPI.LongAnimationFrame', 'Duration.Total'),
    ('PerformanceAPI.LongTask', 'Duration'),
    ('PerformanceAPI.LongTask', 'Duration.V8.Execute'),
    ('PerformanceAPI.LongTask', 'Duration.V8.GC'),
    ('PerformanceAPI.LongTask', 'Duration.V8.GC.Full.Atomic'),
    ('PerformanceAPI.LongTask', 'Duration.V8.GC.Full.Incremental'),
    ('PerformanceAPI.LongTask', 'Duration.V8.GC.Young'),
    ('PerformanceAPI.LongTask', 'StartTime'),
    ('PerformanceManager.FreezingEligibility',
     'HighestCPUAnyIntervalWithoutOptOut'),
    ('PerformanceManager.FreezingEligibility', 'HighestCPUCurrentInterval'),
    ('Permission', 'TimeToDecision'), ('Popup.Closed', 'EngagementTime'),
    ('PowerUsageScenariosIntervalData', 'DeviceSleptDuringInterval'),
    ('PowerUsageScenariosIntervalData', 'TimePlayingVideoInVisibleTab'),
    ('Preloading.Attempt',
     'PrefetchServiceWorkerRegisteredForURLCheckDuration'),
    ('Preloading.Attempt', 'ReadyTime'),
    ('Preloading.Attempt', 'TimeToNextNavigation'),
    ('Preloading.Attempt.PreviousPrimaryPage',
     'PrefetchServiceWorkerRegisteredForURLCheckDuration'),
    ('Preloading.Attempt.PreviousPrimaryPage', 'ReadyTime'),
    ('Preloading.Attempt.PreviousPrimaryPage', 'TimeToNextNavigation'),
    ('Preloading.Prediction', 'TimeToNextNavigation'),
    ('Preloading.Prediction.PreviousPrimaryPage', 'TimeToNextNavigation'),
    ('PrerenderPageLoad', 'InteractiveTiming.FirstInputDelay4'),
    ('PrerenderPageLoad',
     'InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration'
     ),
    ('PrerenderPageLoad',
     'InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration'),
    ('PrerenderPageLoad',
     'InteractiveTimingBeforeSoftNavigation.UserInteractionLatency.HighPercentile2.MaxEventDuration'),
    ('Responsiveness.UserInteraction', 'MaxEventDuration'),
    ('ServiceWorker.MainResourceLoadCompleted', 'CacheLookupTime'),
    ('ServiceWorker.MainResourceLoadCompleted', 'RouterEvaluationTime'),
    ('ServiceWorker.OnLoad', 'TotalCacheLookupTime'),
    ('ServiceWorker.OnLoad', 'TotalRouterEvaluationTime'),
    ('SharedStorage.Worklet.OnDestroyed', 'AbsoluteUsefulResourceDuration'),
    ('SmartCharging', 'DurationOfLastCharge'),
    ('SmartCharging', 'DurationRecentAudioPlaying'),
    ('SmartCharging', 'DurationRecentVideoPlaying'),
    ('SmartCharging', 'TimeSinceLastCharge'),
    ('SoftNavigation', 'InteractiveTiming.INPTime'),
    ('SoftNavigation',
     'InteractiveTiming.UserInteractionLatency.HighPercentile2.MaxEventDuration'
     ),
    ('SoftNavigation', 'PaintTiming.LargestContentfulPaintImageDiscoveryTime'),
    ('SoftNavigation', 'StartTime'),
    ('TabManager.Background.FirstAlertFired', 'TimeFromBackgrounded'),
    ('TabManager.Background.FirstAudioStarts', 'TimeFromBackgrounded'),
    ('TabManager.Background.FirstFaviconUpdated', 'TimeFromBackgrounded'),
    ('TabManager.Background.FirstNonPersistentNotificationCreated',
     'TimeFromBackgrounded'),
    ('TabManager.Background.FirstTitleUpdated', 'TimeFromBackgrounded'),
    ('TabManager.Background.ForegroundedOrClosed', 'TimeFromBackgrounded'),
    ('TabManager.Experimental.SessionRestore.TabSwitchLoadStopped',
     'TabSwitchLoadTime'),
    ('TabManager.SessionRestore.ForegroundTab.ExpectedTaskQueueingDurationInfo',
     'ExpectedTaskQueueingDuration'),
    ('TabManager.TabLifetime', 'TimeSinceNavigation'),
    ('TabRevisitTracker.TabStateChange', 'TimeInPreviousState'),
    ('TabRevisitTracker.TabStateChange', 'TotalTimeActive'),
    ('TouchToFill.TimeToSuccessfulLogin', 'TimeToSuccessfulLogin'),
    ('TranslatePageLoad', 'MaxTimeToTranslate'),
    ('TranslatePageLoad', 'TotalTimeNotTranslated'),
    ('TranslatePageLoad', 'TotalTimeTranslated'),
    ('TrustedWebActivity.Startup', 'TimeToFirstCommitNavigation2.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToFirstCommitNavigation2.Warm'),
    ('TrustedWebActivity.Startup', 'TimeToFirstContentfulPaint.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToFirstContentfulPaint.Warm'),
    ('TrustedWebActivity.Startup', 'TimeToLargestContentfulPaint2.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToLargestContentfulPaint2.Warm'),
    ('TrustedWebActivity.Startup', 'TimeToMarkFullyLoaded.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToMarkFullyLoaded.Warm'),
    ('TrustedWebActivity.Startup', 'TimeToMarkFullyVisible.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToMarkFullyVisible.Warm'),
    ('TrustedWebActivity.Startup', 'TimeToMarkInteractive.Cold'),
    ('TrustedWebActivity.Startup', 'TimeToMarkInteractive.Warm'),
    ('Unload', 'BeforeUnloadDuration'),
    ('Unload', 'BeforeUnloadQueueingDuration'), ('Unload', 'UnloadDuration'),
    ('Unload', 'UnloadQueueingDuration'), ('UserActivity', 'EventLogDuration'),
    ('UserActivity', 'LastActivityTime'),
    ('UserActivity', 'LastUserActivityTime'),
    ('UserActivity', 'RecentTimeActive'),
    ('UserActivity', 'RecentVideoPlayingTime'),
    ('UserActivity', 'ScreenDimDelay'), ('UserActivity', 'ScreenDimToOffDelay'),
    ('UserActivity', 'TimeSinceLastKey'),
    ('UserActivity', 'TimeSinceLastMouse'),
    ('UserActivity', 'TimeSinceLastTouch'),
    ('UserActivity', 'TimeSinceLastVideoEnded'),
    ('V8.GC.FullCycle', 'Duration.MainThread'),
    ('V8.GC.FullCycle', 'Duration.MainThread.Atomic'),
    ('V8.GC.FullCycle', 'Duration.MainThread.Atomic.Cpp'),
    ('V8.GC.FullCycle', 'Duration.MainThread.Cpp'),
    ('V8.GC.FullCycle', 'Duration.SinceLastMarkCompact'),
    ('V8.GC.FullCycle', 'Duration.Total'),
    ('V8.GC.FullCycle', 'Duration.Total.Cpp'),
    ('V8.Wasm.ModuleCompiled', 'WallClockDuration'),
    ('V8.Wasm.ModuleDecoded', 'WallClockDuration'),
    ('V8.Wasm.ModuleInstantiated', 'WallClockDuration'),
    ('V8.Wasm.ModuleTieredUp', 'WallClockDuration'),
    ('VideoConferencingEvent', 'CameraCaptureDuration'),
    ('VideoConferencingEvent', 'MicrophoneCaptureDuration'),
    ('VideoConferencingEvent', 'ScreenCaptureDuration'),
    ('VideoConferencingEvent', 'TotalDuration'),
    ('WebAPK.SessionEnd', 'SessionDuration'),
    ('WebAPK.Uninstall', 'InstalledDuration'),
    ('WebApp.DailyInteraction', 'BackgroundDuration'),
    ('WebApp.DailyInteraction', 'ForegroundDuration'),
    ('WebAuthn.ConditionalUiGetCall', 'TimeSinceDomContentLoaded'),
    ('XR.WebXR.Session', 'Duration')
})


def _is_metric_valid_as_index_field(metric_node):
  """Checks if a given metric node can be used as a field in an index tag.

  Has the following requirements:
    * 'history' is the only aggregation target (no others are considered)
    * there will be at most 1 'aggregation', 1 'history', and 1 'statistic'
      element in a metric element
    * enumerations are the only metric types that are valid

  Args:
    metric_node: A metric node to check.

  Returns: True or False, depending on whethere the given node is valid as an
    index field.
  """
  aggregation_nodes = metric_node.getElementsByTagName('aggregation')
  if aggregation_nodes.length != 1:
    return False

  history_nodes = aggregation_nodes[0].getElementsByTagName('history')
  if history_nodes.length != 1:
    return False

  statistic_nodes = history_nodes[0].getElementsByTagName('statistics')
  if statistic_nodes.length != 1:
    return False

  # Only enumeration type metrics are supported as index fields.
  enumeration_nodes = statistic_nodes[0].getElementsByTagName('enumeration')
  return bool(enumeration_nodes)


def _get_index_fields(metric_node):
  """Get a list of fields from index node descendents of a metric_node."""
  aggregation_nodes = metric_node.getElementsByTagName('aggregation')
  if not aggregation_nodes:
    return []

  history_nodes = aggregation_nodes[0].getElementsByTagName('history')
  if not history_nodes:
    return []

  index_nodes = history_nodes[0].getElementsByTagName('index')
  if not index_nodes:
    return []

  return [index_node.getAttribute('fields') for index_node in index_nodes]


def _get_local_metric_index_fields(metric_node):
  """Gets a set of metric names being used as local-metric index fields."""
  index_fields = _get_index_fields(metric_node)
  local_metric_fields = set()
  for fields in index_fields:
    local_metric_fields.update(LOCAL_METRIC_RE.findall(fields))
  return local_metric_fields


def _split_words_in_metric_name(metric_name: str) -> list[str]:
  """Splits a metric name into lowercase parts each representing a word.

  E.g. 'Something.HTMLParseTimeV20' gets split into
      ['something', 'html', 'parse', 'time', 'v', '20']
  """
  # Metric names are CamelCase possibly containing periods, initialisms, or numbers.
  camel_case_word_matcher = r'[A-Z]?[a-z]+'
  initialisms_or_uppercase_word_matcher = r'[A-Z]+(?=[A-Z][a-z]|\b|\d)'
  numbers_matcher = r'\d+'
  metric_name_pattern = re.compile(f'{camel_case_word_matcher}|'
                                   f'{initialisms_or_uppercase_word_matcher}|'
                                   f'{numbers_matcher}')
  parts = []
  for metric_name_section in metric_name.split('.'):
    parts.extend([
        matched.lower()
        for matched in metric_name_pattern.findall(metric_name_section)
    ])
  return parts


class UkmXmlValidation:
  """Validations for the content of ukm.xml."""

  def __init__(self, ukm_config):
    """
    Args:
      ukm_config: A XML minidom Element representing the root node of the UKM
          config tree.
    """
    self.config = ukm_config

  def check_events_have_owners(self):
    """Check that every event in the config has at least one owner."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      owner_nodes = event_node.getElementsByTagName('owner')

      # Check <owner> tag is present for each event.
      if not owner_nodes:
        errors.append("<owner> tag is required for event '%s'." % event_name)
        continue

      for owner_node in owner_nodes:
        # Check <owner> tag actually has some content.
        if not owner_node.childNodes:
          errors.append(
              "<owner> tag for event '%s' should not be empty." % event_name)
          continue

        for email in owner_node.childNodes:
          # Check <owner> tag's content is an email address, not a username.
          if not ('@chromium.org' in email.data or '@google.com' in email.data):
            errors.append("<owner> tag for event '%s' expects a Chromium or "
                          'Google email address.' % event_name)

    is_success = not errors

    return (is_success, errors)

  def check_metric_type_is_specified(self):
    """Check each metric is either specified with an enum or a unit."""
    errors = []
    warnings = []

    enum_tree = merge_xml.MergeFiles(histogram_paths.ENUMS_XMLS)
    enums, _ = extract_histograms.ExtractEnumsFromXmlTree(enum_tree)

    for event_node in self.config.getElementsByTagName('event'):
      for metric_node in event_node.getElementsByTagName('metric'):
        if metric_node.hasAttribute('enum'):
          enum_name = metric_node.getAttribute('enum')
          # Check if the enum is defined in enums.xml.
          if enum_name not in enums:
            errors.append('Unknown enum %s in UKM event-metric %s:%s.' %
                          (enum_name, event_node.getAttribute('name'),
                          metric_node.getAttribute('name')))
        elif not metric_node.hasAttribute('unit'):
          warnings.append("Warning: Neither 'enum' or 'unit' is specified "
                          'for UKM event-metric %s:%s.'
                          % (event_node.getAttribute('name'),
                             metric_node.getAttribute('name')))

    is_success = not errors
    return (is_success, errors, warnings)

  def check_local_metric_is_aggregated(self):
    """Checks that index fields don't list invalid metrics."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      metric_nodes = event_node.getElementsByTagName('metric')
      valid_index_field_metrics = {
          node.getAttribute('name')
          for node in metric_nodes if _is_metric_valid_as_index_field(node)
      }
      for metric_node in metric_nodes:
        local_metric_index_fields = _get_local_metric_index_fields(metric_node)
        invalid_metrics = local_metric_index_fields - valid_index_field_metrics
        if invalid_metrics:
          event_name = event_node.getAttribute('name')
          metric_name = metric_node.getAttribute('name')
          invalid_metrics_string = ', '.join(sorted(invalid_metrics))
          errors.append(INVALID_LOCAL_METRIC_FIELD_ERROR %(
                          {'event': event_name, 'metric': metric_name,
                           'invalid_metrics': invalid_metrics_string}))

    is_success = not errors
    return (is_success, errors)

  def _get_statistics_error(self, metric_node, event_node):
    """Checks if statistics are nonempty and of valid type."""
    for stats_node in metric_node.getElementsByTagName('statistics'):
      # A node is considered empty if it has no child nodes
      # of the ELEMENT type. Filtering out <statistics/>.
      child_elements = [
          c for c in stats_node.childNodes if c.nodeType == c.ELEMENT_NODE
      ]

      # Checking if tag is nonempty.
      if not child_elements:
        return INVALID_AGGREGATION_STATISTIC_ERROR % (
            {
                'event': event_node.getAttribute('name'),
                'metric': metric_node.getAttribute('name')
            })
      # Checking if tag is of a valid type.
      stat = child_elements[0]
      if (stat.tagName not in VALID_STATISTICS
          or stat.getAttribute('type') not in VALID_STATISTICS[stat.tagName]):
        return INVALID_AGGREGATION_STATISTIC_ERROR % (
            {
                'event': event_node.getAttribute('name'),
                'metric': metric_node.getAttribute('name')
            })

  def check_statistics_non_empty_valid(self):
    """Validates configuration of aggregated metrics."""
    errors = []
    for event_node in self.config.getElementsByTagName('event'):
      for metric_node in event_node.getElementsByTagName('metric'):
        if metric_node.getElementsByTagName('aggregation'):
          validation_error = self._get_statistics_error(metric_node, event_node)
          if validation_error:
            errors.append(validation_error)

    return (not errors, errors)

  def check_metric_names(self) -> tuple[bool, list[str]]:
    """Checks that metrics do not accidentally use reserved keywords."""
    errors = []
    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      for metric_node in event_node.getElementsByTagName('metric'):
        metric_name = metric_node.getAttribute('name')
        if metric_name.lower() in FORBIDDEN_METRIC_NAMES:
          errors.append(
              "Metric name '%s' in event '%s' collides with a UKM-internal "
              'keyword. Please pick a different name.' %
              (metric_name, event_name))

    return (not errors, errors)

  def check_time_metric_unit(self) -> tuple[bool, list[str]]:
    """Checks that metrics for time and duration have a unit in the name."""
    errors = []
    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      for metric_node in event_node.getElementsByTagName('metric'):
        metric_name = metric_node.getAttribute('name')
        name_parts = _split_words_in_metric_name(metric_name)

        has_time_keyword = any(part in TIME_KEYWORDS for part in name_parts)
        if has_time_keyword:
          has_unit = any(part in TIME_UNITS for part in name_parts)
          if not has_unit and (
              event_name, metric_name) not in _TIME_UNIT_EVENT_METRIC_ALLOWLIST:
            errors.append(
                MISSING_TIME_METRIC_UNIT_ERROR % {
                    'event': event_name,
                    'metric': metric_name,
                    'time_keywords': ','.join(sorted(TIME_KEYWORDS)),
                    'time_units': ','.join(sorted(TIME_UNITS)),
                })

    return (not errors, errors)
