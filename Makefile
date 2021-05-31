all:
	gcc -g server.c -o server
	
optim:
	gcc -O3 server.c -o server.optim