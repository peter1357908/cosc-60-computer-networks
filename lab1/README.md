# COSC 60 Lab 1 README.md

1. `Wireshark` version 3.2.2 installed.
2. `C` is my preferred language for socket programming (but I am quite new to socket programming in general).
3. Preferred editor: `vim` and `Notepad++`. Preferred compiler: `GCC`.

## A
After typing the text "something" and hitting `enter` key (in my case, only once), the browser (Chrome) shows the following:
```
This page isn’t working
localhost sent an invalid response.
ERR_INVALID_HTTP_RESPONSE
```
Then the page stopped loading. The captured packets are in the file [8099_packets.pcapng](./captured_packets/8099_packets.pcapng).

## B
The program code is in `tcp_message.c`. Run `nc -l 8099` first, and in another terminal, run `make test_tcp` to see it in action. The captured packets are in the file [tcp_packets.pcapng](./captured_packets/tcp_packets.pcapng).

## C
The program code is in `udp_message.c`. See it in action by running `make test_udp`. The captured packets are in the file [udp_packets_without_nc.pcapng](./captured_packets/udp_packets_without_nc.pcapng). If you run `nc -u -l 2020` in another terminal beforehand, the result is different, and can be found in [udp_packets_with_nc.pcapng](./captured_packets/udp_packets_with_nc.pcapng).
