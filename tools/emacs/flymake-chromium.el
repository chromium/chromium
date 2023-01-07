;; Copyright 2011 The Chromium Authors
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; Set up flymake for use with chromium code.  Uses ninja (since none of the
;; other chromium build systems have latency that allows interactive use).
;;
;; Requires a modern emacs (GNU Emacs >= 23) and that gyp has already generated
;; the build.ninja file(s).  See defcustoms below for settable knobs.


(require 'flymake)

(defcustom cr-flymake-ninja-build-file "out/Debug/build.ninja"
  "Relative path from chromium's src/ directory to the
  build.ninja file to use.")

(defcustom cr-flymake-ninja-executable "ninja"
  "Ninja executable location; either in $PATH or explicitly given.")

(defun cr-flymake-absbufferpath ()
  "Return the absolute path to the current buffer, or nil if the
  current buffer has no path."
  (when buffer-file-truename
      (expand-file-name buffer-file-truename)))

(defun cr-flymake-chromium-src ()
  "Return chromium's src/ directory, or nil on failure."
  (let ((srcdir (locate-dominating-file
                 (cr-flymake-absbufferpath) cr-flymake-ninja-build-file)))
    (when srcdir (expand-file-name srcdir))))

(defun cr-flymake-string-prefix-p (prefix str)
  "Return non-nil if PREFIX is a prefix of STR (23.2 has string-prefix-p but
  that's case insensitive and also 23.1 doesn't have it)."
  (string= prefix (substring str 0 (length prefix))))

(defun cr-flymake-current-file-name ()
  "Return the relative path from chromium's src/ directory to the
  file backing the current buffer or nil if it doesn't look like
  we're under chromium/src/."
  (when (and (cr-flymake-chromium-src)
             (cr-flymake-string-prefix-p
              (cr-flymake-chromium-src) (cr-flymake-absbufferpath)))
    (substring (cr-flymake-absbufferpath) (length (cr-flymake-chromium-src)))))

(defun cr-flymake-from-build-to-src-root ()
  "Return a path fragment for getting from the build.ninja file to src/."
  (replace-regexp-in-string
   "[^/]+" ".."
   (substring
    (file-name-directory
     (file-truename (or (and (cr-flymake-string-prefix-p
                              "/" cr-flymake-ninja-build-file)
                             cr-flymake-ninja-build-file)
                        (concat (cr-flymake-chromium-src)
                                cr-flymake-ninja-build-file))))
    (length (cr-flymake-chromium-src)))))

(defun cr-flymake-getfname (file-name-from-error-message)
  "Strip cruft from the passed-in filename to help flymake find the real file."
  (file-name-nondirectory file-name-from-error-message))

(defun cr-flymake-ninja-command-line ()
  "Return the command-line for running ninja, as a list of strings, or nil if
  we're not during a save"
  (unless (buffer-modified-p)
    (list cr-flymake-ninja-executable
          (list "-C"
                (concat (cr-flymake-chromium-src)
                        (file-name-directory cr-flymake-ninja-build-file))
                (concat (cr-flymake-from-build-to-src-root)
                        (cr-flymake-current-file-name) "^")))))

(defun cr-flymake-kick-off-check-after-save ()
  "Kick off a syntax check after file save, if flymake-mode is on."
  (when flymake-mode (flymake-start-syntax-check)))

(defadvice next-error (around cr-flymake-next-error activate)
  "If flymake has something to say, let it say it; otherwise
   revert to normal next-error behavior."
  (if (not flymake-err-info)
      (condition-case msg
          ad-do-it
        (error (message "%s" (prin1-to-string msg))))
    (flymake-goto-next-error)
    ;; copy/pasted from flymake-display-err-menu-for-current-line because I
    ;; couldn't find a way to have it tell me what the relevant error for this
    ;; line was in a single call:
    (let* ((line-no (flymake-current-line-no))
           (line-err-info-list
            (nth 0 (flymake-find-err-info flymake-err-info line-no)))
           (menu-data (flymake-make-err-menu-data line-no line-err-info-list)))
      (prin1 (car (car (car (cdr menu-data)))) t))))

(defun cr-flymake-find-file ()
  "Enable flymake, but only if it makes sense, and immediately
  disable timer-based execution."
  (when (and (not flymake-mode)
             (not buffer-read-only)
             (cr-flymake-current-file-name))
    ;; Since flymake-allowed-file-name-masks requires static regexps to match
    ;; against, can't use cr-flymake-chromium-src here.  Instead we add a
    ;; generic regexp, but only to a buffer-local version of the variable.
    (set (make-local-variable 'flymake-allowed-file-name-masks)
         (list (list "\\.c\\(\\|c\\|pp\\)"
                     'cr-flymake-ninja-command-line
                     'ignore
                     'cr-flymake-getfname)))
    (flymake-find-file-hook)
    (if flymake-mode
        (when flymake-timer (cancel-timer flymake-timer))
      (kill-local-variable 'flymake-allowed-file-name-masks))))

(defun cr-compile ()
  "Run the interactive compile command with the working directory
  set to src/."
  (interactive)
  (let ((default-directory (cr-flymake-chromium-src)))
    (call-interactively 'compile)))

(add-hook 'find-file-hook 'cr-flymake-find-file 'append)
(add-hook 'after-save-hook 'cr-flymake-kick-off-check-after-save)

;; Show flymake infrastructure ERRORs in hopes of fixing them.  Set to 3 for
;; DEBUG-level output from flymake.el.
(setq flymake-log-level 0)

(provide 'flymake-chromium)
