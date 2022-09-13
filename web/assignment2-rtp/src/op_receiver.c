#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "util.h"
#include "rtp.h"

//#define RECV_BUFFER_SIZE 32768  // 32KB

int receiver(char *receiver_port, int window_size, char *file_name)
{

  //char buffer[RECV_BUFFER_SIZE];

  // create rtp socket file descriptor
  int receiver_fd = rtp_socket(window_size);
  if (receiver_fd == 0)
  {
    perror("create rtp socket failed");
    exit(EXIT_FAILURE);
  }

  // create socket address
  // forcefully attach socket to the port
  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; //本机任意地址
  address.sin_port = htons(atoi(receiver_port));

  // bind rtp socket to address
  if (rtp_bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  int recv_bytes;
  struct sockaddr_in sender;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // listen to incoming rtp connection
  if (rtp_listen(receiver_fd, 1) < 0)
  {
    rtp_close(receiver_fd);
    return 0;
  }
  // accept the rtp connection
  rtp_accept(receiver_fd, (struct sockaddr *)&sender, &addr_len);

  int filefd = open(file_name, O_WRONLY);
  char buffer[window_size * 1472 + 1];
  memset(buffer, 0, sizeof(buffer));

  char buf[2048];

  int seq_num[window_size]; //只需要保留seq_num
  for (int i = 0; i < window_size; i++)
  {
    seq_num[i] = -1;
  }

  // receive packet 利用recvfrom确定发送端地址
  int ep_num = 0;
  while (1)
  {
    //对整个滑动窗口处理 从期待的ep_num开始
    //buf保存需要保存的数据到对应文件夹
    //先把buffer 存满 不对buffer拆解 直接全部返回
    //printf("rece ep_num %d\n",ep_num);
    for (int i = 0; i < window_size; i++)
    {
      if (seq_num[i] == -1)
      { //窗口存在位置
        memset(buf, 0, sizeof(buf));
        int recv_byte = rtp_recvfrom(receiver_fd, (void *)buf, 1472, 0, (struct sockaddr *)&sender, &addr_len);
       
        if (recv_byte == -2)
        { //收到end
          for (int i = 0; i < window_size; i++)
          {
            if (seq_num[i] == ep_num)
            {
              printf("accept packet num %d\n", ep_num);
              rtp_header_t *rtp = (rtp_header_t *)(buffer + i * 1472);
              write(filefd, buffer + i * 1472 + sizeof(rtp_header_t), rtp->length);
              memset(buffer + i * 1472, 0, 1472);
              seq_num[i] = -1;
              ep_num += 1;
              i = 0;
            }
          }
          rtp_close(receiver_fd);
          close(filefd);
          rcb_end();
          return 0;
        }
        //1.错误数据包忽视
        if (recv_byte == -1)
        {
          continue;
        }
        //2.已经接收到的包
        rtp_header_t *rtp = (rtp_header_t *)buf;
        if (rtp->seq_num < ep_num)
        {
          printf("repeat packet%d\n", rtp->seq_num);
          continue;
        }
        //3.如果存在不再储存
        int cached=0;
        for (int j = 0; j < window_size; j++)
        {
          if (seq_num[j] == rtp->seq_num)
          {
            printf("packet num %d cached\n", rtp->seq_num);
            rtp_header_t rtp_ack;
            ini_rtp(&rtp_ack,RTP_ACK,0,rtp->seq_num,1);
            sendto(receiver_fd, (void *)&rtp_ack, sizeof(rtp_header_t), 0, (struct sockaddr *)&sender, addr_len);
            cached=1;
            break;
          }
        }
        if(cached) continue;
        seq_num[i] = rtp->seq_num;
        memcpy(buffer + i * 1472, buf, recv_byte);
      }
    }
    for (int i = 0; i < window_size; i++)
    {
      if (seq_num[i] > ep_num + window_size||seq_num[i] < ep_num)
      {
        memset(buffer + i * 1472, 0, 1472);
        seq_num[i] = -1;
      }
      else
      {
        rtp_header_t *rtp = (rtp_header_t *)(buffer + i * 1472);
        //缓存的确定ack
        rtp_header_t rtp_ack;
        printf("cached packet %d\n",rtp->seq_num);
        ini_rtp(&rtp_ack,RTP_ACK,0,rtp->seq_num,1);
        sendto(receiver_fd, (void *)&rtp_ack, sizeof(rtp_header_t), 0, (struct sockaddr *)&sender, addr_len);
      }
    }
    //1.对于直接可以存储的包 直接返回ack确认
    for (int i = 0; i < window_size; i++)
    {
      if (seq_num[i] == ep_num)
      {
        printf("accept packet num %d\n", ep_num);
        rtp_header_t *rtp = (rtp_header_t *)(buffer + i * 1472);
        write(filefd, buffer + i * 1472 + sizeof(rtp_header_t), rtp->length);
        memset(buffer + i * 1472, 0, 1472);
        seq_num[i] = -1;
        ep_num += 1;
        i = 0;
      }
    }
  }
  //close(filefd);
  return 0;
}

/*
 * main():
 * Parse command-line arguments and call receiver function
*/
int main(int argc, char **argv)
{
  char *receiver_port;
  int window_size;
  char *file_name;

  if (argc != 4)
  {
    fprintf(stderr, "Usage: ./receiver [Receiver Port] [Window Size] [File Name]\n");
    exit(EXIT_FAILURE);
  }

  receiver_port = argv[1];
  window_size = atoi(argv[2]);
  file_name = argv[3];
  return receiver(receiver_port, window_size, file_name);
}
