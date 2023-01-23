# Updating WPR Recorded Stories

## Preparation (Android only)
You will need a device with USB debugging enabled, connected to your computer. In addition:

- If you only have one such device connected, you can simply run

```
export ID=android
```


- If you have multiple such devices connected, you’ll need to pick one to record sites, find its serial number from the system settings (Settings > About Phone > Model & Hardware) and then execute

```
# Verify that the device has connected to your workstation successfully: its
# serial number must be listed below and have "device" next to it:
third_party/catapult/devil/bin/deps/linux2/x86_64/bin/adb devices
...
<serial> device
...


# Set mobile device ID:
export $ID=<serial>
```

# To update or add a story

1. Look in [The WPR Playbook (Googlers Only)](http://go/wpr-playbook) to see if the story has any maintenance instructions defined. If it doesn’t, create an entry and fill it out (you will likely update it as you record).
2. If you are updating a story, find the story class definition in [`src/tools/perf/page_sets`](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/page_sets/) (codesearching the test name will likely be easiest).
Copy the story class and add this year as the suffix to the class and the story name.
Example: `CnnStory2019` and `NAME='browse:news:cnn:2019'`
Set the tag for this year: `TAGS = [ ... , story_tags.YEAR_2019]`

## Guided Process (Recommended)

The following command will guide you through all of the steps required to record, validate, and upload a new recording.

* NOTE: BSS stands for Benchmark or Story Set. If you’re recording a system health benchmark, use `desktop_system_health_story_set` or `mobile_system_health_story_set`. 
Otherwise, use the actual name of the benchmark.

```
# Desktop:
tools/perf/update_wpr --story="$NAME" --benchmark-or-story-set="$BSS" auto
# Mobile:
tools/perf/update_wpr --story="$NAME" --device-id=$ID --benchmark-or-story-set="$BSS" auto
```
## Manual Process (via update_wpr)

You can run specific steps from the guided process described above with the following commands:

### Run on a live site

```
# Desktop:
tools/perf/update_wpr --story="$NAME" --benchmark-or-story-set="$BSS" live
# Mobile:
tools/perf/update_wpr --story="$NAME" --device-id=$ID --benchmark-or-story-set="$BSS" live
```


### Record the story into a new WPR archive

```
# Desktop:
tools/perf/update_wpr --story="$NAME" --benchmark-or-story-set="$BSS" record
# Mobile:
tools/perf/update_wpr --story="$NAME" --device-dd=$ID --benchmark-or-story-set="$BSS" record
```

If the story requires login, then you need to add `SKIP_LOGIN = False` to the story class definition while recording and remove it after recording (crbug.com/882479). In order to make the WPR archive more robust, temporarily add pauses using `action_runner.Wait(10)` between the user interactions like page navigation and at the end of the story.


### Replay using the recorded WPR

```
# Desktop:
tools/perf/update_wpr --story="$NAME" --benchmark-or-story-set="$BSS" replay
# Mobile:
tools/perf/update_wpr --story="$NAME" --device-dd=$ID --benchmark-or-story-set="$BSS" replay
```

Check that the console:error:all metrics have low values and are similar to the live run.

### Handling missing URLs

If there are missing files or broken request you can try to manually add static files to the existing archive. Typically you have to ignore requests to ad networks as they include many randomly generated query parameters or domain names, which causes the replay server to miss.
Note that this does work well only for static files, as there is currently no way to specify cookies. You will find the $WPR_ARCHIVE in the system_health_{desktop/mobile}.json file. See the catapult instructions on how to install the required go packages.

```
go run third_party/catapult/web_page_replay_go/src/httparchive.go add "$WPR_ARCHIVE" "$WPR_ARCHIVE" url1 url2...
```

After updating the archive, go back to the previous replay step and iterate.

### Uploading a new WPR archive

```
# Desktop:
tools/perf/update_wpr --story="$NAME" upload
# Mobile:
tools/perf/update_wpr --story="$NAME" --device-dd=$ID upload
```

### Final Steps

Commit all changes and upload a CL with the following description:

```
[perf] Add $NAME system health story

Bug:878390
```

Run a pinpoint job and check that there are the same low number of `console:error:all` metrics.

```
# Desktop:
tools/perf/update_wpr --story="$NAME" --pageset-repeat=20 pinpoint
# Mobile:
tools/perf/update_wpr --story="$NAME" --pageset-repeat=20 --device-dd=$ID pinpoint
```

Or manually test the newly created story by using a pinpoint job

```
# Desktop
Bot: linux-perf
Benchmark: system_health.common_desktop
Story: $NAME
Extra Test Arguments: --pageset-repeat=20

# Mobile
Bot: Android Pixel2 Perf
Benchmark: system_health.common_mobile
Story: $NAME
Extra Test Arguments: --pageset-repeat=20
```

Once the pinpoint jobs finish, check the console:error:* metrics. The task log output will also contain the specific error messages.

If everything looks good, send the CL to browser-perf-engprod@google.com for review.

## Manual Process (using record_wpr, run_benchmark, and upload_to_google_storage.py)

To run a story on a live site:

```
# Desktop:
./tools/perf/run_benchmark run system_health.memory_desktop --browser-executable=$(pwd)/out/Release/chrome --browser=exact --output-format=html --show-stdout --reset-results --use-live-sites --story-filter="$NAME"

# Mobile:
./tools/perf/run_benchmark run system_health.memory_mobile --browser-executable=$(pwd)/out/Release/apks/ChromePublic.apk --browser=exact --device=$YOUR_DEVICE_ID --output-format=html --show-stdout --reset-results --use-live-sites --story-filter="$NAME"
```


Adjust the story definition if the run fails and try again.

To record a story:

```
# Desktop:
tools/perf/record_wpr --browser-executable=$(pwd)/out/Release/chrome --story-filter="$NAME" desktop_system_health_story_set

# Mobile:
tools/perf/record_wpr --device=YOUR_DEVICE_ID --browser-executable=$(pwd)/out/Release/apks/ChromePublic.apk --story-filter="$NAME" mobile_system_health_story_set
```

In order to reduce HTTP 404 errors, add `action_runner.Wait(10)` commands in story interaction before page navigations and the story end.

To test a recording:

- same as "To run a story on a live site" but without the --use-live-sites
- Assert that the console:error:all metrics have low values
- To assess flakiness use --pageset-repeat=10

To upload a recording:

Look up the wpr version number $NNN in the associated .json file (example - tools/perf/page_sets/data/system_health_desktop.json):

```
# Desktop:
upload_to_google_storage.py --bucket chrome-partner-telemetry  tools/perf/page_sets/data/system_health_desktop_${NNN}.wprgo

# Mobile:
upload_to_google_storage.py --bucket chrome-partner-telemetry  tools/perf/page_sets/data/system_health_mobile_${NNN}.wprgo
```

Running this command will generate a .sha1 file. Add this to your CL.
