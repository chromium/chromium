from __future__ import print_function

import os
import sys
import subprocess


def main(argv):
    cwd = os.getcwd()
    subprocess_env = os.environ.copy()
    subprocess_args = []

    for arg in argv:
        # if it's env var
        if arg[:5] == '--env':
            equal_pos = arg.index('=')
            key = arg[5:equal_pos]
            value = arg[equal_pos + 1:len(arg)]
            print('Set env variable {}={}'.format(key, value))
            subprocess_env[key] = value
        else:
            # if it's cwd
            if arg[:5] == '--cwd':
                arg_cwd = arg[5:]
                if arg_cwd[:1] == '/':
                    cwd = arg_cwd # absolute path
                else:
                    cwd = os.path.normpath('/'.join((cwd, arg_cwd))) # relative path
                print('Set cwd={}'.format(cwd))
            else:
                # cmd arguments
                subprocess_args += [arg]

    process = subprocess.Popen(subprocess_args, env=subprocess_env,
                               cwd=cwd, stdout=sys.stdout, stderr=sys.stderr)
    process.communicate()
    return process.returncode


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.exit('interrupted')
