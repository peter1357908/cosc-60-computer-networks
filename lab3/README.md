## My MRT protocol

#### MRT header composition

* Identifies the senders via the `sockaddr_in` returned from `recvfrom()`, so the MRT header does not contain further identifier info. However, the checksum can be made stronger by including in the identifier info (but otherwise it is redundant).

#### Handling out-of-order delivery

* Uses Go-Back-N for flow control.

* The fragment number is similar to TCP's sequence number in that the sender will propose an initial number and the receiver will acknowledge the number every time the corresponding payload is successfully buffered; since I use GBN, each ADAT would acknowldge the success of all prior fragments as well. However, the major distinction is that the fragment number increments 1 per UDP datagram successfully received (it can also be considered 1 per IP packet in my implementation), unlike sequence number, which increments 1 per byte successfully received. Nonetheless, the fragment number is still encoded as an integer.

#### Handling high latency, packet loss, and silent disconnection:

* the sender is always responsible for *actively* maintaining the connection. At first, the sender keeps sending RCONs until an ACON arrives; then the sender will keep sending DATAs - if the advertised window size is too small or the sender has nothing to send at the moment, the DATAs will be empty, otherwise DATA containing payloads will be sent. The receiver, on the other hand, passively maintains the connection by responding to every transmission from the sender (likely sending duplicate acknowledgements).

## Lab question responses
1. How I tested against packet loss


1. How I tested against data corruption


1. How I tested against out-of-order delivery


1. How I tested against high-latency delivery


1. How I tested sending small amounts of data


1. How I tested with large amounts of data


1. How I tested flow-control


1. Functions my `sender` and `receiver` program did and did not use


1. How I tested the functions that weren't used in `sender` or `receiver`


1. Files that contain the implementation of the nine primary MRT abstractions

## Notable implementation choices:

* only supports the loopback interface (consequently, connections/senders are identified by their port number alone)

* senders and receivers perform clean-up on a successful transmission by tricking the checker into thinking that a timeout happened (in fact, the receiver can just do nothing and let the connection timeout by itself).

* once a connection closes, the receiver can no longer access the corresponding buffer (it gets garbage-collected alongside the other sender info)

## Structural TODOs / TOTHINKs (not part of the write-up):

* protect against more bad use cases such as repeatedly calling `mrt_open()`
* consider using MSG_CONFIRM flags for sending acknowledgements
* consider sending acknowldedgements in new threads (minor concurrency)
* consider creating a lock for each sender rather than for the entire pair of queues
* keep the buffer of a closed connection in memory if the buffer still has unread bytes... until...?
* any reason to make `mrt_accept()` and `mrt_receive()` families non-blocking?
* store `last_frag` instead of `next_frag` in the `sender_t`.
* use `CVAR` instead of relying on waking up repeatedly from `sleep()` (what was the term for such a bad practice?).
* verify in sender that the received message is indeed from the target receiver
* verify all received info (even given same hash, same source, etc. E.g. the received fragment number must be within valid range)
* make a sender window struct... the current approach is too unwieldy
* use circular buffer (or other efficient ones) instead of relying on memmove()
