Request for points back on the following test cases -

testcase test01
difference between my output and expected output
10,14c10,14
< Child5(): Sleep done at time 10990098
< Child4(): Sleep done at time 10990140
< Child3(): Sleep done at time 10990180
< Child2(): Sleep done at time 10990215
< Child1(): Sleep done at time 10990259
---
> Child3(): Sleep done at time 10990098
> Child1(): Sleep done at time 10990140
> Child5(): Sleep done at time 10990180
> Child4(): Sleep done at time 10990215
> Child2(): Sleep done at time 10990259

Output only differs with order of waking up sleeping processes, due to using a heap rather than a linked list

testcase test02
difference between my output and expected output
23,24d22
< Child9(): After sleeping 3 seconds, difference in system clock is 3309627
< Child9(): After sleeping 3 seconds, difference in system clock is 3309668
26c24,27
< Child9(): After sleeping 3 seconds, difference in system clock is 3309747
---
> Child9(): After sleeping 3 seconds, difference in system clock is 3309825
> Child9(): After sleeping 3 seconds, difference in system clock is 3309665
> Child9(): After sleeping 3 seconds, difference in system clock is 3309706
> Child9(): After sleeping 3 seconds, difference in system clock is 3309822
28,32c29,32
< Child9(): After sleeping 3 seconds, difference in system clock is 3309823
< Child9(): After sleeping 3 seconds, difference in system clock is 3309864
< Child9(): After sleeping 3 seconds, difference in system clock is 3309904
< Child9(): After sleeping 3 seconds, difference in system clock is 3309948
< Child9(): After sleeping 3 seconds, difference in system clock is 3309991
---
> Child9(): After sleeping 3 seconds, difference in system clock is 3309881
> Child9(): After sleeping 3 seconds, difference in system clock is 3309867
> Child9(): After sleeping 3 seconds, difference in system clock is 3309837
> Child9(): After sleeping 3 seconds, difference in system clock is 3309969

Output only differs with order of waking up sleeping processes and time by a few microseconds, due to using 
a heap rather than a linked list

testcase test20
difference between my output and expected output
20d19
< Child_21(): writing to term1
21a21
> Child_21(): writing to term1
40d39
< Child_23(): writing to term3
41a41
> Child_23(): writing to term3

Output only differs by order in which stuff is being written to terminals, though all terminals recieve the 
correct messages

testcase test22
difference between my output and expected output
17,18d16
< buffer written 'two: second line
< '
19a18,19
> '
> buffer written 'two: second line

Output only differs in order of which buffers are written to terminal, due to different implementation 
