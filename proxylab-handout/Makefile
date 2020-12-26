#
# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific 
# Makefile to build your proxy from sources.
#
CC = gcc
CPPC = g++
CFLAGS = -g -Wall
CPPFLAGS = -g -Wall -Wextra -std=c++17 -O3
LDFLAGS = -pthread -lcsapp

.PHONY: all

all: proxy

# csapp.o: csapp.c csapp.h
# 	$(CC) $(CFLAGS) -c csapp.c
libcsapp.a: csapp.o
	ar rv libcsapp.a csapp.o

csapp.o: csapp2.cpp csapp2.h
	$(CPPC) $(CPPFLAGS) -c csapp2.cpp -o csapp.o

cache.o: cache.cpp cache.h
	$(CPPC) $(CPPFLAGS) -c cache.cpp

proxy.o: proxy.cpp cache.h csapp2.h
	$(CPPC) $(CPPFLAGS) -c proxy.cpp

proxy: proxy.o csapp.o cache.o libcsapp.a
	$(CPPC) $(CPPFLAGS) -L. proxy.o cache.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; \
	cd ..; \
	tar --exclude='ignore' --exclude='.noproxy' --exclude='.proxy' \
		-czvf proxylab-handin.tar.gz proxylab-handout)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

