CFLAGS = -pg -g -Wall -std=c++14 -mpopcnt -march=native

test : test.cpp cuckoo-hashtable/hashtable.h
	g++ $(CFLAGS) -Ofast -o test test.cpp  
