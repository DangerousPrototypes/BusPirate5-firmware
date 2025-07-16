import os

for root, dirs, files in os.walk('.'):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            print(os.path.join(root, file))
