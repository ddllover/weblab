#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3
#define BUFFER_SIZE 2048
#define data_size 1461
typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t; //11个byte

typedef struct RTP_control_block {
    uint32_t window_size;
    uint32_t ep_num;//记录下一次发送的seq_num
    int *ack;  //记录上一次发送且被确认的包序列
    // TODO: you can add your RTP-related fields here
} rcb_t;

static rcb_t* rcb = NULL;
//这里全局变量rcb

void ini_rtp(rtp_header_t * rtp,uint8_t type,uint16_t length,uint32_t seq_num,int compute);
// different from the POSIX
void rcb_end();

int rtp_socket(uint32_t window_size);

int rtp_listen(int sockfd, int backlog);

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int rtp_close(int sockfd);

int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen);

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen);

int rtp_select(int sockfd);

int op_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen);
#endif //RTP_H
