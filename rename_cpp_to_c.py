#!/usr/bin/env python3
import os
root = os.path.abspath(os.path.dirname(__file__))
renamed = []
for dirpath, dirnames, filenames in os.walk(root):
    for name in filenames:
        if name.endswith('.cpp'):
            old = os.path.join(dirpath, name)
            new = os.path.splitext(old)[0] + '.c'
            os.replace(old, new)
            renamed.append((old, new))
            print('RENAMED', old, '->', new)
backup = os.path.join(root, 'userland', 'servers', 'gpu_server.cpp.bak')
if os.path.exists(backup):
    os.remove(backup)
    print('REMOVED', backup)
print('TOTAL RENAMED', len(renamed))
