CC = g++
CFLAGS = -Wall -O3
LDLIBS = -lpthread -lcrypto

build: clean
build:
	mkdir -p build dist
build: main client crypto link

main: main.o link
client: client.o link
crypto: crypto.o link

main.o:
	$(CC) $(CFLAGS) -c main.cpp -o build/main.o
client.o:
	$(CC) $(CFLAGS) -c client.cpp -o build/client.o
crypto.o:
	$(CC) $(CFLAGS) -c crypto.cpp -o build/crypto.o
link:
	$(CC) $(CFLAGS) build/* -o dist/safechat $(LDLIBS)

clean:
	rm -r build dist
install:
	sudo cp dist/safechat /usr/bin/
remove:
	sudo rm /usr/bin/safechat
