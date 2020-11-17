temp: temp.objdump
.PHONY: temp

temp.objdump: temp.o
	objdump -d temp.o > temp.objdump

temp.o: temp.s
	gcc -c temp.s