# -m 10015

d = {0: 0, 1: 1}
# output {0: 0, 1: 1}
print(d)
i = 2
while i <= 40:
    d[i] = d[i - 2] + d[i - 1]
    del d[i - 2]
    i = i + 1
del i
# output 824 bytes in use; 10 refs in use
mem()
# output {39: 63245986, 40: 102334155}
print(d)
gc()
# output 824 bytes in use; 10 refs in use
mem()
del d
# output 72 bytes in use; 3 refs in use
mem()

a = {}
a[True] = a
b = {1: a}
del a
# output {1: {True: {True: {...: ...}}}}
print(b)
# output 584 bytes in use; 10 refs in use
mem()
del b[1]
# output {}
print(b)
# output 552 bytes in use; 9 refs in use
mem()
gc()
# output 312 bytes in use; 6 refs in use
mem()
del b
# output 72 bytes in use; 3 refs in use
mem()
