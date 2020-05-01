## My MRT protocol

Uses Go-Back-N for flow control.

Only supports the loopback interface (consequently, connection identifier only consists of the port number in network byte order)

Designed for one-time data transfer (receive the entire data and be done)

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

