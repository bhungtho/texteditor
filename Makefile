te: te.c
	$(CC) te.c -o te -g -Wall -Wextra -pedantic -std=c99

clean:
	-rm te.exe