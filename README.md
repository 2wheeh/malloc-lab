#### (Explicit으로 제출)
### **Explicit** 

- **LIFO** 방식 으로 연결리스트 구현
- **First-Fit** (LIFO top 부터 탐색)
- **Coalescing** 발생 시 이미 **연결 리스트에 존재하는 free block을 리스트에서 빼지 않고 무조건 리스트 내에 남아서**, 새로 free 된 block이 붙는 방식으로 구현 
-> 연결 리스트 bottom에 더 큰 block, top에 더 작은 block이 배치 
-> **first-fit**으로 **best-fit**에 더 근접한 util 성능 기대했음

- heap checker : for debugging

```
#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

***********
Main Files:
***********

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

**********************************
Other support files for the driver
**********************************

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h
```
