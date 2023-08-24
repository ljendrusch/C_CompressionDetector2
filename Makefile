
all: bin/client bin/server

bin/client: utils.c client.c
	gcc -o bin/client utils.c client.c -I.

bin/server: utils.c server.c
	gcc -o bin/server utils.c server.c -I.

clean:
	rm -rf bin
	mkdir bin
