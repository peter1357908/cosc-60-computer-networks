## COSC 60 Lab 4 README.md

### How to use this STUN NAT-detector

This detector is partially automated. We assume that you already know that you are behind a NAT before using this detector. This detector finds out which type of NAT  you are behind out of the four found [here](https://www.voip-info.org/stun/). 

To begin, run `make test_symmetry` to test if you are behind a symmetric NAT. There are at least the following cases:

1. you receive no prompt and the program remains blocked: this could mean that the binding requests are never successfully sent. Try adding `perror()` after `sendto()` to help debug; check if you are connected to the internet and if the STUN servers are reachable.
1. you receive 1 prompt about your NAT IP and NAT PORT but the program remains blocked: there is still not enough information to tell if you are behind a symmetric or not - perform the same debugging procedures as the "received no prompt" case.
1. you receive multiple prompts and you notice that you have different NAT IP and NAT PORT each time: **you are behind a Symmetric NAT**.
1. you receive multiple prompts (but the program gets blocked) and you notice that you have the same NAT IP and NAT PORT each time: you are likely behind a non-symmetric NAT; proceed by running `make test_cone_level`.
1. you are notified that **you are behind a Symmetric NAT**.
1. you are notified that you are likely not behind a symmetric NAT, and that you should run `make test_cone_level`.

Then, after running `make test_cone_level`, there are at least the following cases:

1. you receive no prompt and the program remains blocked: this could mean the same errors happened as in the first case above. However, if you check Wireshark and notice that all binding requests are correctly configured and successfully sent, then **you are behind a Port Restricted Cone NAT**.

1. you are notified that **you are behind a Full Cone NAT**

1. you are notified that "You are definitely not behind a Port Restricted Cone NAT" - in that case, if the program gets stuck with no prompts claiming that you are behind a Full Cone NAT, then likely **you are behind a Restricted Cone NAT**.

### What type of NAT am I behind?

I am apparently behind a "Port Restricted Cone NAT" - I have a static mapped address, but any external host can send a packet to me (at that address) ONLY IF I have sent a packet to that external host at the exact IP address AND the exact port before.

The result of following in the procedures in the last section can be found in the Wireshark captures also in this folder.

### Was I able to receive a UDP packet from a fellow student? From which student(s) have I tried?

No; I have not attempted UDP communication with any fellow student.

### My timezone and preferences for final-project group members:

I am in Central Time right now and will remain so for the rest of the term.

I prefer to group with the following fellow students:

* Clay C. Foye
* Siddharth Agrawal
* Benjamin Cape
* Vlado Vojdanovski

### References:

[Voip-info.org on STUN](https://www.voip-info.org/stun/)

[The STUN Protocol Explained – Messages, Attributes, Error codes](https://www.3cx.com/blog/voip-howto/stun-details/)

[RFC5389 Session Traversal Utilities for NAT (STUN)](https://tools.ietf.org/html/rfc5389)

[What you need to know about symmetric NAT](https://think-like-a-computer.com/2011/09/19/symmetric-nat/)

[List of supposedly alive STUN servers](https://github.com/DamonOehlman/freeice#stun)

### The STUN servers used in this detector module

* stun.voipstunt.com:3478 (this one still supports the deprecated RFC 3489 specifications like `SOURCE_ADDRESS` and `CHANGED_ADDRESS`)
* stun.l.google.com:19302
* stun1.l.google.com:19302
* stun2.l.google.com:19302
* stun3.l.google.com:19302
* stun4.l.google.com:19302

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
