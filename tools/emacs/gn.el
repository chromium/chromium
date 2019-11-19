;;; gn-mode.el - A major mode for editing gn files.

;; Copyright 2015 The Chromium Authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; Author: Elliot Glaysher <erg@chromium.org>
;; Created: April 03, 2015
;; Keywords: tools, gn, ninja, chromium

;; This file is not part of GNU Emacs.

;; This is not the official copy. The official copy can be found at:
;; https://gn.googlesource.com/gn/+/master/tools/gn/misc/emacs/gn-mode.el

;;; Commentary:

;; A major mode for editing GN files. GN stands for Generate Ninja. GN is the
;; meta build system used in Chromium. For more information on GN, see the GN
;; manual: <https://chromium.googlesource.com/chromium/src/+/master/tools/gn/README.md>

;;; To Do:

;; - We syntax highlight builtin actions, but don't highlight instantiations of
;;   templates. Should we?



(require 'smie)

(defgroup gn nil
  "Major mode for editing Generate Ninja files."
  :prefix "gn-"
  :group 'languages)

(defcustom gn-indent-basic 2
  "The number of spaces to indent a new scope."
  :group 'gn
  :type 'integer)

(defcustom gn-format-command "gn format --stdin"
  "The command to run to format gn files in place."
  :group 'gn
  :type 'string)

(defgroup gn-faces nil
  "Faces used in Generate Ninja mode."
  :group 'gn
  :group 'faces)

(defface gn-embedded-variable
  '((t :inherit font-lock-variable-name-face))
  "Font lock face used to highlight variable names in strings."
  :group 'gn-faces)

(defface gn-embedded-variable-boundary
  '((t :bold t
       :inherit gn-embedded-variable))
  "Font lock face used to highlight the '$' that starts a
variable name or the '{{' and '}}' which surround it."
  :group 'gn-faces)

(defvar gn-font-lock-reserved-keywords
  '("true" "false" "if" "else"))

(defvar gn-font-lock-target-declaration-keywords
  '("action" "action_foreach" "bundle_data" "copy" "create_bundle" "executable"
    "group" "loadable_module" "shared_library" "source_set" "static_library"
    "target"))

;; pool() is handled specially since it's also a variable name
(defvar gn-font-lock-buildfile-fun-keywords
  '("assert" "config" "declare_args" "defined" "exec_script" "foreach"
    "forward_variables_from" "get_label_info" "get_path_info"
    "get_target_outputs" "getenv" "import" "not_needed" "print"
    "process_file_template" "read_file" "rebase_path" "set_default_toolchain"
    "set_defaults" "set_sources_assignment_filter" "split_list" "template"
    "tool" "toolchain" "propagates_configs" "write_file"))

(defvar gn-font-lock-predefined-var-keywords
  '("current_cpu" "current_os" "current_toolchain" "default_toolchain"
    "host_cpu" "host_os" "invoker" "python_path" "root_build_dir" "root_gen_dir"
    "root_out_dir" "target_cpu" "target_gen_dir" "target_name" "target_os"
    "target_out_dir"))

(defvar gn-font-lock-var-keywords
  '("all_dependent_configs" "allow_circular_includes_from" "arflags" "args"
    "asmflags" "assert_no_deps" "bundle_deps_filter" "bundle_executable_dir"
    "bundle_resources_dir" "bundle_root_dir" "cflags" "cflags_c" "cflags_cc"
    "cflags_objc" "cflags_objcc" "check_includes" "code_signing_args"
    "code_signing_outputs" "code_signing_script" "code_signing_sources"
    "complete_static_lib" "configs" "data" "data_deps" "defines" "depfile"
    "deps" "include_dirs" "inputs" "ldflags" "lib_dirs" "libs" "output_dir"
    "output_extension" "output_name" "output_prefix_override" "outputs" "pool"
    "precompiled_header" "precompiled_header_type" "precompiled_source"
    "product_type" "public" "public_configs" "public_deps"
    "response_file_contents" "script" "sources" "testonly" "visibility"
    "write_runtime_deps" "bundle_contents_dir"))

(defconst gn-font-lock-keywords
  `((,(regexp-opt gn-font-lock-reserved-keywords 'words) .
     font-lock-keyword-face)
    (,(regexp-opt gn-font-lock-target-declaration-keywords 'words) .
     font-lock-type-face)
    (,(regexp-opt gn-font-lock-buildfile-fun-keywords 'words) .
     font-lock-function-name-face)
    ;; pool() as a function
    ("\\<\\(pool\\)\\s-*("
     (1 font-lock-function-name-face))
    (,(regexp-opt gn-font-lock-predefined-var-keywords 'words) .
     font-lock-constant-face)
    (,(regexp-opt gn-font-lock-var-keywords 'words) .
     font-lock-variable-name-face)
    ;; $variables_like_this
    ("\\(\\$\\)\\([a-zA-Z0-9_]+\\)"
     (1 'gn-embedded-variable-boundary t)
     (2 'gn-embedded-variable t))
    ;; ${variables_like_this}
    ("\\(\\${\\)\\([^\n }]+\\)\\(}\\)"
     (1 'gn-embedded-variable-boundary t)
     (2 'gn-embedded-variable t)
     (3 'gn-embedded-variable-boundary t))
    ;; {{placeholders}}    (see substitute_type.h)
    ("\\({{\\)\\([^\n }]+\\)\\(}}\\)"
     (1 'gn-embedded-variable-boundary t)
     (2 'gn-embedded-variable t)
     (3 'gn-embedded-variable-boundary t))))

(defun gn-smie-rules (kind token)
  "These are slightly modified indentation rules from the SMIE
  Indentation Example info page. This changes the :before rule
  and adds a :list-intro to handle our x = [ ] syntax."
  (pcase (cons kind token)
    (`(:elem . basic) gn-indent-basic)
    (`(,_ . ",") (smie-rule-separator kind))
    (`(:list-intro . "") gn-indent-basic)
    (`(:before . ,(or `"[" `"(" `"{"))
     (if (smie-rule-hanging-p) (smie-rule-parent)))
    (`(:before . "if")
     (and (not (smie-rule-bolp)) (smie-rule-prev-p "else")
          (smie-rule-parent)))))

(defun gn-fill-paragraph (&optional justify)
  "We only fill inside of comments in GN mode."
  (interactive "P")
  (or (fill-comment-paragraph justify)
      ;; Never return nil; `fill-paragraph' will perform its default behavior
      ;; if we do.
      t))

