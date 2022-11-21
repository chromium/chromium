// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to run.
// `go run update_readme.go.` at //third_party/libgav1.
// README.chromium is updated with the correct info.
package main

import (
	"fmt"
	"os/exec"
	"strings"
)

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
	updateReadme()
}
