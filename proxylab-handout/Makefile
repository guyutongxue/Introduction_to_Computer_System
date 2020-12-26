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
CPPFLAGS = -g -Wall -Wextra -std=c++17
LDFLAGS = -pthread

.PHONY: all

all: proxy

# csapp.o: csapp.c csapp.h
# 	$(CC) $(CFLAGS) -c csapp.c
csapp.o: csapp2.cpp csapp2.h
	$(CPPC) ${CPPFLAGS} -c csapp2.cpp -o csapp.o

proxy.o: proxy.cpp csapp2.h
	$(CPPC) $(CPPFLAGS) -c proxy.cpp

proxy: proxy.o csapp.o
	$(CPPC) $(CPPFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; \
	cd ..; \
	tar --exclude='ignore' -czvf proxylab-handin.tar.gz proxylab-handout)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

