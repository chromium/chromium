# LowPriorityAsyncScriptExecution
This suite runs the tests in http/low-priority-async-script-execution/ with
`--enable-features=LCPCriticalPathPredictor,LCPScriptObserver,LowPriorityAsyncScriptExecution:low_pri_async_exec_timeout/100ms/low_pri_async_exec_exclude_lcp_influencers/true`.

See crbug.com/1480143.
