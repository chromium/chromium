// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to run.
// `go run generate_libgav1_src_gni.go.` at //third_party/libgav1.
// libgav1_src.gni is generated.
package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
)

const (
	gniFile      = "libgav1_srcs.gni"
	commonPrefix = "//third_party/libgav1/"
	srcDir       = "./src/src"
	header       = `# This file is generated. Do not edit.`
)

func getCppFiles(dir string) []string {
	files, err := ioutil.ReadDir(dir)
	if err != nil {
		panic(err)
	}

	var paths []string
	for _, file := range files {
		if file.IsDir() {
			paths = append(paths, getCppFiles(filepath.Join(dir, file.Name()))...)
		}
		ext := filepath.Ext(file.Name())
		if ext == ".cc" || ext == ".h" {
			paths = append(paths, filepath.Join(dir, file.Name()))
		}
	}
	return paths
}

func getTopDirs(dir string) []string {
	files, _ := ioutil.ReadDir(dir)
	var paths []string
	for _, file := range files {
		if file.IsDir() {
			paths = append(paths, filepath.Join(dir, file.Name()))
		}
	}
	return paths
}

func format(dir string, files []string, file *os.File) {
	sourcesName := "gav1_" + dir + "_sources"
	fmt.Fprintf(file, "\n%s = [\n", sourcesName)
	for _, f := range files {
		fmt.Fprintf(file, "  \"%s%s\",\n", commonPrefix, f)
	}
	fmt.Fprintf(file, "]\n")
}

func updateReadme() {
	gitCmd := exec.Command("bash", "-c", "git --no-pager log -1 --format=\"%cd%n%H\" --date=format:\"%A %B %d %Y\"")
	gitCmd.Dir = "src"
	out, err := gitCmd.Output()
	if err != nil {
		panic(fmt.Sprintf("failed to execute git command: %v", err))
	}

	vals := strings.Split(string(out), "\n")

	if len(vals) < 2 {
		panic(fmt.Sprintf("unexpected git log result: %v %v", vals))
	}
	date := vals[0]
	hash := vals[1]

	sedCmd := exec.Command("sed", "-E", "-i.back", "-e",
		fmt.Sprintf("s/^(Date:)[[:space:]]+.*$/\\1 %s/", date), "-e",
		fmt.Sprintf("s/^(Commit:)[[:space:]]+[a-f0-9]{40}/\\1 %s/", hash),
		"README.chromium")
	if err := sedCmd.Run(); err != nil {
		panic(fmt.Sprintf("failed to execute sed command: %v %v", sedCmd, err))
	}

	rmCmd := exec.Command("rm", "README.chromium.back")
	if rmCmd.Run() != nil {
		panic(fmt.Sprintf("failed to execute rm command: %v", err))
	}
}

func main() {
	files := getCppFiles(srcDir)
	topDirs := getTopDirs(srcDir)
	m := make(map[string][]string)
	for _, f := range files {
		found := false
		for _, d := range topDirs {
			if strings.HasPrefix(f, d) {
				var bd string
				for _, asm := range []string{"sse4"} {
					pattern := "*_" + asm + "*"
					if match, err := filepath.Match(pattern, filepath.Base(f)); err != nil {
						panic(err)
					} else if match {
						bd = filepath.Base(d) + "_" + asm
						break
					}
				}
				if bd == "" {
					bd = filepath.Base(d)
				}

				// Split the dsp headers out to their own variables as the
				// assembly may depend on both its headers and the top-level
				// headers.
				if strings.HasPrefix(bd, "dsp") && filepath.Ext(f) == ".h" {
					m[bd+"_headers"] = append(m[bd+"_headers"], f)
				} else {
					m[bd] = append(m[bd], f)
				}
				found = true
				break
			}
		}
		if !found {
			m["common"] = append(m["common"], f)
		}
	}

	if err := os.RemoveAll(gniFile); err != nil {
		panic(err)
	}

	file, err := os.OpenFile(gniFile, os.O_WRONLY|os.O_CREATE, 0666)
	if err != nil {
		panic(err)
	}
	fmt.Fprintf(file, "%s\n", header)

	var keys []string
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)

	for _, k := range keys {
		v := m[k]
		format(k, v, file)
	}
	file.Close()

	gnCmd := exec.Command("gn", "format", gniFile)
	if gnCmd.Run() != nil {
		panic(fmt.Sprintf("failed to execute gn format command: %v", err))
	}

	updateReadme()
}
