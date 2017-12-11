Project 4 - CISC 340
Kelsey Faulise and Josh Johnson

Files:
simcache.c: This is the file that has that code to run the simulator. 


Makefile: This is the makefile that will compile the simulator code above. It also will remove the files when you are completed using the project. 


Test Cases:

We have 3 different tests, each are tested using a fully associative cache, a directly mapped cache, and a set associative cache. These are to be used to fully test to make sure that with different types of caches the simulator will still run smoothly. 

Test 1:

This test case consists of a few loads and stores and is meant to test the cache of its ability to use and perform store words. This is important because store word is a privitol function and has to function properly throughout the simulator.

Test 2:

This test case is meant to test the caches ability to use LRU and to load different values into the cache. This test is mainly ment for testing eviction and loading.

Test 3:

This test case is included to test consecutive loads, then add the values, then to store the added values.



