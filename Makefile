CC = g++

CFLAGS = -pg -g -Wall -std=c++14 -mpopcnt -march=native

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

all: int_test count_req_test

int_test : int_test.cpp hashtable.h
	g++ $(CFLAGS) -Ofast -o int_test int_test.cpp  

count_req_test : count_req_test.cpp hashtable.hashtable
	g++ $(CFLAGS) -Ofast -o count_req_test count_req_test.cpp  



clean:
	rm -f int_test
	rm -f count_req_test