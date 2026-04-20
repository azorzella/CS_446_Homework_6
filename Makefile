fromscratch:
	make clean
	make myfs

myfs: myfs.o
	gcc -Wall -o myfs myfs.o -g

myfs.o: myfs.c
	gcc -Wall -c myfs.c -g

clean:
	rm -f *.o
	rm -f myfs

run:
	make fromscratch
	clear
	./myfs

test:
	make fromscratch
	clear
	./myfs > actual.txt
	./test.sh