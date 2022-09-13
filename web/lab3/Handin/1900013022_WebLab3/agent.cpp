#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>
//sender client
#define debug
using namespace std;
class Router_node
{
public:
    char ip[20];
    char port[10];
    int id;
} router_node[11];
class Router_tab
{ //路由表
public:
    int dest;
    int next;
    int cost;
} router_tab[11]; //用来接收show
int num;
class connect_socket{
    public:
    bool link;
    int sock;
}receiver_sock[11];
bool update();
unordered_map<int, int> router_i;
int send(char *receiver_ip, char *receiver_port, char *command)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET; //ipv4
    inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr);
    receiver_addr.sin_port = htons(atoi(receiver_port)); //字符端口转接收方端口
    printf("[info] connecting.....\n");
    while(connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) < 0)
    {
        //printf("[info] %s\n",strerror(errno));
        //close(sock);
        //return 0;
    }
    printf("[info] connect success!\n");
    write(sock, command, strlen(command));
    if (command[0] == 's')
    {
        read(sock, (void *)router_tab, num * sizeof(Router_tab));
        for (int i = 0; i < num; i++)
        {
            if (router_tab[i].cost != -1)
            {
                printf("dest: %d, next: %d, cost: %d\n", router_tab[i].dest, router_tab[i].next, router_tab[i].cost);
            }
        }
    }
    else
    {
        int ack = 0;
        if (read(sock, (char *)&ack, sizeof(ack)) <= 0) //读取read自带阻塞
        {
            printf("[info] the other side has been closed.\n");
        }
        if (ack == 1)
        {
            printf("[info] ack get\n");
        }
    }
    close(sock);
    return 1;
}
bool compare(Router_node &a, Router_node &b)
{
    return a.id < b.id;
}
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0); //测试使用
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./agent <router location file>");
        exit(EXIT_FAILURE);
    }
    //printf("测试开始\n");
    FILE *fp = fopen((char *)argv[1], "r");
    if (fp == nullptr)
    {
        fprintf(stderr, "无法打开");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%d", &num);
    for (int i = 0; i < num; i++)
    {
        fscanf(fp, "\n%[^,],%[^,],%d", router_node[i].ip, router_node[i].port, &router_node[i].id);
        //printf("%s %s %d\n", router_node[i].ip, router_node[i].port, router_node[i].id);
        
    }
    sort(router_node, router_node + num, compare);
    for(int i=0;i<num;i++){
        router_i.emplace(router_node[i].id, i);
    }
    fclose(fp);
    char command[100] = {0};
    while (cin >> command)
    {
        if (command[0] == 'd')
        { //dv
            for (int i = 0; i < num; i++)
            {
                send(router_node[i].ip, router_node[i].port, command);
            }
        }
        else if (command[0] == 'u')
        { //update
            int id;
            sscanf(command + 7, "%d", &id);
            send(router_node[router_i[id]].ip, router_node[router_i[id]].port, command);
        }
        else if (command[0] == 's')
        {
            int id;
            sscanf(command + 5, "%d", &id);
            //printf("[info] show:%d\n", id);
            send(router_node[router_i[id]].ip,  router_node[router_i[id]].port, command);
        }
        else if (command[0] == 'r')
        {
            int id;
            sscanf(command + 6, "%d", &id);
            send(router_node[router_i[id]].ip,  router_node[router_i[id]].port, command);
        }
        else
        {
            perror("command error\n");
            exit(EXIT_FAILURE);
        }
        memset(command, 0, sizeof(command));
    }
    //1、读取路由位置 监听所有位置
    //从命令行接收命令
    //1、dv
    //2、update:1,6,4
    //3、show:2
    //4、reset:2
    //根据命令向router 发送信息
    return 0;
}