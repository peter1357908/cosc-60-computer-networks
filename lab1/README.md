# COSC 60 Lab 1 README.md

1. `Wireshark` installed.
2. `C` chosen as preferred language.
3. Preferred editor: `vim` and `Notepad++`. Preferred compiler: `GCC`

A. the browser (Chrome) shows "This page isn’t workinglocalhost sent an invalid response.
ERR_INVALID_HTTP_RESPONSE" and stopped loading. The captured packets are in the file `8099 packets.pcapng`
B. the program code is in `tcp_message.c`. After compiling with gcc and running (with `nc -l 8099` in another terminal), the captured packets are in the file `tcp packets.pcapng`
C. the program code is in `udp_message.c`. After compiling with gcc and running, the captured packets are in the file `udp packets without nc.pcapng`. If we run `nc -u -l 2020` in another terminal beforehand, the result is different, and can be found in `udp packets with nc.pcapng`.
