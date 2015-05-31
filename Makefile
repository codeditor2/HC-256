CC := gcc
LIB := /home/rudy/myprogram/lib/libmediastreamer.so /home/rudy/myprogram/lib/libortp.so
OPT :=
POPT := --shared -fPIC -std=c99

default:
	$(CC) rtpsend.c -L /home/rudy/myprogram/lib/ -lortp -I /home/rudy/myprogram/include/ -o rtpsend
	$(CC) rtprecv.c -L /home/rudy/myprogram/lib/ -lortp -I /home/rudy/myprogram/include/ -o rtprecv
	$(CC) recv.c $(OPT) -L /home/rudy/myprogram/lib/ -lmediastreamer -lortp -I /home/rudy/myprogram/include/ -o recv
	$(CC) send.c $(OPT) -L /home/rudy/myprogram/lib/ -lmediastreamer -lortp -I /home/rudy/myprogram/include/ -o send
	$(CC) sendrecv.c $(OPT) -L /home/rudy/myprogram/lib/ -lmediastreamer -lortp -I /home/rudy/myprogram/include/ -o sendrecv
	$(CC) $(POPT) mscrypt.c hc256.c -L /home/rudy/myprogram/lib/ -lmediastreamer -lortp -I /home/rudy/myprogram/include/ -o libmscrypt.so
	$(CC) $(OPT) test_hc256.c hc256.c -L /home/rudy/myprogram/lib/ -lmediastreamer -lortp -I /home/rudy/myprogram/include/ -o test
clean:
	rm -rf *~ recv send sendrecv *.o
