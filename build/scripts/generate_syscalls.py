# build/scripts/generate_syscalls.py
syscalls = [
    ('write', 1),
    ('read', 2),
    ('exit', 3),
]

with open('syscall_table.S','w') as f:
    f.write('.global syscall_table\n')
    f.write('syscall_table:\n')
    for name, num in syscalls:
        f.write(f'    .quad {name}\n')
