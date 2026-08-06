# build/scripts/symbol_map.py
import subprocess, sys
elf = sys.argv[1]
out = subprocess.check_output(['x86_64-elf-objdump','-t', elf]).decode()
with open(elf+'.sym','w') as f:
    f.write(out)
