## COSC 60 Lab 4 README.md

### How to compile and run this STUN NAT-detector

### What type of NAT am I behind?

### Was I able to receive a UDP packet from a fellow student? From which student(s) have I tried?

### What is your timezone and do you have preferences for final-project group members? (I'll use this info to make final project groups: first I group according to preference, then to timezone, then to programming language)


### References:

[Voip-info.org on STUN](https://www.voip-info.org/stun/)

[The STUN Protocol Explained – Messages, Attributes, Error codes](https://www.3cx.com/blog/voip-howto/stun-details/)

[RFC5389 Session Traversal Utilities for NAT (STUN)](https://tools.ietf.org/html/rfc5389)

[What you need to know about symmetric NAT](https://think-like-a-computer.com/2011/09/19/symmetric-nat/)

[List of supposedly alive STUN servers](https://github.com/DamonOehlman/freeice#stun)

### Some Servers and Their IP Addresses

* stun.voipstunt.com:3478 77.72.169.(210~213) (this one still supports the deprecated specifications like `SOURCE_ADDRESS` and `CHANGED_ADDRESS`)
* stun.l.google.com:19302 74.125.128.127 74.125.133.127 108.177.15.127
* stun1.l.google.com:19302 74.125.68.127 64.233.163.127
* stun2.l.google.com:19302 74.125.204.127 74.125.68.127
* stun3.l.google.com:19302 173.194.202.127 74.125.204.127
* stun4.l.google.com:19302 173.194.199.127 173.194.202.127 (interesting pattern...)

### STUN Cheat Sheet

##### first 16 bits in the header:

* `0x0001` : Binding Request
* `0x0101` : Binding Response
* `0x0111` : Binding Error Response

##### next 16 bits in the header:

The size of the STUN message, in *bytes*, *excluding* the 20-byte STUN header. (remember: all STUN attributes are padded to a multiple of 4 bytes)

##### next 32 bits in the header:

The magic cookie field; must be `0x2112A442` (for RFC5389?).

##### next 96 bits in the header:

Transaction ID, chosen by the client, sent in the request, and echoed by the server in response. Should be cryptographically random.

##### STUN Attributes:

16-bit `type`, 16-bit `length` (in bytes), and a `value` that's a multiple of 32-bits. Only the first occurrence of a unique attribute needs to be processed.

[RFC5389 Attributes](https://tools.ietf.org/html/rfc5389#section-15)


After running your client, your client should report either:
Peer may connect to IP-address:port-number
I am IP-address:port-number, but probably unreachable
Could not connect to a STUN server

"You should be able to complete the lab by sending just two different messages and listening to what comes back"

"I do know the STUN server stun.voipstunt.com:3478 works well for me and allows for IP spoofing"
