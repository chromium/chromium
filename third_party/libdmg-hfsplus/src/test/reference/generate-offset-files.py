from random import choice
from string import ascii_uppercase, ascii_lowercase, digits
import sys

choices = list()
choices.extend(ascii_uppercase)
choices.extend(ascii_lowercase)
choices.extend(digits)
for c in ("x", "X", "y", "Y", "z", "Z"):
    choices.remove(c)

for i in range(int(sys.argv[1])):
    print("".join(choice(choices) for i in range(8)))
