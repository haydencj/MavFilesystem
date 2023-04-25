mfs: mfs.c
	gcc mfs.c -o mfs -g -Wall -Werror

clean:
	rm ./mfs