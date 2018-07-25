import os
import sys
import shutil


def main(argv):
    directory = argv[0]
    if os.path.exists(directory):
        shutil.rmtree(directory)


if __name__ == '__main__':
    try:
        main(sys.argv[1:])
    except KeyboardInterrupt:
        sys.exit('interrupted')
