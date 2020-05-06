CFLAGS = -pg -g -Wall -std=c++14 -mpopcnt -march=native

test : test.cpp cuckoofilter/src/cuckoofilter.h cuckoofilter/src/singletable.h
	g++ $(CFLAGS) -Ofast -o test test.cpp  
