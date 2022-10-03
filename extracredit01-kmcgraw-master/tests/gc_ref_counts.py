# -m 10008

# Simple test: one garbage value refers to non-garbage
non_garbage = 123
garbage = [0, non_garbage]
garbage[0] = garbage
# output 184 bytes in use; 6 refs in use
mem()
del garbage
# output 184 bytes in use; 6 refs in use
mem()
gc()
# output 104 bytes in use; 4 refs in use
mem()
# output 123
print(non_garbage)
del non_garbage
# output 72 bytes in use; 3 refs in use
mem()

# More complex test: multiple garbage values refer to each other and non-garbage
a = {}
b = {"prev": a}
a["next"] = b
c = {"prev": b, "next": a}
b["next"] = c
a["prev"] = c
a["value"] = "aaa"
b["value"] = [a["value"], "bbb"]
c["value"] = b["value"]
values1 = c["value"]
values2 = values1
# output aaa
print(a["value"])
# output ["aaa", "bbb"]
print(b["value"])
# output ["aaa", "bbb"]
print(c["value"])
# output 1224 bytes in use; 25 refs in use
mem()
del a
del b
del c
# output 1224 bytes in use; 25 refs in use
mem()
gc()
# output 216 bytes in use; 7 refs in use
mem()
# output ["aaa", "bbb"] ["aaa", "bbb"]
print(values1, values2)
del values1
# output 216 bytes in use; 7 refs in use
mem()
del values2
# output 72 bytes in use; 3 refs in use
mem()
gc()
# output 72 bytes in use; 3 refs in use
mem()
