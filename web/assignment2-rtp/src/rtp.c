#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include "rtp.h"
#include "util.h"

void rcb_init(uint32_t window_size)
{
    if (rcb == NULL)
    {
        rcb = (rcb_t *)calloc(1, sizeof(rcb_t));
    }
    else
    {
        perror("The current version of the rtp protocol only supports a single connection");
        exit(EXIT_FAILURE);
    }
    rcb->window_size = window_size;
    rcb->ep_num = 0;
    rcb->ack = (int *)malloc(window_size * sizeof(int));
    for (int i = 0; i < window_size; i++)
    {
        rcb->ack[i] = -1;
    }
    // TODO: you can initialize your RTP-related fields here
}
void rcb_end()
{
    free(rcb->ack);
}
/*********************** Note ************************/
/* RTP in Assignment 2 only supports single connection.
/* Therefore, we can initialize the related fields of RTP when creating the socket.
/* rcb is a global varialble, you can directly use it in your implementatyion.
/*****************************************************/
int rtp_socket(uint32_t window_size)
{
    rcb_init(window_size);
    // create UDP socket
    return socket(AF_INET, SOCK_DGRAM, 0); //ipv4 udp
}

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int rtp_listen(int sockfd, int backlog)
{
    //用recvfrom接到START的信息
    printf("wait connect\n");
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    rtp_header_t rtp;
    int ret = rtp_recvfrom(sockfd, (void *)&rtp, sizeof(rtp), 0, (struct sockaddr *)&sender, &addr_len);
    if (ret < 0)
        return -1; //check_sum 错误
    else
    {
        if (rtp.type == RTP_START)
        {
            printf("listen START \n");
            ini_rtp(&rtp, RTP_ACK, 0, rtp.seq_num, 1);
            printf("send START ACK\n");
            sendto(sockfd, (void *)&rtp, sizeof(rtp), 0, (struct sockaddr *)&sender, sizeof(struct sockaddr));
        }
    }
    // TODO: listen for the START message from sender and send back ACK
    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
    // For RTP, backlog is always 1
    return 1;
}

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    // Since RTP in Assignment 2 only supports one connection,
    // there is no need to implement accpet function.
    // You don’t need to make any changes to this function.
    return 1;
}

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // TODO: send START message and wait for its ACK
    //char buffer[2048];
    //memset(buffer,0,sizeof(buffer));
    rtp_header_t rtp; //=(rtp_header_t*)buffer;;
    ini_rtp(&rtp, RTP_START, 0, 1, 1);
    rtp_header_t rtp_ack;
    printf("connecting sendto start\n");
    sendto(sockfd, (void *)&rtp, sizeof(rtp), 0, addr, addrlen);
    if (rtp_select(sockfd) > 0)
    { //等待对面接收START
        printf("accept start ack\n");
        rtp_recvfrom(sockfd, (void *)&rtp_ack, sizeof(rtp_ack), 0, NULL, 0);
        if (rtp_ack.type == RTP_ACK && rtp_ack.seq_num == 1)
        { //
            return 1;
        }
    }
    //正常接收start ack已经返回 无ack 错误ack啊
    ini_rtp(&rtp, RTP_END, 0, 1, 1);
    sendto(sockfd, (void *)&rtp, sizeof(rtp), 0, addr, addrlen);
    if (rtp_select(sockfd) > 0)
    {
        printf("accept end ack\n");
        rtp_recvfrom(sockfd, (void *)&rtp_ack, sizeof(rtp_ack), 0, NULL, 0);
        if (rtp_ack.type == RTP_ACK && rtp_ack.seq_num == 1)
        { //
            ;
        }
    }
    else
    {
        printf("time out wait end ack\n");
        return -1;
    }
    return -1; //直接断开链接
}

int rtp_close(int sockfd)
{
    return close(sockfd);
}

