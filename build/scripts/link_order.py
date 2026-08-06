# build/scripts/link_order.py
import os

objs = []
for root, _, files in os.walk('../kernel'):
    for f in files:
        if f.endswith('.o'):
            objs.append(os.path.join(root,f))

with open('link_order.ld','w') as f:
    f.write('SECTIONS {\n')
    for o in objs:
        f.write(f'    KEEP({o})\n')
    f.write('}\n')
