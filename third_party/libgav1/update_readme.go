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
)

func updateReadme() {
	gitCmd := exec.Command("bash", "-c", "git --no-pager log -1 --format=format:\"%H\"")
	gitCmd.Dir = "src"
	out, err := gitCmd.Output()
	if err != nil {
		panic(fmt.Sprintf("failed to execute git command: %v", err))
	}

	hash := string(out)

	sedCmd := exec.Command("sed", "-E", "-i.back", "-e",
		fmt.Sprintf("s/^(Revision:)[[:space:]]+[a-f0-9]{40}/\\1 %s/", hash),
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
