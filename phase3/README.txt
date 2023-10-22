Request for points on the following testcases -

TESTCASE test07
diff between expected and our output
11d10
< Child2(): done
13a13
> Child2(): done

Our output only differs in the order in which processes get blocked/unblocked due to difference in Spawn implementation, though everything works functionality wise

TESTCASE test10
diff between expected and our output
6,8c6,8
< Child1a(): current time = 103   Should be close, but does not have to be an exact match
< Child1a(): current time = 109   Should be close, but does not have to be an exact match
< Child1a(): current time = 114   Should be close, but does not have to be an exact match
---
> Child1a(): current time = 67   Should be close, but does not have to be an exact match
> Child1a(): current time = 74   Should be close, but does not have to be an exact match
> Child1a(): current time = 81   Should be close, but does not have to be an exact match
12,14c12,14
< Child1b(): current time = 190   Should be close, but does not have to be an exact match
< Child1b(): current time = 197   Should be close, but does not have to be an exact match
< Child1b(): current time = 202   Should be close, but does not have to be an exact match
---
> Child1b(): current time = 114   Should be close, but does not have to be an exact match
> Child1b(): current time = 120   Should be close, but does not have to be an exact match
> Child1b(): current time = 127   Should be close, but does not have to be an exact match
18,20c18,20
< Child1c(): current time = 285   Should be close, but does not have to be an exact match
< Child1c(): current time = 293   Should be close, but does not have to be an exact match
< Child1c(): current time = 299   Should be close, but does not have to be an exact match
---
> Child1c(): current time = 163   Should be close, but does not have to be an exact match
> Child1c(): current time = 170   Should be close, but does not have to be an exact match
> Child1c(): current time = 175   Should be close, but does not have to be an exact match

Our output differs only by a few microseconds, due to running slightly faster than the provided output

TESTCASE test11
diff between expected and our output
6c6,7
< Child1a(): current time = 75   Should be close, but does not have to be an exact match
---
> Child1a(): current time = 67   Should be close, but does not have to be an exact match
> Child1a(): current time = 74   Should be close, but does not have to be an exact match
8d8
< Child1a(): current time = 86   Should be close, but does not have to be an exact match
12,14c12,14
< Child1b(): current time = 162   Should be close, but does not have to be an exact match
< Child1b(): current time = 169   Should be close, but does not have to be an exact match
< Child1b(): current time = 174   Should be close, but does not have to be an exact match
---
> Child1b(): current time = 114   Should be close, but does not have to be an exact match
> Child1b(): current time = 120   Should be close, but does not have to be an exact match
> Child1b(): current time = 127   Should be close, but does not have to be an exact match
18,20c18,20
< Child1c(): current time = 258   Should be close, but does not have to be an exact match
< Child1c(): current time = 266   Should be close, but does not have to be an exact match
< Child1c(): current time = 272   Should be close, but does not have to be an exact match
---
> Child1c(): current time = 163   Should be close, but does not have to be an exact match
> Child1c(): current time = 170   Should be close, but does not have to be an exact match
> Child1c(): current time = 175   Should be close, but does not have to be an exact match

Our output differs only by a few microseconds, due to running slightly faster than the provided output

TESTCASE test12
diff between expected and our output
6,11c6,11
< Child1a(): current time of day = 103   Should be close, but does not have to be an exact match
< Child1a(): current CPU time =  81     Should be close, but does not have to be an exact match
< Child1a(): current time of day = 114   Should be close, but does not have to be an exact match
< Child1a(): current CPU time =  92     Should be close, but does not have to be an exact match
< Child1a(): current time of day = 127   Should be close, but does not have to be an exact match
< Child1a(): current CPU time = 105     Should be close, but does not have to be an exact match
---
> Child1a(): current time of day =  67   Should be close, but does not have to be an exact match
> Child1a(): current CPU time =  74     Should be close, but does not have to be an exact match
> Child1a(): current time of day =  81   Should be close, but does not have to be an exact match
> Child1a(): current CPU time =  86     Should be close, but does not have to be an exact match
> Child1a(): current time of day =  95   Should be close, but does not have to be an exact match
> Child1a(): current CPU time = 103     Should be close, but does not have to be an exact match
15,20c15,20
< Child1b(): current time of day = 211   Should be close, but does not have to be an exact match
< Child1b(): current CPU time = 191     Should be close, but does not have to be an exact match
< Child1b(): current time of day = 225   Should be close, but does not have to be an exact match
< Child1b(): current CPU time = 206     Should be close, but does not have to be an exact match
< Child1b(): current time of day = 242   Should be close, but does not have to be an exact match
< Child1b(): current CPU time = 222     Should be close, but does not have to be an exact match
---
> Child1b(): current time of day = 133   Should be close, but does not have to be an exact match
> Child1b(): current CPU time = 139     Should be close, but does not have to be an exact match
> Child1b(): current time of day = 147   Should be close, but does not have to be an exact match
> Child1b(): current CPU time = 154     Should be close, but does not have to be an exact match
> Child1b(): current time of day = 163   Should be close, but does not have to be an exact match
> Child1b(): current CPU time = 170     Should be close, but does not have to be an exact match
24,29c24,29
< Child1c(): current time of day = 332   Should be close, but does not have to be an exact match
< Child1c(): current CPU time = 310     Should be close, but does not have to be an exact match
< Child1c(): current time of day = 346   Should be close, but does not have to be an exact match
< Child1c(): current CPU time = 320     Should be close, but does not have to be an exact match
< Child1c(): current time of day = 359   Should be close, but does not have to be an exact match
< Child1c(): current CPU time = 334     Should be close, but does not have to be an exact match
---
> Child1c(): current time of day = 202   Should be close, but does not have to be an exact match
> Child1c(): current CPU time = 211     Should be close, but does not have to be an exact match
> Child1c(): current time of day = 218   Should be close, but does not have to be an exact match
> Child1c(): current CPU time = 225     Should be close, but does not have to be an exact match
> Child1c(): current time of day = 233   Should be close, but does not have to be an exact match
> Child1c(): current CPU time = 242     Should be close, but does not have to be an exact match

Our output differs only by a few microseconds, due to running slightly faster than the provided output

TESTCASE test19
diff between expected and our output
4,5c4,5
< start3(): elapsed time in middle =  7 Should be close, but does not have to be an exact match
< start3(): elapsed time at end    = 16 Should be close, but does not have to be an exact match
---
> start3(): elapsed time in middle =  9 Should be close, but does not have to be an exact match
> start3(): elapsed time at end    = 15 Should be close, but does not have to be an exact match

Our output differs only by a few microseconds, due to running slightly faster than the provided output

TESTCASE test20
diff between expected and our output
13d12
< start3(): After V -- may appear before: Child1(): After P attempt #3
14a14
> start3(): After V -- may appear before: Child1(): After P attempt #3

Our output differs only by a few microseconds, due to running slightly faster than the provided output
