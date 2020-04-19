# Write-up

## How to use the client
To build, run `make client`

Usage: `client server_port username`

For a fast test, run `make client_test`, which will try to connect to `localhost:4242` with username "Peter".

Upon successfully joining the chatroom, send messages by typing it and hitting `Enter`. To quit, input the `EOF` character.

Currently, a username can be at most 20 characters, while each message can be at most 512 characters.

## How the client is programmed
The client is a double-threaded process. After the connection is established, the client would first create a new thread used exclusively for outgoing transmission and have that thread send a `JOIN` request; in turn, the original thread will be exclusively for interpreting (and displaying) incoming transmission.

## How to use the server
To build, run `make server`

Usage: `server server_port`

For a fast test, run `make server_test`, which will run a server listening at `localhost:4242`.

## How the server is programmed
The server is a `(n+1)`-threaded process, where `n` is the number of clients currently connected to it. Upon running, the server will start listening at `localhost` with the port specified in the command line argument - it goes into an infinite loop trying to accept incoming connections. 

With each accepted connection, the connection socket is recorded and a new thread is created for receiving messages from that connection. Most importantly, each thread, upon receiving a message, will iterate through all the connected clients and send them the exact same message (thanks to the convenient Dual-Purpose Protocol), thus achieving "broadcast under TCP." When the server notices that the client disconnects or requests to disconnect, the respective thread will shutdown&close the socket and terminate (after broadcasting the QUIT message, if the client requested to disconnected).

## How to test this implementation
The easiest way to test (right after cloning this repo) is to open two or more terminals - in one terminal, run `make test_server`; in another, run `make test_client`, and finally, run `./client 4242 username` in other terminals. Now start chatting!

## DPP, the Dual-Purpose Protocol

Please see [here](https://sjcjoosten.nl/etc/ComputerNetworksWiki/index.php/Lab2_DPP) for explanation.

One of the biggest advantages of having one identifier for a pair of transmission types (e.g. message from client versus notification of such message from server) is that it allows the server to be almost entirely passive (the server can broadcasts all messages without worrying about parsing and re-building them). In other words, this protocol is easily P2P-friendly.

## How to interpret the DPP

This is also mentioned in [the Wiki](https://sjcjoosten.nl/etc/ComputerNetworksWiki/index.php/Lab2_DPP). The basic idea is to delimit the transmission with double colons `::`, then, based on the transmission type as indicated in the first segment (`HELO`, `MSG`, `BYE`), react accordingly. In my implementation, the server blindly broadcasts the messages it receives (i.e. a user will receive his own `HELO` message), and the client is tasked with parsing the incoming transmissions (displaying human-friendly notifications based on the transmission).

More specifically, the parsing logic and facts about the protocol is contained in the `dpp` module, and the client only knows as much as `dpp.h` tells it (`dpp` has functions that parse incoming transmission and build outgoing transmission from given input; the client invokes those functions and does not worry about parsing/building).

## How the multi-client feature is implemented

The client does not care about this feature, and the server is tasked with maintaining it - as mentioned in the "How the server is programmed" section, the server will create a new thread for each client connection, and try to receive transmission from each connection concurrently. Synchronizations are implemented to prevent race conditions, especially during the broadcasting that happens upon receiving any transmission (e.g. the modifying and iterating-through of the socket table is critical).

## How multiple clients can talk "at the same time"

This is possible because:

1. each client sends and receives concurrently, via different threads (otherwise the flow will be blocked while waiting for user input)

2. the server receives from each client on a different thread, and broadcast each transmission as soong as they arrive (to ensure concurrency and minimal delay)

## How joining and disconnecting is handled on the client-side

The client requests to join and disconnect in its sender thread. In my implementation, the user is only allowed to start sending messages once its join request is acknowledged (when it receives the same join request it sent to the server). The client quits automatically upon noticing that the server disconnected (possibly a result of a disconnect request from said client).

## How to use this client with other students' servers

Merely follow their build/compilation/run instructions. On the binary level, nothing special needs to be done to facilitate the comminucation (e.g. my client can connect to each one of Allen's, Clay's, and my own server). Just make sure that the port number argument is the same across.

## How to use other students' clients with this server

Merely follow their build/compilation/run instructions. On the binary level, nothing special needs to be done to facilitate the comminucation (e.g. my server can handle one each of Allen's client, Clay's client, and my own client, at the same time). Just make sure that the port number argument is the same across.

## Differences among implementations

Although we all use the DPP, the way the user is able to initiate a disconnect request is different - for mine, entering `EOF` triggers the client to request a disconnection, whereas for Allen's and Clay's, entering a `/quit` message is the way.

As obvious as it sounds, the language difference is there, too - C is in general lower-level than Java (Clay) and Go (Allen), hence the difference in code complexity and run-time efficiency.

The UI is slightly different, too, as it is not specified in the protocol (it is basically a feature of the client).

## Notable implementation choices:
1. Currently, duplicate usernames among the clients are allowed (and not differentiated).

