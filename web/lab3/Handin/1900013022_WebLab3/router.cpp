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
#include <pthread.h>
#include <unordered_map>
#include <algorithm>
#include <queue>
//receiver
using namespace std;
//#define debug
FILE *out;
int router_dis[11][11] = {0}; //dv值
int id;
int id_i;
int num;
int router_nei_wei[11] = {0}; //记录是否和邻接点的w

queue<char *> q_command;
pthread_t ntid;

class Router_node //路由节点,用i来查表
{
public:
    char ip[20];
    char port[10];
    int id;
} router_node[11];

unordered_map<int, int> router_i; //从id查位置i

class Router_tab
{ //路由表
public:
    int dest;
    int next;
    int cost;
} router_tab[11]; //用来单独缓存本id的dv值
bool update();

void print(const char *str)
{

#ifdef debug
    fprintf(out, "%s", str);
#endif
    return;
}
void print_dis()
{
#ifdef debug
    fprintf(out, "[info] router_dis\n");
    for (int i = 0; i < num; i++)
    {
        fprintf(out, "[info]:id(%d)", router_node[i].id);
        for (int j = 0; j < num; j++)
        {
            fprintf(out, "%d ", router_dis[i][j]);
        }
        print("\n");
    }
    print("[info] router_nei_wei\n");
    for (int i = 0; i < num; i++)
    {
        fprintf(out, "%d ", router_nei_wei[i]);
    }
    print("\n");
#endif
    return;
}
int send(char *receiver_ip, char *receiver_port) //发送完关闭
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET; //ipv4
    inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr);
    receiver_addr.sin_port = htons(atoi(receiver_port)); //字符端口转接收方端口
    print("[info] connecting\n");
    while (connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) < 0)
    {   
    }
    print("[info] connect success!\n");

    char command[2048] = {0};
    sprintf(command, "M:"); //M标记一下 并告诉自己是的位置
    memcpy(command + 2, (void *)&id_i, sizeof(int));
    print("[info]:send command\n");
    memcpy(command + 6, (void *)(&router_dis[id_i][0]), num * sizeof(int));

    print_dis();
    write(sock, command, 6 + num * sizeof(int));

    int ack = 0;
    if (read(sock, (void *)&ack, sizeof(ack)) == 0) //读取read自带阻塞
    {
        print("[info] the other side has been closed.\n");
    }
    if (ack == 1)
    {
        print("[info] get ack\n");
    }
    close(sock);
    return 1;
}
int exchange()
{
    if (update())
    { //只从邻近路由更新
        for (int i = 0; i < num; i++)
        { //对于大于1000的断开
            if (router_dis[id_i][i] > 1000)
            {
                router_dis[id_i][i] = -1;
                router_tab[i].cost = -1;
                router_nei_wei[i] = -1;
            }
        }
        for (int i = 0; i < num; i++)
        { //可达到把自己的更新传播
            if (router_nei_wei[i] != -1)
            {
                char temp[100] = {0};
                sprintf(temp, "[info]send other router id %d\n", router_node[i].id);
                print(temp);
                send(router_node[i].ip, router_node[i].port);
            }
        }
    }
    return 0;
}
bool update()
{
    bool change = false;
    print_dis();

    for (int i = 0; i < num; i++) //重新计算自己的dv
    {
        if (i == id_i)
        {
            router_tab[i].cost = 0;
            router_dis[id_i][i] = 0;
            continue;
        }
        int min = 1001;
        for (int j = 0; j < num; j++)
        {
            if (router_nei_wei[j] != -1 && router_dis[j][i] != -1 && j != id_i)
                if (min > router_nei_wei[j] + router_dis[j][i])
                {
                    min = router_nei_wei[j] + router_dis[j][i];
                    router_tab[i].next = router_node[j].id;
                }
        }
        min = min > 1000 ? -1 : min;
        if (min != router_tab[i].cost)
        {
            router_tab[i].cost = min;
            router_dis[id_i][i] = min;
            change = true;
        }
    }

    print_dis();
    return change;
}
int handle(char *command)
{
    if (command[0] == 'd')
    { //dv
        for (int i = 0; i < num; i++)
        {
            if (i != id_i)
            {
                if (router_nei_wei[i] != -1)
                {
#ifdef debug
                    fprintf(out, "[info]send other router id %d\n", router_node[i].id);
#endif
                    send(router_node[i].ip, router_node[i].port);
                }
            }
        }
    }
    else if (command[0] == 'u')
    { //update
        int s_id, t_id, w;
        sscanf(command + 7, "%d,%d,%d", &s_id, &t_id, &w);
        int t_i = router_i[t_id];
        w = w > 1000 ? -1 : w;
        router_nei_wei[t_i] = w;
        /// 要看发生变化了没有 如果没有不发送
        update();
    }
    else if (command[0] == 'M')
    { //得到别人dv更新的信息
        int t_i;
        memcpy(&t_i, command + 2, sizeof(int));
#ifdef debug
        fprintf(out, "[info]:receive command id_i:%d\n", t_i);
#endif
        memcpy(&router_dis[t_i][0], command + 6, num * sizeof(int));
        print_dis();
        exchange();
    }

    else if (command[0] == 'r')
    {
        int id;
        sprintf(command + 6, "%d", id);
        //删除自己节点()
        //send(router_i[id].ip,router_i[id].port);
    }
    else
    {
        perror("command error\n");
        exit(EXIT_FAILURE);
    }
}