(defun gn-run-format ()
  "Run 'gn format' on the buffer in place."
  (interactive)
  ;; We can't `save-excursion' here; that will put us at the beginning of the
  ;; shell output, aka the beginning of the document.
  (let ((my-start-line (line-number-at-pos)))
    (shell-command-on-region (point-min) (point-max) gn-format-command nil t)
    (goto-line my-start-line)))

(defvar gn-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "\C-c\C-f" 'gn-run-format)
    map))

;;;###autoload
(define-derived-mode gn-mode prog-mode "GN"
  "Major mode for editing gn (Generate Ninja)."
  :group 'gn

  (setq-local comment-use-syntax t)
  (setq-local comment-start "#")
  (setq-local comment-end "")
  (setq-local indent-tabs-mode nil)

  (setq-local fill-paragraph-function 'gn-fill-paragraph)

  (setq-local font-lock-defaults '(gn-font-lock-keywords))

  ;; For every 'rule("name") {', adds "name" to the imenu for quick navigation.
  (setq-local imenu-generic-expression
              '((nil "^\s*[a-zA-Z0-9_]+(\"\\([a-zA-Z0-9_]+\\)\")\s*{" 1)))

  (smie-setup nil #'gn-smie-rules)
  (setq-local smie-indent-basic gn-indent-basic)

  ;; python style comment: “# …”
  (modify-syntax-entry ?# "< b" gn-mode-syntax-table)
  (modify-syntax-entry ?\n "> b" gn-mode-syntax-table)
  (modify-syntax-entry ?_ "w" gn-mode-syntax-table))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.gni?\\'" . gn-mode))

(provide 'gn-mode)
