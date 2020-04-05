all: tcp udp

tcp: tcp_message.c
	gcc tcp_message.c -o tcp_message
	
udp: udp_message.c
	gcc udp_message.c -o udp_message
	
test_tcp: tcp
	./tcp_message
	
test_udp: udp
	./udp_message
	
clean:
	rm -f tcp_message
	rm -f udp_message