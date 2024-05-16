Usage:
git cl<command>[options]

A git-command for integrating reviews on Gerrit.

Commands are:
  [32marchive    [39m archives and deletes branches associated with closed changelists
  [32mbaseurl    [39m gets or sets base-url for this branch
  [32mcheckout   [39m checks out a branch associated with a given Gerrit issue
  [32mcomments   [39m shows or posts review comments for any changelist
  [32mcreds-check[39m checks credentials and suggests changes
  [32mdcommit    [39m dEPRECATED: Used to commit the current changelist via git-svn
  [32mdescription[39m brings up the editor for the current CL's description
  [32mdiff       [39m shows differences between local tree and last upload
  [32mformat     [39m runs auto-formatting tools (clang-format etc.) on the diff
  [32mhelp       [39m prints list of commands or help for a specific command
  [32missue      [39m sets or displays the current code review issue number
  [32mland       [39m commits the current changelist via git
  [32mlint       [39m runs cpplint on the current changelist or given files
  [32mowners     [39m finds potential owners for reviewing
  [32mpatch      [39m applies (cherry-picks) a Gerrit changelist locally
  [32mpresubmit  [39m runs presubmit tests on the current changelist
  [32mset-close  [39m closes the issue
  [32mset-commit [39m sets the commit bit to trigger the CQ
  [32msplit      [39m splits a branch into smaller branches and uploads CLs
  [32mstatus     [39m show status of changelists
  [32mtree       [39m shows the status of the tree
  [32mtry        [39m triggers tryjobs using either Buildbucket or CQ dry run
  [32mtry-results[39m prints info about results for tryjobs associated with the current CL
  [32mupload     [39m uploads the current changelist to codereview
  [32mupstream   [39m prints or sets the name of the upstream branch, if any
  [32mweb        [39m opens the current CL in the web browser

Options:
  --version      show program's version number and exit
  -h, --help     show this help message and exit
  -v, --verbose  Use 2 times for more debugging info
