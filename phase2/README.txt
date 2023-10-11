Request for points on the following testcases -

TESTCASE test08
diff between expected and our output:
23a24
> start2(): joined with kid 6, status = 4
27d27
< start2(): joined with kid 6, status = 4

Our output is only different where process 6 is joined by its parent, due to a difference in how processes are unblocked when they are in consumer/producer queues and all of the processes receive the correct messages from the correct mailboxes while returning the correct status

TESTCASE test13
diff between expected and our output:
9c9
< XXp1(): status = 110012   -- Should be near 100k, but does not have to be an exact match.  Is often 90k or so on Russ's computer.
---
> XXp1(): status = 110007   -- Should be near 100k, but does not have to be an exact match.  Is often 90k or so on Russ's computer.
12c12
< XXp1(): status = 210015   -- Should be roughly 100k more than the previous report.
---
> XXp1(): status = 210009   -- Should be roughly 100k more than the previous report.

Our output is only different by a few microseconds in each instance it is checked. The first is near 100k, and the second is roughly 100k more than the previous report, as desired by the output

TESTCASE test25
diff between expected and our output:
29a30
> XXp2(): priority 2, after receipt of message, result = 14   message = 'Third message'
31a33
> XXp3(): join'd with child 8 whose status is 2
33d34
< XXp2(): priority 2, after receipt of message, result = 14   message = 'Third message'
35d35
< XXp3(): join'd with child 8 whose status is 2

Our output is only different in which the order of a process receiving its message along with when the child is joined by its parent, so I believe this should get credit as all processes receive the correct messages along with the correct join status just in a different order

TESTCASE test26
diff between expected and our output:
39,40d38
< XXp2(): priority 3, after sending message, result = 0
< XXp3(): join'd with child 7 whose status is 3
46c44
< XXp2(): priority 4, after sending message, result = 0
---
> XXp2(): priority 3, after sending message, result = 0
48a47,48
> XXp3(): join'd with child 7 whose status is 3
> XXp2(): priority 4, after sending message, result = 0

Our output only differs by the order in which a process is unblocked from being in a queue and every process receives and sends the correct message

TESTCASE test30
diff between expected and our output:
13a14,15
>
> start2(): joined with kid 5, status = 3
18,19d19
<
< start2(): joined with kid 5, status = 3

Our output is only different where process 5 is joined by its parent, due to a difference in how processes are unblocked when they are in consumer/producer queues and all of the processes receive the correct messages from the correct mailboxes while returning the correct status

TESTCASE test31
diff between expected and our output:
15a16,17
> start2(): joined with kid 5, status = 3
>
19,20d20
<
< start2(): joined with kid 5, status = 3

Our output is only different where process 5 is joined by its parent, due to a difference in how processes are unblocked when they are in consumer/producer queues and all of the processes receive the correct messages from the correct mailboxes while returning the correct status

TESTCASE test46
diff between expected and our output:
21a22,23
> Process 5 joined with status: 1
>
24,25d25
<
< Process 5 joined with status: 1

Our output is only different where process 5 is joined by its parent, due to a difference in how processes are unblocked when they are in consumer/producer queues and all of the processes receive the correct messages from the correct mailboxes while returning the correct status
