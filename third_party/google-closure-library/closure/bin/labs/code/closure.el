;; Copyright The Closure Library Authors. All Rights Reserved.
;;
;; Licensed under the Apache License, Version 2.0 (the "License");
;; you may not use this file except in compliance with the License.
;; You may obtain a copy of the License at
;;
;;      http://www.apache.org/licenses/LICENSE-2.0
;;
;; Unless required `by applicable law or agreed to in writing, software
;; distributed under the License is distributed on an "AS-IS" BASIS,
;; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;; See the License for the specific language governing permissions and
;; limitations under the License.

;; Closure JS code editing functions for emacs.

;; Author: nnaze@google.com (Nathan Naze)

;; Remember the path of this file, as we will base our paths on it.
(setq closure-el-path load-file-name)

(defun closure-el-directory ()
  "Get the directory the closure.el file lives in."
  (file-name-directory closure-el-path))

(defun closure-generate-jsdoc-path()
  "The path of the generate_jsdoc.py script."
  (concat (closure-el-directory) "generate_jsdoc.py"))

(defun closure-insert-jsdoc ()
  "Insert JSDoc for the next function after the cursor."
  (interactive)
  (save-excursion ; Remembers cursor location
    (call-process-region
     (point) (point-max)
     (closure-generate-jsdoc-path)
     t t)))

(provide 'closure)