void *thr_fn(void *arg)
{
    while (true)
    {
        if (!q_command.empty())
        {
            char *command = q_command.front();
            handle(command);
            q_command.pop();
        }
    }
}
int receiver(char *receiver_port) //点对点，需要建立链接，建立实现功能后结束
{

    struct sockaddr_in address, sender;
    socklen_t sender_len;

    int listenfd, connfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    //inet_pton(AF_INET, receiver_ip, &address.sin_addr);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(atoi(receiver_port));

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(listenfd, (struct sockaddr *)&address, sizeof(address));

    listen(listenfd,128);
    print("[info] accepting....\n");
    //让receiver部分保持活跃 防止阻塞
    pthread_create(&ntid, nullptr, thr_fn, nullptr);
    while (true)
    {
        char *command = new char[2048];
        memset(command, 0, sizeof(command));
        sender_len = sizeof(sender);
        connfd = accept(listenfd, (struct sockaddr *)&sender, &sender_len);
        if (connfd == -1)
        {
            perror("accept error\n");
            exit(0);
        }
        print("[info]accept success\n");
        int n = read(connfd, command, 2048);
        if (n <= 0)
        {
            close(connfd);
        }
        else
        {
            print("[info] receive command\n");

            int ack = 1;
            print("[info] send ack\n");
            if (command[0] == 's')
            {   
                /*while(!q_command.empty()){

                }; //用此操作将receive和handle部分同步*/
                write(connfd, (void *)router_tab, num * sizeof(Router_tab));
                for (int i = 0; i < num; i++)
                { //路径不可达不能输出
#ifdef debug
                    fprintf(out, "dest: %d, next: %d, cost: %d\n", router_tab[i].dest, router_tab[i].next, router_tab[i].cost);
#endif
                }
            }
            else
            {
                write(connfd, (void *)&ack, sizeof(ack)); //发送ack确定
                // 此次连接结束
                q_command.push(command);
            }
            close(connfd);
            //handle(command);
        }
    }
    //读取的信息一类为agent发送 另一类为其他相邻router发送 router的发送也用router加router_dis自己的那行
    return 0;
}

bool compare(Router_node &a, Router_node &b)
{
    return a.id < b.id;
}

int main(int argc, char **argv)
{
    //setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 4)
    {
        fprintf(stderr, "Usage: ./router <router location file> <topology conf file> <router id>");
        exit(EXIT_FAILURE);
    }
#ifdef debug
    char name[100] = "/mnt/c/Users/Lenovo/Desktop/Linux/lab3/Handin/web/";
    sprintf(name + strlen(name), "%s.txt", argv[3]);
    out = fopen(name, "w+");
    setvbuf(out, NULL, _IONBF, 0);
#endif

    sscanf(argv[3], "%d", &id);

    FILE *fp = fopen(argv[1], "r");

    fscanf(fp, "%d", &num);
    for (int i = 0; i < num; i++)
    {
        fscanf(fp, "\n%[^,],%[^,],%d", router_node[i].ip, router_node[i].port, &router_node[i].id);
#ifdef debug
        fprintf(out, "[info] %s %s %d\n", router_node[i].ip, router_node[i].port, router_node[i].id);
#endif
        for (int j = 0; j < num; j++)
        {
            router_dis[i][j] = -1;
            if (i == j)
                router_dis[i][j] = 0;
        }
    }
    sort(router_node, router_node + num, compare);

    for (int i = 0; i < num; i++)
    {
        router_i.emplace(router_node[i].id, i);
        router_tab[i].dest = router_node[i].id;
        router_tab[i].next = router_node[i].id;
        router_tab[i].cost = -1;
        router_nei_wei[i] = -1;
        if (router_node[i].id == id)
        {
            router_tab[i].cost = 0;
            router_nei_wei[i] = 0;
        }
    }

    id_i = router_i[id];
#ifdef debug
    fprintf(out, "[info] router id:%d id_i:%d\n", id, id_i);
#endif

    /*for (int i = 0; i < num; i++)
        { //路径不可达不能输出
            //if (router_tab[i].cost != -1)
            printf("%d\n",router_node[i].id);
                printf("%d,%d,%d\n", router_tab[i].dest, router_tab[i].next, router_tab[i].cost);
        }*/
    fclose(fp);

    //读入自己的id

    int edge;
    fp = fopen(argv[2], "r+");
    fscanf(fp, "%d", &edge);
    int s_id, t_id, w;
    for (int i = 0; i < edge; i++)
    {
        fscanf(fp, "%d,%d,%d", &s_id, &t_id, &w);
#ifdef debug
        fprintf(out, "%d,%d,%d\n", s_id, t_id, w);
#endif

        if (s_id == id || t_id == id)
        {
            if (t_id == id)
                swap(s_id, t_id);
            int t_i = router_i[t_id];
            w = w > 1000 ? -1 : w;
            //初始化邻接节点
            router_nei_wei[t_i] = w;
            //初始化路由表
            router_tab[t_i].dest = t_id;
            router_tab[t_i].next = t_id;
            router_tab[t_i].cost = w;
            //距离向量
            router_dis[id_i][t_i] = w;
        }
    }
    fclose(fp);
    print_dis();

    receiver(router_node[id_i].port);

    pthread_cancel(ntid);
#ifdef debug
    fclose(out);
#endif
    return 0;
}