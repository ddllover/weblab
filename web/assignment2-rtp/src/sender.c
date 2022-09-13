#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "rtp.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/*client*/
int sender(char *receiver_ip, char *receiver_port, int window_size, char *message)
{

  // create socket
  int sock = 0;
  if ((sock = rtp_socket(window_size)) < 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // create receiver address
  struct sockaddr_in receiver_addr;
  memset(&receiver_addr, 0, sizeof(receiver_addr));
  receiver_addr.sin_family = AF_INET;                  //ipv4
  receiver_addr.sin_port = htons(atoi(receiver_port)); //字符端口转接收方端口

  // convert IPv4 or IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr) <= 0)
  {
    perror("address failed");
    exit(EXIT_FAILURE);
  }

  // connect to server receiver
  if (rtp_connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) < 0)
  {
    rtp_close(sock);
    return 0;
  }
  printf("connect success!\n");

  char test_data[window_size * data_size + 1];
  int data_len = 0;
  memset(test_data, 0, sizeof(test_data));
  //从文件里面读取信息 或者 直接得到信息
  // TODO: if message is filename, open the file and send its content
  int filefd = -1;
  int message_len;
  int len = 0, last_len = 0;
  filefd = open(message, O_RDONLY);
  if (filefd >= 0)
  {
    printf("open file\n");
    int read_byte = read(filefd, test_data, window_size * data_size);
    data_len = read_byte;
  }
  else
  {
    message_len = strlen(message);
  }
  int read_ed = 0;

  while (1)
  { //优化 滑动可以用队列 目前简单粗暴直接重新复制
    if (filefd < 0)
    { //非文件信息 观察一下 总是段错误
      //printf("%s",message);
      if (len * data_size + window_size * data_size < message_len)
      {
        memcpy(test_data, message + len * data_size, window_size * data_size); //len标记需要发送的首个位置
        data_len = window_size * data_size;
      }
      else
      {
        if (message_len > len * data_size)
        {
          memcpy(test_data, message + len * data_size, message_len - len * data_size);
          data_len = message_len - len * data_size;
        }
        else{
          break;
        }
      }
      len = rtp_sendto(sock, (void *)test_data, data_len, 0, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
    }
    else
    { //文件信息
      int read_byte = 0;
      //printf("sendto\n");
      if (len > last_len)
      {
        data_len -= (len - last_len) * data_size;
        if (data_len > 0)
          memcpy(test_data, test_data + (len - last_len) * data_size, data_len);
        if (!read_ed && (read_byte = read(filefd, test_data + data_len, window_size * data_size - data_len)) == 0)
        {
          //printf("最后一个窗口\n %s\n 数据 %d",test_data,read_byte);
          read_ed = 1;
        }
        data_len += read_byte;
        last_len = len;
      }
      if (read_ed && data_len < 0)
      {
        break;
      }
      len = rtp_sendto(sock, (void *)test_data, data_len, 0, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
      //printf("datalen %d lastlen %d len %d\n",data_len,last_len,len);
    }
  }
  //发送end信息 500ms等待得到ack 否则直接结束
  rtp_header_t rtp;
  ini_rtp(&rtp, RTP_END, 0, 0, 1);
  sendto(sock, (void *)&rtp, sizeof(rtp_header_t), 0, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));
  if (rtp_select(sock) > 0)
  {                                                                 //超时直接忽略
    recvfrom(sock, (void *)&rtp, sizeof(rtp_header_t), 0, NULL, 0); //阻塞的还需要用select
    if (rtp.type == RTP_ACK && rtp.seq_num == 0)
    { //对面返回-1
      ;
    }
  }
  close(filefd);
  // close rtp socket
  rtp_close(sock);
  rcb_end();
  return 0;
}

/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv)
{
  char *receiver_ip;
  char *receiver_port;
  int window_size;
  char *message;

  if (argc != 5)
  {
    fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [message]");
    exit(EXIT_FAILURE);
  }

  receiver_ip = argv[1];
  receiver_port = argv[2];
  window_size = atoi(argv[3]);
  message = argv[4];
  return sender(receiver_ip, receiver_port, window_size, message);
}