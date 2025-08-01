import os

for root, dirs, files in os.walk('bpio'):
    for file in files:
        if file.endswith('.py'):
            print("import bpio." + os.path.splitext(file)[0] + " as " + os.path.splitext(file)[0])