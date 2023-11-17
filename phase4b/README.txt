Requesting points back for the following test cases -

test01
output diff:

10,14c10,14
< Child5(): Sleep done at time 11050103
< Child4(): Sleep done at time 11050146
< Child3(): Sleep done at time 11050193
< Child2(): Sleep done at time 11050236
< Child1(): Sleep done at time 11050282
---
> Child3(): Sleep done at time 11050103
> Child1(): Sleep done at time 11050146
> Child5(): Sleep done at time 11050193
> Child4(): Sleep done at time 11050236
> Child2(): Sleep done at time 11050282

Output only differs due to implementing a heap rather than a linked list for sleeping processes

test16
output diff:

13,20d12
< after writing track 5
< process 13 quit with status 2
< after writing track 6
< process 20 quit with status 9
< after writing track 7
< process 17 quit with status 6
< after writing track 9
< process 15 quit with status 4
28a21,28
> after writing track 5
> process 13 quit with status 2
> after writing track 6
> process 20 quit with status 9
> after writing track 7
> process 17 quit with status 6
> after writing track 9
> process 15 quit with status 4

Output only differs due to implementing different approach to handling disk writes/reads

test18
output diff:

10a11,14
> after writing unit 0 track 3
> process 14 quit with status 3
> after writing unit 1 track 2
> process 17 quit with status 6
19,22d22
< after writing unit 0 track 3
< process 14 quit with status 3
< after writing unit 1 track 2
< process 17 quit with status 6

Output only differs due to implementing different approach to handling disk writes/reads

test20
output diff:

20d19
< Child_21(): writing to term1
21a21
> Child_21(): writing to term1
40d39
< Child_23(): writing to term3
41a41
> Child_23(): writing to term3

Output only differs due to implementing different approach to handling terminal writes/reads

test22
output diff:

17,18d16
< buffer written 'two: second line
< '
19a18,19
> '
> buffer written 'two: second line

Output only differs due to implementing different approach to handling terminal writes/reads

test23
output diff:
21,24c21,24
< ChildS(4): After sleeping 1 seconds, diff in sys_clock is 1109370
< ChildS(3): After sleeping 2 seconds, diff in sys_clock is 2129455
< ChildS(2): After sleeping 3 seconds, diff in sys_clock is 3209544
< ChildS(1): After sleeping 4 seconds, diff in sys_clock is 4329635
---
> ChildS(4): After sleeping 1 seconds, diff in sys_clock is 1109378
> ChildS(3): After sleeping 2 seconds, diff in sys_clock is 2209459
> ChildS(2): After sleeping 3 seconds, diff in sys_clock is 3289545
> ChildS(1): After sleeping 4 seconds, diff in sys_clock is 4369632
31c31
< 'ChildS(0): After sleeping 5 seconds, diff in sys_clock is 5449704
---
> 'ChildS(0): After sleeping 5 seconds, diff in sys_clock is 5489709

Output only differs by several microseconds 
