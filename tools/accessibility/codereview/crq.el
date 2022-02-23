;;; TODO(plundblad):
;;; - Allow adding comments outside of the diff directly from a target
;;;   file.
;;; - Hide hunks with only imports etc.
;;; - Hide whitespace-only changes.
;;; - Fix path depotization to not assume google3 or eng.

;;; This elisp file adds a mode based on diff-mode to add comments to specific
;;; lines of files in the diff.  It is intended to be used with files produced
;;; by g4 diff (it requires the target files to exist).  In a diff file,
;;; enable this mode with crq-mode.  You can also run g4 diff
;;; and enable crq mode with the command crq-review.
;;; Then press Enter on any line in the diff
;;; to open up a comment at the end of the file annotated with filename and
;;; line number.
;;;
;;; When done, enter any overall comments right below the "=== COMMENTS ==="
;;; line.  The first line can be "LGTM" or "FYI" to add a vote.  Then
;;; use the command crq-publish to upload and publish your comments to
;;; critique.

(require 'diff-mode)
(require 'p4-google)

(defconst crq-comments-header "=== COMMENTS ===\n")
(defconst crq-file-separator
  "========================================================================\n")
(defconst crq-file-format
  (concat crq-file-separator "File %s\n"))
(defconst crq-line-format
  "------------------------------------\nLine %d: %s\n\n")
(defconst mch-program
  (expand-file-name "mph.py"
                    (file-name-directory load-file-name)))

(defconst download-program
  (expand-file-name "download_issue"
                    (file-name-directory load-file-name)))


(define-derived-mode crq-mode diff-mode "Crq"
  "Special diff mode for code reviews
\\{crq-mode-map}"
  (crq-ensure-header)
  (crq-install-diff-map))

(define-key crq-mode-map "\C-c\C-p" 'crq-publish)
(defun crq-review (change-list-number)
  "Run download script asynchronously on CHANGE-LIST-NUMBER (a string) and
activate crq-mode in the resulting buffer."
  (interactive (list (read-change-list)))
  (let ((process
         (start-process "Gerrit Review" "gerrit-review" download-program change-list-number)))
    (set-process-sentinel process 'crq-diff-sentinel)
    (message "Gerrit download...")))

; TODO(plundblad): Perhaps run the python script asynchronously.
(defun crq-publish (arg)
  "Publishes the comments in the current buffer to critique."
  (interactive "P")
  (goto-char (point-max))
  (let ((result (call-process-region (point-min) (point-max) mch-program nil
                                     (current-buffer) t
                                     "/dev/stdin")))
    (if (not (zerop result))
        (progn
          (message "Error running mch"))))
  t)

(defun crq-comment ()
  (interactive)
  (let* ((loc (diff-find-source-location nil t))
	 (buf (nth 0 loc))
	 (pos (nth 2 loc))
	 (src (nth 3 loc))
	 (offset (+ (car pos) (cdr src)))
	 (line
	  (with-current-buffer buf
	    (save-excursion
	      (goto-char offset)
	      (thing-at-point 'line))))
	 (filename (buffer-file-name buf))
	 (lineno
	  (with-current-buffer buf
            (save-excursion
              (goto-char offset)
              ; count-lines counts the current line if point is not
              ; at the beginning of the line.
              (beginning-of-line)
              (1+ (count-lines (point-min) (point)))))))
    (crq-ensure-header)
    (crq-add-comment
     (file-relative-name filename) lineno line)))

(defun crq-ensure-header ()
  (save-excursion
    (goto-char (point-min))
    (if (not (search-forward crq-comments-header nil t))
      (progn
	(goto-char (point-max))
	(insert ?\n crq-comments-header
		?\n crq-file-separator)))))

(defvar crq-diff-map
  (let ((map (make-sparse-keymap)))
    (define-key map "\r" 'crq-comment)
    map))

(defun crq-install-diff-map ()
  (let* ((end
	 (save-excursion
	   (goto-char (point-max))
	   (unless (search-backward crq-comments-header nil t)
	     (error "No comment header"))
	   (point)))
	 (overlay (make-overlay (point-min) end)))
    (overlay-put overlay 'keymap crq-diff-map)))


(defun crq-fix-filename (fullname)
  (cond
   ((string-match "/\\(google3\\|eng\\)/.*$" fullname)
    (concat "//depot" (match-string 0 fullname)))
   ((string-match "/\\(a\\|b\\)/.*$" fullname)
    (substring (match-string 0 fullname) 3))
   (t fullname)))

(defun crq-trim-line (line)
  (if (string-match "^\\s-*\\(.*?\\)\\s-*$" line)
      (match-string 1 line)
    line))

(defun crq-add-comment (filename lineno text)
  (push-mark)
  (goto-char (point-max))
  (forward-line -1)
  (beginning-of-line)
  (unless (looking-at crq-file-separator)
    (error "Garbage at the end of the file"))
  (insert (format crq-file-format filename))
  (insert (format crq-line-format lineno
		  (crq-trim-line text)))
  (forward-line -1))

(defun crq-diff-sentinel (process event)
  (if (string= event "finished\n")
      (progn
        (message "Downloaded from gerrit")
        (pop-to-buffer (process-buffer process))
        (crq-mode)
        (goto-char (point-min))
        (search-forward "\t" nil t))
    (progn
      (message "g4 diff %s" event)
      (ding))))

(provide 'crq)
