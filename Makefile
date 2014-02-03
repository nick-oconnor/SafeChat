CC = g++
CFLAGS = -Wall -O3
LDLIBS = -lpthread -lcrypto

build:
	$(CC) $(CFLAGS) *.cpp -o safechat $(LDLIBS)

install:
	sudo cp safechat /usr/bin/

remove:
	sudo rm /usr/bin/safechat
