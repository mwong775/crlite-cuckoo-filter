CC = g++

SOURCES=int_test, count_req_test
INCLUDES=$(wildcard *.h)

CFLAGS = -pg -g -Wall -std=c++14 -mpopcnt -march=native

LDFLAGS+= -Wall -lpthread -lssl -lcrypto

all: int_test count_req_test

int_test : int_test.cpp cuckoohash/cuckoo_hashtable.h
	g++ $(CFLAGS) -Ofast -o int_test int_test.cpp  

count_req_test : count_req_test.cpp cuckoo_hashtable.h cuckoo_config.h cuckoo_hashutil.h bucket_container.h
	g++ $(CFLAGS) -Ofast -o count_req_test count_req_test.cpp  

# This will run clang-format on all the source files and *all* .h files in the directory. 
# It will use .clang-format for formatting rules. Please get the most recent
# .clang-format file, and feel free to add it to your directory. There's no penalty for
# having one copy of .clang-format in each assignment directory.
format:
	clang-format -i $(SOURCES) $(INCLUDES)

clean:
	rm -f int_test
	rm -f count_req_test