// Copyright 2015 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"encoding/base64"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"net/url"
	"path"
	"strings"
	"time"

	"google.golang.org/appengine/v2"
	"google.golang.org/appengine/v2/memcache"
	"google.golang.org/appengine/v2/urlfetch"
)

func main() {
	appengine.Main()
}

func init() {
	http.HandleFunc("/", handler)
}

func handler(w http.ResponseWriter, r *http.Request) {
	const (
		baseURL             = "https://chromium.googlesource.com/crashpad/crashpad/+/"
		mainBaseURL         = baseURL + "main/"
		generatedDocBaseURL = baseURL + "doc/doc/generated/?format=TEXT"
		bugBaseURL          = "https://bugs.chromium.org/p/crashpad/"
	)

	redirectMap := map[string]string{
		"/":                                mainBaseURL + "README.md",
		"/bug":                             bugBaseURL,
		"/bug/":                            bugBaseURL,
		"/bug/new":                         bugBaseURL + "issues/entry",
		"/doc/developing.html":             mainBaseURL + "/doc/developing.md",
		"/doc/status.html":                 mainBaseURL + "/doc/status.md",
		"/index.html":                      mainBaseURL + "README.md",
		"/man":                             mainBaseURL + "doc/man.md",
		"/man/":                            mainBaseURL + "doc/man.md",
		"/man/catch_exception_tool.html":   mainBaseURL + "tools/mac/catch_exception_tool.md",
		"/man/crashpad_database_util.html": mainBaseURL + "tools/crashpad_database_util.md",
		"/man/crashpad_handler.html":       mainBaseURL + "handler/crashpad_handler.md",
		"/man/exception_port_tool.html":    mainBaseURL + "tools/mac/exception_port_tool.md",
		"/man/generate_dump.html":          mainBaseURL + "tools/generate_dump.md",
		"/man/index.html":                  mainBaseURL + "doc/man.md",
		"/man/on_demand_service_tool.html": mainBaseURL + "tools/mac/on_demand_service_tool.md",
		"/man/run_with_crashpad.html":      mainBaseURL + "tools/run_with_crashpad.md",
	}

	ctx := appengine.NewContext(r)
	client := urlfetch.Client(ctx)

	destinationURL, exists := redirectMap[r.URL.Path]
	if exists {
		http.Redirect(w, r, destinationURL, http.StatusFound)
		return
	}

	if strings.HasPrefix(r.URL.Path, "/bug/") {
		http.Redirect(w, r, bugBaseURL+"issues/detail?id="+r.URL.Path[5:], http.StatusFound)
		return
	}

	// Don’t show dotfiles.
	if strings.HasPrefix(path.Base(r.URL.Path), ".") {
		http.Error(w, http.StatusText(http.StatusNotFound), http.StatusNotFound)
		return
	}

	u, err := url.Parse(generatedDocBaseURL)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	u.Path = path.Join(u.Path, r.URL.Path)
	urlStr := u.String()

	item, err := memcache.Get(ctx, urlStr)
	if err == memcache.ErrCacheMiss {
		resp, err := client.Get(urlStr)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			w.WriteHeader(resp.StatusCode)
			for k, v := range w.Header() {
				w.Header()[k] = v
			}
			io.Copy(w, resp.Body)
			return
		}

		// Redirect directories to their index pages (/doc/ -> /doc/index.html).
		if resp.Header.Get("X-Gitiles-Object-Type") == "tree" {
			http.Redirect(w, r, path.Join(r.URL.Path, "/index.html"), http.StatusFound)
			return
		}

		decoder := base64.NewDecoder(base64.StdEncoding, resp.Body)
		b, err := ioutil.ReadAll(decoder)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		item = &memcache.Item{
			Key:        urlStr,
			Value:      b,
			Expiration: 1 * time.Hour,
		}
		if err := memcache.Set(ctx, item); err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
	} else if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", contentType(path.Base(u.Path)))
	fmt.Fprintf(w, "%s", item.Value)
}

// contentType returns the appropriate content type header for file.
func contentType(file string) string {
	contentTypes := map[string]string{
		".html": "text/html; charset=UTF-8",
		".css":  "text/css; charset=UTF-8",
		".js":   "text/javascript; charset=UTF-8",
		".png":  "image/png",
		".ico":  "image/x-icon",
	}
	for suffix, typ := range contentTypes {
		if strings.HasSuffix(file, suffix) {
			return typ
		}
	}
	return "text/plain; charset=UTF-8"
}
