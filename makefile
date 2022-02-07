all: httpd
LIBS = -lpthread #-lsocket
httpd: httpd.c
	gcc -g -W -Wall $(LIBS) -o $@ $<

clean:
	rm httpd
