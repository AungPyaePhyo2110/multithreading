CXX=g++
LD=g++
AR=ar
CXXFLAGS=-std=c++20 -fsanitize=thread -g -Wall -pedantic -O2
SHELL:=/bin/bash
MACHINE=$(shell uname -m)-$(shell echo $$OSTYPE)

all: test

deps:
	g++ -MM *.cpp > Makefile.d

test: solution.o sample_tester.o
	$(LD) $(CXXFLAGS) -o $@ $^ -L./aarch64-linux-gnu -lprogtest_solver -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

lib: progtest_solver.o bigint.o
	mkdir -p $(MACHINE)
	$(AR) cfr $(MACHINE)/libprogtest_solver.a $^

clean:
	rm -f *.o test *~ core sample.tgz Makefile.d

pack: clean
	rm -f sample.tgz
	tar zcf sample.tgz --exclude progtest_solver.cpp --exclude Makefile.mingw --exclude bigint.cpp *


-include Makefile.d
