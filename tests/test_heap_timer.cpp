#include <tests/test_heap_timer.h>

#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5
#define FD_LIMIT 65535
#define TIME_HEAP_CAP 64

static int pipefd[2];
static time_heap* hp_timer = new time_heap(TIME_HEAP_CAP);
static int epollfd = 0;

int rand_number() {
    srand((int)time(0));
    int ret = 1+(int)(10.0*rand()/(RAND_MAX+1.0));
    return ret;
}

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags != SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_handler() {
    // 定时处理任务，实际上就是调用tick函数
    hp_timer->tick();
    // 因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信息
    alarm(TIMESLOT);
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
void cb_func(client_data* user_data) {
    if (!user_data) return;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d \n", user_data->sockfd);
}

void test_heap_timer(const char* ip, int port) {

    int ret;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr*) &address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    // 设置信号处理函数
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;

    client_data* users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure \n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_addrlength);
                addfd(epollfd, connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;

                heap_timer* timer = new heap_timer;
                timer->expire = TIMESLOT * rand_number();
                timer->cb_func = cb_func;
                timer->user_data = &users[connfd];
                hp_timer->add_timer(timer);

                users[connfd].timer =  timer;

            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN )) {    // 信号处理
                char signals[1024];
                ret = recv(sockfd, signals, sizeof(signals) - 1, 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; i++) {
                        switch(signals[i]) {
                            case SIGALRM:
                            {
                                /**
                                 * 用timeout 变量标记有定时任务需要处理，但不立即处理定时任务
                                 * 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务
                                 */
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d \n", ret, users[sockfd].buf, sockfd);
                heap_timer* timer = users[sockfd].timer;

                if (ret < 0) {
                    // 如果发生读错误，则关闭连接，并移除对应的定时器
                    if (errno != EAGAIN) {
                        cb_func(&users[sockfd]);
                        if (timer) {
                            hp_timer->del_timer(timer);
                        }
                    }
                } else if (ret == 0) {
                    // 如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器
                    cb_func(&users[sockfd]);
                    if (timer) {
                        hp_timer->del_timer(timer);
                    }
                } else {
                }
            } else {

            }
        }
       
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }
}