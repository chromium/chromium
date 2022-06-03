This tool is designed to measure and aggregate a few key performance metrics
for all Chrome processes over time. The tool supports other browsers such as
Edge or Firefox, or in fact any other processes. The inclusion of processes
is based on a simple partial process image name match against the image name
argument (optional, chrome.exe is the default image name used when no
argument is provided).

# Compilation
Open IdleWakeups.sln in Visual Studio and select Build > Build Solution (F7).
IdleWakeups.exe can then be found in src/tools/win/IdleWakeups/x64/Debug/.

# Usage
`IdleWakeups.exe` to match all Chrome processes.

`IdleWakeups.exe Firefox` to match all Firefox processes.

`IdleWakeups.exe msedge` to match all Edge processes.

`IdleWakeups.exe 12345` to match process ID 12345 and its child processes.

The process matching the provided parameter is identified by case-sensitive
string prefix, e.g., `some_process` and `some_process.exe` would both work. If
the parameter is numeric and a valid process ID, it is treated as a process ID.

When the tool starts it begins gathering and aggregating CPU usage, private
commit, number of context switches / sec, power usage, and handle counts for all
matched processes. Hit Ctrl-C to stop the measurements and print average and
median values over the entire measurement interval.

By default, CPU usage is normalized to one CPU core, with 100% meaning one CPU
core is fully utilized.

[Intel Power Gadget](https://software.intel.com/en-us/articles/intel-power-gadget-20)
is required to allow IdleWakeups tool to query power usage.

Optional command-line flags:

* `--cpu-seconds`: display CPU time in seconds rather than by percentage.

* `--stop-on-exit`: stop measurement automatically when target process exits.

* `--tabbed`: output summary only, in tab-delimited format; useful for copying
  to a spreadsheet.