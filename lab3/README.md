## My MRT protocol

#### MRT Transmission composition

* An MRT Tranmission consists of 5 parts in order: checksum (8 bytes, unsigned long), type (4 bytes, int), fragment number (4 bytes, int), window size (4 bytes, int), and the payload (max size varies and is defined in MAX_MRT_PAYLOAD_LENGTH in `mrt.h`).

* There are 6 types of MRT transmissions (each of them corresponds to an integer as defined in `mrt.h` as well):
  1. `RCON`: a connection request, in which the sender includes the preferred initial fragment number (set to be 0 in the implementation)
  1. `ACON`: acknowledgement for RCON, in which the receiver acknowledges the initial fragment number and start expecting the next fragment as the DATA fragment. The receiver advertises for its current window size (the first, non-duplicate ACON should contain the max window size) for this connection in `ACON`.
  1. `DATA`: a data transmission, with its corresponding fragment number. An empty DATA transmission with a special fragment number is one sent purely to keep the connection alive (more in section below).
  1. `ADAT`: acknowledgement for DATA , in which the receiver acknowledges that all fragments, up to the included fragment number, are already either processed or buffered in the receiver window. The receiver also advertises for its current window size in `ADAT`.
  1. `RCLS`: a disconnection request, which the sender only sends after making sure that the sender has nothing buffered to send anymore (in other words, all sent data's acknowledges are correctly received). As a result, no fragment number is necessary here (it will only be sent after the last sent fragment is acknowledged).
  1. `ACLS`: acknowledgement for RCLS; nothing special - in fact, all this transmission has is a hash and a type of `ACLS`. It is not very useful, either, due to how `RCLS` is designed (the sender can start packing up immediately after sending out an `RCLS`).

* A receiver identifies the connections/senders via the `sockaddr_in` returned from `recvfrom()`, so the MRT header does not contain further identifier info. However, the checksum can be made stronger by including in the identifier info (but otherwise it is redundant). Since the sender does not need to authenticate themselves, the connection id is assigned locally (instead of being received from the first ACON).

#### Flow control and congestion control

* Uses Go-Back-N and sliding window for flow control (lost/dropped transmissions will eventually be treated as unsent and be automatically resent; corrupted and out-of-order transmissions are simply dropped, and thus also eventually resent).

* The fragment number is similar to TCP's sequence number in that the sender will propose an initial number and the receiver will acknowledge the number every time the corresponding payload is successfully buffered; since this module uses GBN, each ADAT would acknowldge the success of all prior fragments as well. However, the major distinction is that the fragment number increments 1 per MRT transmission successfully received (it can also be considered 1 per UDP datagram and 1 per IP packet in my implementation), unlike sequence number, which increments 1 per byte successfully received. Nonetheless, the fragment number is still encoded as an integer.

* the sender is always responsible for *actively* maintaining the connection. At first, the sender keeps sending RCONs until an ACON arrives; then the sender will keep sending DATAs: 

  * if the advertised window size is too small or the sender has nothing to send at the moment, the DATAs will be empty, otherwise DATA containing payloads will be sent. The receiver, on the other hand, passively maintains the connection by responding to every transmission from the sender (likely sending duplicate acknowledgements).

  * if the sender has nothing to send at the moment, it will start a timer. The timer is reset if the sender has anything to send again. Upon reaching the timeout threshold, the sender marks all unacknowledged fragments as unsent and reset the timer, so in the next iteration, the sender will naturally start resending those fragments.

* timeout counter for "drop-connection" is automatically incremented; each relevant incoming transmission can reset it (for example, having received an ACON means the sender is still alive, so the drop-connection counter can be reset). If no transmission occurs for a period of time, the counter will be let to exceed the timeout threshold and cause the connection to be dropped.

* similarly, there is a timeout counter for "resend-data" - if for a while the sender's data buffer has been stuck at a certain level, it means that the receiver is probably just getting all out-of-order data (any ADAT with a newer fragment number can take some part off the sender buffer). In that case, per GBN, just mark all unacknowledged buffered payloads as unsent.

#### Handling various sizes of data

* It does really not matter - however small a payload is, it is immediately (attempted to be) queued in the buffer (and sent whenever possible, so there is no intentional blocking to "allow the data to build up"); however large a payload is, it will be copied one payload's max_size at a time into the buffer - the program's memory use is thus limited (in fact, fixed) for each sender/connection.

## Lab question responses

#### Testing in general

* To see 2 senders and 1 receiver in action:
  
  `make test_receiver2`, `make test_sender1`, and `make test_sender2`, each in a different terminal, in any order you wish (now you can start typing to stdin and hitting Enter or EOF to see the module's performance).

* To see 1 sender and 1 receiver with input generated by and piped from `number_writer`, and then had their output compared by `diff` simply do:

  `make test_sender_newline` and `make test_receiver_newline`

  or `make test_sender_comma` and `make test_receiver_comma`

* The usage are `sender sender_port_number read_size` and `receiver num_connections`, respectively.

* The main testing tool is [Clumsy](https://github.com/jagt/clumsy) on Windows.

* To make the effect of a bad link more obvious, uncomment line 361-364 in `mrt_receiver` (this will make `diff` mad, though, but using naked eye should be enough to see the output consistenncy).

#### Actual response

1. How I tested against packet loss

  By enabling the `Drop` option on `Clumsy` with 15% chance both ways and looking at the result produced by `diff` between actual output by the receiver and the expected output by the `number_writer` directly (e.g. `make test_sender_newline` and `make test_receiver_newline`).

1. How I tested against data corruption

  By enabling the `Tamper` option on `Clumsy` with 15% chance both ways...

1. How I tested against out-of-order delivery

  By enabling the `Out of order` option on `Clumsy` with 15% chance both ways...

1. How I tested against high-latency delivery

  By enabling the `Lag` option on `Clumsy` with 10ms both ways (the default expected RTT set in `mrt.h` is 10ms)...

1. How I tested sending small amounts of data

  By passing a small `read_size` to the `sender` driver (e.g. `make test_sender_newline`) - the application reads small bites and call `mrt_send()` on each such bite individually.

1. How I tested with large amounts of data

  By passing in a big `read_size` to the `sender` driver and a big enough input to make the effect obvious (e.g. `make test_sender_comma`).


1. How I tested flow-control

  By setting a high enough latency with `Clumsy`, so the `sender` constantly have a big enough buffer such that it will stop sending more (and send empty DATA instead). Alternatively, I let the `receiver` sleep for a significant amount of time between two `mrt_receive1()` calls, so that the receiver's window is constantly filled (in which case the sender will also end up starting to send empty DATA until the receiver's window clears up).

1. Functions my `sender` and `receiver` program did and did not use

  `mrt_probe()` and `mrt_accept_all()`.

1. How I tested the functions that weren't used in `sender` or `receiver`

  * `mrt_accept_all()`: Instead of using a for loop with `mrt_accept1()`, I could use a while loop that constantly calls `mrt_accept_all()` and record the number of connections accepted - break out of the while loop if the number accepted exceeds the number input in command line (admittedly this could result in accepting more than necessary, but that is not the point).

  * `mrt_probe()`: Instead of repeatedly calling `mrt_receive1()`, I could use a while loop that constantly calls `mrt_probe()`; if it returns a connection, mrt_receive1() from it, and if it does not, sleep for a while and continue into the next iteration of the while loop.

1. Files that contain the implementation of the nine primary MRT abstractions

  `mrt_sender.c` and `mrt_receiver.c` contain the actual code while `mrt_sender.h` and `mrt_receiver.h` are the header files for the abstracted methods available to `mrt` module users (you can find detailed usage info in those header files). `mrt.h` contains protocol-level definitions useful to both the sender module and the receiver module (but should be opaque to MRT users).

## Notable implementation choices:

* all sleep intervals, timeout increments, and thresholds depend on a single input - the `EXPECTED_RTT` in `mrt.h`. By adjusting `EXPECTED_RTT`, the module can adapt to links of various expected latencies.

* currently IPv4-exclusive.

* senders and receivers perform clean-up on a successful transmission by tricking the checker into thinking that a timeout happened.

* the receiver can access a connection's buffer even after that connection is dropped, but only until the receiver calls `mrt_close()`.

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
* try to decide between sending meaningful DATA and empty DATA by looking at the expected window size, instead of the "lastest reported window size."

#### TOTHINKs

* any reason to make `mrt_accept()` and `mrt_receive()` families non-blocking?
