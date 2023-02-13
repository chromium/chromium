# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Use Chromium tracing to associate mojo endpoints with sandboxes.

This script provides a rich view of where mojo interfaces are hosted
and consumed when Chromium is build with mojo tracing enabled, and
provides a coarser view for default builds or packages such as Chrome
Canary.

## With Mojo Tracing Enabled

1. Enable detailed mojo tracing:

gn args ./out/Default
...
enable_mojo_tracing = true
...
autoninja -C ./out/Default chrome

2. Collect a trace:

chrome.exe --trace-startup=-*,disabled-by-default-sandbox,startup,mojom ^
           --trace-startup-file=c:/src/temp/tracing/foo.log ^
           --trace-startup-duration=0

Or visit chrome://tracing and enable categories above, save file.

3. Run this script:

trace-sandbox-viewer.py file.log

trace-sandbox-viewer.py file.json.gz

### Output

{
  "None(Browser)": [
    "(Impl)IPC::mojom::Channel::GetAssociatedInterface",
    "(Impl)IPC::mojom::Channel::Receive",
    "(Impl)IPC::mojom::Channel::SetPeerPid",
    "(Impl)apps::mojom::AppService::RegisterPublisher",
    "(Impl)apps::mojom::AppService::RegisterSubscriber",
    "(Impl)apps::mojom::Publisher::Connect",
    ...
    "data_decoder::mojom::DataDecoderService::BindJsonParser",
    "data_decoder::mojom::JsonParser::Parse",
    "data_decoder::mojom::JsonParser::ParseCallback",
    ...
  ],
  "Network": [
    ...
  ],
  "Renderer": [
    "(Impl)IPC::mojom::Channel::GetAssociatedInterface",
    "(Impl)IPC::mojom::Channel::Receive",
    "(Impl)IPC::mojom::Channel::SetPeerPid",
    "(Impl)autofill::mojom::AutofillAgent::FieldTypePredictionsAvailable",
    "(Impl)autofill::mojom::PasswordAutofillAgent::SetLoggingState",
    "(Impl)blink::mojom::AppCacheFrontend::CacheSelected",
    ...
  ],
  "Utility": [
    "(Impl)IPC::mojom::Channel::SetPeerPid",
    "(Impl)content::mojom::ChildHistogramFetcherFactory::CreateFetcher",
    "(Impl)content::mojom::ChildProcess::BindReceiver",
    "(Impl)content::mojom::ChildProcess::BindServiceInterface",
    "(Impl)content::mojom::ChildProcess::BootstrapLegacyIpc",
    "(Impl)content::mojom::ChildProcess::GetBackgroundTracingAgentProvider",
    "(Impl)content::mojom::ChildProcess::Initialize",
    "(Impl)data_decoder::mojom::DataDecoderService::BindImageDecoder",
    "(Impl)data_decoder::mojom::DataDecoderService::BindJsonParser",
    "(Impl)data_decoder::mojom::ImageDecoder::DecodeImage",
    "(Impl)data_decoder::mojom::ImageDecoder::DecodeImageCallback",
    "(Impl)data_decoder::mojom::JsonParser::Parse",
    "(Impl)data_decoder::mojom::JsonParser::ParseCallback",
    ...
  ],
}
```

## Default Builds

This script is also useful for default builds of Chromium but will
show interfaces in both hosting and consuming processes (as mojom
tracing is limited). Run using the same arguments as above.

### Output.

Note interfaces appear in both host and client contexts:

```
{
  "None(Browser)": [
    "IPC Channel",
    "apps.mojom.AppService",
    "apps.mojom.Publisher",
    ...
    "data_decoder.mojom.DataDecoderService",
    "data_decoder.mojom.JsonParser",
    ...
  ],
  "Utility": [
    "IPC Channel",
    "content.mojom.ChildHistogramFetcherFactory",
    "content.mojom.ChildProcess",
    "data_decoder.mojom.DataDecoderService",
    "data_decoder.mojom.JsonParser",
    ...
  ],
  ...
  "Renderer": [
    "IPC Channel",
    "blink.mojom.AppCacheFrontend",
    ...
    "network.mojom.URLLoaderClient",
    "safe_browsing.mojom.PhishingModelSetter",
    "safe_browsing.mojom.SafeBrowsing",
    ...
  ],
  ...
}
```

"""

import argparse
import gzip
import json
import sys


def guess_open_file(filename):
    if filename.endswith('.gz'):
        return gzip.open(filename, mode='rb')
    else:
        return open(filename, 'rb')


def read_json_events(files, categories):
    events = []
    for filename in files:
        with guess_open_file(filename) as fp:
            objs = json.load(fp)
            for event in objs['traceEvents']:
                if event['cat'] in categories:
                    events.append(event)
    return events


def assign_interfaces_to_sandboxes(events):
    # Returns SandboxType:set(Interfaces).
    intmap = {}
    # Running map of PID:SandboxType
    pidmap = {}
    # running map of PID:ProcName
    procmap = {}
    # Assumes events are sorted by timestamp.
    for event in events:
        # Don't yet know how to remove processes when they finish
        # (i.e. which event to match).
        if event['cat'] == 'mojom':
            pid = event['pid']
            interface = event['name']
            # If there is a sandbox, use that.
            if pid in pidmap:
                sbox = pidmap[pid]
                if not sbox in intmap:
                    intmap[sbox] = set()
                intmap[sbox].add(interface)
            # Otherwise if we saw a process, use that.
            elif pid in procmap:
                sbox = "None(" + procmap[pid] + ")"
                if not sbox in intmap:
                    intmap[sbox] = set()
                intmap[sbox].add(interface)
        elif event['cat'] == '__metadata' and event['name'] == 'process_name':
            pid = event['pid']
            proc = event['args']['name']
            procmap[pid] = proc
        elif event['cat'] == 'disabled-by-default-sandbox':
            pid = int(event['args']['policy']['processIds'][0])
            sbox = event['args']['sandboxType']
            pidmap[pid] = sbox
    return intmap


def output_as_json(interfaces):
    dumpable = {}
    for host in interfaces:
        dumpable[host] = sorted(interfaces[host])
    return json.dumps(dumpable, indent=2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='*', help='file file...')
    args = parser.parse_args()
    if len(args.files) < 1:
        print("Need at least one file")
        return 1

    events = read_json_events(
        args.files,
        categories=['mojom', 'disabled-by-default-sandbox', '__metadata'])
    interfaces = assign_interfaces_to_sandboxes(events)

    print(output_as_json(interfaces))


if __name__ == '__main__':
    sys.exit(main())
