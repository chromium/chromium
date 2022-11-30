import subprocess
sp = subprocess.Popen(['cargo', 'run', '--example', 'hello'], stdout=subprocess.PIPE)
stdout = sp.communicate()[0].decode("utf-8")
lines = stdout.splitlines()
assert lines == ["life before main", "main"], "got %s" % lines
print("Test Passed")
