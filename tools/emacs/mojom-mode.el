;; Copyright 2023 The Chromium Authors
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

(require 'cc-mode)
(require 'imenu)

(defgroup mojom nil
  "Major mode for editing mojom files."
  :prefix "mojom-"
  :group 'languages)

;; Syntax highlighting directives are based on the Mojom IDL grammar:
;; https://chromium.googlesource.com/chromium/src/+/master/mojo/public/tools/bindings#grammar-reference

;; Based on <Identifier> in the grammar.
(defconst mojom-regexp-identifier "[a-zA-Z_][0-9a-zA-Z_]*")
;; Invented to match fully-qualified type names like `foo.bar.Baz`.
(defconst mojom-regexp-qualified-type
  (concat "\\(" mojom-regexp-identifier "\\.\\)*\\(?2:" mojom-regexp-identifier "\\)\\??"))
(defconst mojom-regexp-whitespace "[\s\n]+")

(defconst mojom-idl-builtins
  '(
    ;; <NumericType> in the grammar.
    "bool"
    "int8" "int16" "int32" "int64"
    "uint8" "uint16" "uint32" "uint64"
    "float" "double"
    ;; <Array> in the grammar.
    "array"
    ;; <Map> in the grammar.
    "map"
    ;; <HandleType> in the grammar.
    "handle"
    ;; <SpecificHandleType> in the grammar.
    "message_pipe"
    "shared_buffer"
    "data_pipe_consumer"
    "data_pipe_producer"
    "platform"
    ;; These are part of the language, but not documented in the grammar.
    "pending_remote"
    "pending_receiver"
    "pending_associated_remote"
    "pending_associated_receiver"
    "string"
    ))

(defconst mojom-idl-keywords
  '(
    ;; <ModuleStatement> in the grammar.
    "module"
    ;; <ImportStatement> in the grammar.
    "import"
    ;; <Interface> in the grammar.
    "interface"
    ;; <Struct> in the grammar.
    "struct"
    ;; <Union> in the grammar.
    "union"
    ;; <Enum> in the grammar.
    "enum"
    ;; <Const> in the grammar.
    "const"
    ;; <Feature> in the grammar.
    "feature"
    ))

(defconst mojom-idl-constants
  '(
    ;; <Literal> in the grammar.
    "true" "false"  "default"))

(defconst mojom-font-lock-keywords
  `(
    ;; <ModuleStatement> in the grammar.
    (,(concat "module\s+\\(" mojom-regexp-qualified-type "\\)")
     1 font-lock-string-face)

    ;; Function names.
    (,(concat "\\(" mojom-regexp-identifier "\\)\s*(")
      1 font-lock-function-name-face)

    ;; Punctuation.
    (,(regexp-opt '("=>" "?" "<" ">")) . font-lock-negation-char-face)

    (,(regexp-opt mojom-idl-builtins 'words) . font-lock-builtin-face)
    (,(regexp-opt mojom-idl-keywords 'words) . font-lock-keyword-face)
    (,(regexp-opt mojom-idl-constants 'words) . font-lock-constant-face)

    ;; Match types followed by identifiers, respectively highlighting them as
    ;; font-lock-type-face and font-lock-variable-name-face. For types that have
    ;; dotted module paths, only highlight the final element of the name. For
    ;; example, in `foo.bar.Baz baz`, we only want to highlight "Baz" as a type.
    (,(concat
       "\\(" mojom-regexp-qualified-type "\\|<" mojom-regexp-qualified-type ">\\)"
       mojom-regexp-whitespace
       mojom-regexp-identifier)
     2 font-lock-type-face)
    (,(concat
       "\\(" mojom-regexp-qualified-type "\\|<" mojom-regexp-qualified-type ">\\)"
       mojom-regexp-whitespace
       "\\(?3:" mojom-regexp-identifier "\\)"
       )
     3 font-lock-variable-name-face)))

(define-derived-mode mojom-mode c++-mode "Mojom"
  "Major mode for editing mojom files."
  :group 'mojom

  ;; Ensure that underscores are considered part of a word. This was necessary
  ;; to ensure that "handle" in "foo_handle_bar" isn't highlighted as a builtin.
  (modify-syntax-entry ?_ "w")

  ;; Completely replace c++-mode's font-lock configuration.
  (setq-local font-lock-defaults '(mojom-font-lock-keywords))

  ;; Configure indentation.
  (setq-local indent-tabs-mode nil)
  (setq-local tab-width 2)

  ;; Configure basic imenu functionality. This dumps everything at the same
  ;; level, but reuses the line's indentation as the imenu label for a visual.
  ;; We are not actually parsing the file, so this will inevitably produce
  ;; incomplete and incorrect results in some circumstances.
  ;;
  ;; TODO(dmcardle) Either add support for enum values or drop support for
  ;; fields. Maybe the latter would be more useful as an index anyway.
  (setq-local imenu-generic-expression
              `(
                ;; Definitions of interfaces, structs, enums, and unions.
                (nil
                 ,(concat "^\s*\\(interface\\|struct\\|enum\\|union\\)"
                          mojom-regexp-whitespace
                          mojom-regexp-identifier)
                 0)
                ;; Definitions of fields.
                (nil
                 ,(let ((maybe-attribute-section "\\(\\[[^]]*\\]\\)?")
                        (type-maybe-parameterized "[A-Za-z0-9_.,<>\s]+\\??")
                        (maybe-ordinal-value "\\(@[0-9]+\\)?")
                        (maybe-whitespace "[\s\n]*"))
                    (concat
                     "^\s*"
                     maybe-attribute-section maybe-whitespace
                     type-maybe-parameterized mojom-regexp-whitespace
                     mojom-regexp-identifier maybe-whitespace
                     maybe-ordinal-value maybe-whitespace
                     ";"))
                 0)
                ;; Function names. An identifier followed by an open parenthesis.
                (nil ,(concat "^\s*" mojom-regexp-identifier "[\s\n]*(") 0)))

  (imenu-add-menubar-index))

(add-to-list 'auto-mode-alist '("\\.mojom\\'" . mojom-mode))

(provide 'mojom-mode)
