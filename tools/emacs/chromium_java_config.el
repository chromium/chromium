;; Chromium Java indentation is slightly different from Google java
;; indentation this config should help you adjust the conflict add
;; following to the init.el:
;;     (setq-default chrome-root "/path/to/chrome/src/")
;;     (add-to-list 'load-path (concat chrome-root "tools/emacs"))
;;     (require 'chromium_java_config)
;; The depths 100 is critical to override some configurations
(add-hook 'java-mode-hook (lambda ()
                            (setq c-basic-offset 4
                                  tab-width 4
                                  indent-tabs-mode t)) 100)

;; If you prefer to have directory level configuation. You can
;; consider using the below configuration instead.
;; This define some local special settings which won't apply
;; globally, the add-hook with lowest priority ensure it is
;; lastly added, therefore will be applied regardless.
;; (dir-locals-set-class-variables
;;  'chromium-java-setting
;;  '((nil . ((fill-column . 100)))
;;    (java-mode . ((c-basic-offset . 4)
;;                  (tab-width . 4)
;;                  (indent-tabs-mode . t)))))

;;
;; This is part we apply the right directory for the directory
;; settings. We take one level up in case you store your secondary
;; chromium checkout in the parent directory
;; (dir-locals-set-directory-class
;;  (file-name-directory
;;   (directory-file-name
;;    (concat
;;     (or (getenv "CLANKIUM_SRC") (getenv "CHROMIUM_SRC")))))
;;  'chromium-java-setting)

(provide 'chromium_java_config)