int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen)
{
    // TODO: send message
    char buffer[2048];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    for (int i = 0; i < rcb->window_size && len > 0; i++)
    {
        memset(buffer, 0, sizeof(buffer));

        if (len >= data_size)
        {
            ini_rtp(rtp, RTP_DATA, data_size, i + rcb->ep_num, 0);
            len -= data_size;
        }
        else
        {
            ini_rtp(rtp, RTP_DATA, len, i + rcb->ep_num, 0);
            len = 0;
        }
        memcpy((void *)buffer + sizeof(rtp_header_t), msg + i * data_size, rtp->length);
        rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + rtp->length);
        int sent_bytes = sendto(sockfd, (void *)buffer, sizeof(rtp_header_t) + rtp->length, flags, to, tolen); //确保每个包都是1472
        printf("sent packed num %d\n", rtp->seq_num);

        if (sent_bytes != (sizeof(struct RTP_header) + rtp->length))
        {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    }
    //确认一次ack
    //使用select函数当作计时器
    //for(int i=0;i<rcb->window_size && len > 0;i++){
    int ret = rtp_select(sockfd);
    if (ret == 0)
    {     //超时 全部重新传输
        ; //return rcb->ep_num;
    }
    else
    { //sockfd 发生变化
        rtp_header_t rtp_ack;
        rtp_recvfrom(sockfd, (void *)&rtp_ack, sizeof(rtp_ack), flags, NULL, 0); //阻塞模式
        if (rtp_ack.type == RTP_ACK)
        { //证明收到了ack
            printf("accept ack %d\n", rtp_ack.seq_num);
            //if(rtp_ack.seq_num>=rcb->ep_num)
            rcb->ep_num = rtp_ack.seq_num;
        }
    }
    //}
    return rcb->ep_num;
}

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    // TODO: recv message  函数内需要记录的信息参数无法支持 所以只对确定包状况
    char buffer[2048];
    int recv_bytes = recvfrom(sockfd, buffer, len, flags, from, fromlen);
    if (recv_bytes < 0)
    {
        perror("receive error");
        exit(EXIT_FAILURE);
    }
    buffer[recv_bytes] = '\0';
    // extract header 取head部分
    rtp_header_t *rtp = (rtp_header_t *)buffer;

    // verify checksum 对其他部分进行校验和 无论收到什么数据都要进行校验和
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, rtp->length + sizeof(rtp_header_t));
    //校验和不同 删除数据包 并且不发送数据
    if (pkt_checksum != computed_checksum)
    {
        //printf("packet num %d length %d type%d \n",rtp->seq_num,rtp->length,rtp->type,rtp->checksum);
        perror("checksums not match");
        return -1;
    }
    if (rtp->type != RTP_END)
    { //保存信息
        memcpy(buf, buffer, rtp->length + sizeof(rtp_header_t));
    }
    else if (rtp->type == RTP_END)
    {
        //发送end_ack 监听一段时间end 如果这段时间没有收到end 则证明对方收到
        printf("accept end\n");
        ini_rtp(rtp, RTP_ACK, 0, rtp->seq_num, 1);
        printf("send end ack\n");
        sendto(sockfd, (void *)rtp, sizeof(rtp_header_t), flags, from, *fromlen);
        return -2;
    }
    return rtp->length + sizeof(rtp_header_t); //返回数据长度
}

int rtp_select(int sockfd)
{ //充当计时器功能
    fd_set fds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    int ret = select(sockfd + 1, &fds, NULL, NULL, &tv);
    if (ret == -1)
    {
        perror("select error");
        exit(EXIT_FAILURE);
    }
    return ret;
}

void ini_rtp(rtp_header_t *rtp, uint8_t type, uint16_t length, uint32_t seq_num, int compute)
{
    rtp->type = type;
    rtp->length = length;
    rtp->seq_num = seq_num;
    rtp->checksum = 0;
    if (compute)
        rtp->checksum = compute_checksum((void *)rtp, sizeof(rtp_header_t));
}
int op_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen)
{
    char buffer[2048];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    for (int i = 0; i < rcb->window_size && len > 0; i++)
    {
        memset(buffer, 0, sizeof(buffer));
        int find = 0; //发现已经被发送且确定
        for (int j = 0; j < rcb->window_size; j++)
        {
            if (rcb->ack[j] == i + rcb->ep_num)
            {
                find = 1;
                break;
            }
        }
        if (!find)
        {
            if (len >= data_size)
            {
                ini_rtp(rtp, RTP_DATA, data_size, i + rcb->ep_num, 0);
                len -= data_size;
            }
            else
            {
                ini_rtp(rtp, RTP_DATA, len, i + rcb->ep_num, 0);
                len = 0;
            }
            memcpy((void *)buffer + sizeof(rtp_header_t), msg + i * data_size, rtp->length);
            rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + rtp->length);
            int sent_bytes = sendto(sockfd, (void *)buffer, sizeof(rtp_header_t) + rtp->length, flags, to, tolen); //确保每个包都是1472
            printf("sent packed num %d\n", rtp->seq_num);

            if (sent_bytes != (sizeof(struct RTP_header) + rtp->length))
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
        }
    }
    //确认一次ack
    //使用select函数当作计时器
    int min_ep = rcb->ep_num;
    for (int i = 0; i < rcb->window_size; i++)
    { //ep_num 该窗口没传送的最小的包序号
        if (rcb->ack[i] < min_ep)
        {   int ret = rtp_select(sockfd);
            if (ret == 0)
            { //超时 重新传送
                rcb->ack[i] = -1;
            }
            else
            { //sockfd 收到ack
                rtp_header_t rtp_ack;
                rtp_recvfrom(sockfd, (void *)&rtp_ack, sizeof(rtp_ack), flags, NULL, 0);
                if (rtp_ack.type == RTP_ACK)
                {                 //证明收到了ack
                    int find = 0; //发现已经被发送且确定  不接受同样的ack
                    for (int j = 0; j < rcb->window_size; j++)
                    {
                        if (rcb->ack[j] == rtp_ack.seq_num)
                        {
                            find = 1;
                            break;
                        }
                    }
                    if (!find)
                    {
                        printf("accept ack %d ep_num %d\n", rtp_ack.seq_num,rcb->ep_num);
                        rcb->ack[i] = rtp_ack.seq_num;
                    }
                }
            }
        }
    }
    for(int i=0;i<rcb->window_size;i++){
        if(rcb->ack[i]==rcb->ep_num){
            rcb->ep_num++;
            i=0;
        }
    }
    //printf("send ep_num %d\n",rcb->ep_num);
    return rcb->ep_num;
}