import pathlib,sys,math
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for n in (4,8,12):
    for layer in range(2):
        with (out/f'grid{n}_{layer}.obj').open('w') as f:
            for j in range(n):
                for i in range(n):
                    z=layer*.075+.003*math.sin(i+j)
                    f.write(f'v {i*.25+layer*.025:.9f} {j*.25+layer*.031:.9f} {z:.9f}\n')
            for j in range(n-1):
                for i in range(n-1):
                    a=j*n+i+1;b=a+1;c=a+n;d=c+1
                    f.write(f'f {a} {b} {d}\nf {a} {d} {c}\n')
