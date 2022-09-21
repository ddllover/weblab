# weblab
# lab2 : A Simple Reliable Transport Protocol
In this assignment, you will build a simple reliable transport protocol, RTP, on top
of UDP
###  RTP Specification
RTP sends data in the format of a header, followed by a chunk of data. RTP has four
header types: ST ART, END, DAT A, and ACK, all following the same format:
```
typedef s t r u c t RTP_header {
uint8_t type ; // 0 : START; 1 : END; 2 : DATA; 3 : ACK
uint16_t length ; // Length of data ; 0 for ACK, START and END packets
uint32_t seq_num ;
uint32_t checksum ; // 32−b i t CRC
} rtp_header_t ;
```
* Establish connection. To initiate a connection, sender starts with a ST ART message along with a random seq_num value, and wait for an ACK for this ST ART
message.
* Data transmission. After establishing the connection, additional packets in the same
connection are sent using the DAT A message type, adjusting seq_num appropriately.
sender will use 0 as the initial sequence number for data packets in that connection.
* Terminate connection. After everything has been transferred, the connection should
be terminated with sender sending an END message, and waiting for the corresponding ACK for this message.
* ACK for START & END. The ACK seq_num values for ST ART and END messages
should both be set to whatever the seq_num values are that were sent by sender.
* Packet Size. An important limitation is the maximum size of your packets. The UDP
protocol has an 8 byte header, and the IP protocol underneath it has a header of 20
bytes. Because we will be using Ethernet networks, which have a maximum frame size
of 1500 bytes, this leaves 1472 bytes for your entire packet structure (including both
the header and the chunk of data)
## Assignment components
sender should be invoked as follows:
`. / sender [ Receiver IP ] [ Receiver Port ] [ Window Size ] [ Message ]`
receiver should be invoked as follows:
`. / receiver [ Receiver Port ] [ Window Size ] [ File Name]`
## optimizations
* receiver will not send cumulative ACKs anymore; instead, it will send back an
ACK with seq_num set to whatever it was in the data packet (i.e., if a sender
sends a data packet with seq_num set to 2, receiver will also send back an ACK
with seq_num set to 2). It should still drop all packets with seq_num greater than
or equal to N+ window_size, where N is the next expected seq_num.
* sender must maintain information about all the ACKs it has received in its
current window. In this way, packet 0 having a timeout would not necessarily
result in a retransmission of packets 1 and 2.

# lab3
In this assignment, you will implement the distance vector routing protocol.
## Protocol Description
### Router location file
The router location file maps a unique router ID to a router process. Each router is
defined by a 3-tuple:`<router IP,router port,router id>`
Each router is assigned a unique identifier router id. It runs on IP router IP,
and listens on port router port for two pieces of information: (i) commands from the
agent and (ii) messages from other routers.
### Topology configuration file
The topology configuration file specifies the router topology. Here, the distance between
two neighboring routers is unidirectional, i.e., the distance from router 1 to router 2 may
be different from the distance from router 2 to router 1. The topology configuration
file is a text file composed of a number of topology tuples. The format of a topology
tuple is shown below:`<RouterID1,RouterID2,weight>`
## The router program
`.router <router location file> <topology conf file> <router id>`
## The agent program
|Name|Example of commands|Description|
|----|----|----|
|dv|dv|All routers are triggered to propagate distance vectors to neighboring routers until
the link costs become stable|
|update|update:1,6,4|Change the link weight of link (1,6) to 4. If
the link weight is -1, then it means that the
link is deleted. If the link is absent before,
then it means that the link is added with
the update command|
|show|show:2|Display the routing table of router 2.|
|reset|reset:2|Reset the number of distance vectors of
router 2 to zero. This helps us easily debug your assignment|

`.agent <router location file>`

## Protocol Messages
* Messages between the agent and a router.
* Messages between two routers
