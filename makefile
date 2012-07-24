build:
	g++ -c -O3 -Wall -s -Iheaders -MMD -MP -MF main.o.d -o main.o main.cpp
	g++ -o safechat -Wl,-S main.o -lpthread -lcrypto -lssl
	rm main.o*
install:
	cp safechat /usr/bin/
