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
CPPFLAGS = -g -Wall -Wextra -Werror -std=c++17
LDFLAGS = -lpthread

.PHONY: all

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.cpp csapp.h
	$(CPPC) $(CPPFLAGS) -c proxy.cpp

proxy: proxy.o csapp.o
	$(CPPC) $(CPPFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; \
	cd ..; \
	tar --exclude='tiny' -czvf proxylab-handin.tar.gz proxylab-handout)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

