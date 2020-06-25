CC = g++

CFLAGS = -pg -g -Wall -std=c++14 -mpopcnt -march=native

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

all: int_test count_req_test hello_hash

int_test : int_test.cc cuckoohashtable.hh
	g++ $(CFLAGS) -Ofast -o int_test int_test.cc

count_req_test : count_req_test.cc cuckoohashtable.hh
	g++ $(CFLAGS) -Ofast -o count_req_test count_req_test.cc  

hello_hash : hello_hash.cpp cuckoohashtable.h
	g++ $(CFLAGS) -Ofast -o hello_hash hello_hash.cpp  

clean:
	rm -f int_test
	rm -f count_req_test
	rm -f hello_hash