## My MRT protocol

#### MRT header composition

* A receiver identifies the connections/senders via the `sockaddr_in` returned from `recvfrom()`, so the MRT header does not contain further identifier info. However, the checksum can be made stronger by including in the identifier info (but otherwise it is redundant). Since the sender does not need to authenticate themselves, the connection id is assigned locally (instead of being received from the first ACON).

#### Handling out-of-order delivery

* Uses Go-Back-N for flow control.

* The fragment number is similar to TCP's sequence number in that the sender will propose an initial number and the receiver will acknowledge the number every time the corresponding payload is successfully buffered; since I use GBN, each ADAT would acknowldge the success of all prior fragments as well. However, the major distinction is that the fragment number increments 1 per UDP datagram successfully received (it can also be considered 1 per IP packet in my implementation), unlike sequence number, which increments 1 per byte successfully received. Nonetheless, the fragment number is still encoded as an integer.

#### Handling high latency, packet loss, and silent disconnection:

* the sender is always responsible for *actively* maintaining the connection. At first, the sender keeps sending RCONs until an ACON arrives; then the sender will keep sending DATAs: 

  * if the advertised window size is too small or the sender has nothing to send at the moment, the DATAs will be empty, otherwise DATA containing payloads will be sent. The receiver, on the other hand, passively maintains the connection by responding to every transmission from the sender (likely sending duplicate acknowledgements).

  * if the sender has nothing to send at the moment, it will start a timer. The timer is reset if the sender has anything to send again. Upon reaching the timeout threshold, the sender marks all unacknowledged fragments as unsent and reset the timer, so in the next iteration, the sender will naturally start resending those fragments.

* timeout counter for closing is automatically incremented; each incoming transmission resets the corresponding timeout counter. If no transmission occurs for a period of time, the counter will be let to exceed the timeout threshold and cause the connection to be dropped.

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

`mrt_sender.c` and `mrt_receiver.c` contain the actual code while `mrt_sender.h` and `mrt_receiver.h` are the header files for the abstracted methods available to `mrt` module users.

## Notable implementation choices:

* currently IPv4-exclusive.

* senders and receivers perform clean-up on a successful transmission by tricking the checker into thinking that a timeout happened (in fact, the receiver can just do nothing and let the connection timeout by itself).

* once a connection closes, the receiver can no longer access the corresponding buffer (it gets garbage-collected alongside the other sender info)

## Structural TODOs / TOTHINKs (not part of the write-up):

#### breaking changes:

* acknowledge by bytes instead of fragments
* use `CVAR` instead of relying on waking up repeatedly from `sleep()` (what was the term for such a bad practice?).
* better mutex usage / management for receiver (instead of relying on the `q_lock` to do most of the work)
* better mutex usage / management for sender (HMM need to implement a lock that ensures that a connection_t is not freed... it's more than putting the locks outside the connection_t struct...)
* make a sender window struct... the current approach is too unwieldy


#### not-so-breaking changes:

* protect against more bad use cases such as repeatedly calling `mrt_open()`
* consider using MSG_CONFIRM flags for sending acknowledgements
* consider sending acknowldedgements in new threads (minor concurrency)
* keep the buffer of a closed connection in memory if the buffer still has unread bytes... until...?
* store `last_frag` instead of `next_frag` in the `sender_t`.
* verify in sender that the received message is indeed from the target receiver
* verify all received info (even given same hash, same source, etc. E.g. the received fragment number must be within valid range)
* use circular buffer (or other efficient ones) instead of relying on memmove()
* stop indenting for mutex pairs... it hurts me. I hurt myself. In the receiver module...

#### TOTHINKs

* any reason to make `mrt_accept()` and `mrt_receive()` families non-blocking?
