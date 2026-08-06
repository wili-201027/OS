import io

path = r"E:\os\userland\compositor\window_manager.cpp"
with io.open(path, "r", encoding="utf-8") as f:
    c = f.read()

fixes = [
    (
        "for(int i=0;i<16;++i) e->tag[i]=0; for(int i=0;i<32;++i) e->id[i]=0;",
        "for(int i=0;i<16;++i) e->tag[i]=0;\n    for(int i=0;i<32;++i) e->id[i]=0;",
    ),
]

missing = []
for old, new in fixes:
    if old not in c:
        missing.append(old)
    else:
        c = c.replace(old, new)

if missing:
    print("MISSING:", missing)
else:
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(c)
    print("OK")
