#include <tests/test_wheel_timer.h>

#define MAX_EVENT_NUMBER 1024

static int pipefd[2];
static time_wheel wh_timer;
static int g_sec = 0;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void sig_handler(int sig)
{
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

void do_time1(client_data* user_data)
{
    printf("timer %s, %d\n", __FUNCTION__, g_sec++);
    tw_timer* timer = wh_timer.add_timer(1);
    timer->cb_func = do_time1;
}

void timer_handler() {
    // 定时处理任务，实际上就是调用tick函数
    wh_timer.tick();
    // 因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信息
    alarm(1);
}

void test_wheel_timer() {
    addsig(SIGALRM);
    alarm(1); // 1s的周期心跳


    bool timeout = false;
    int num = 0;
    int ret;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);

    while(1) {
        num++;
        // printf("num: %d\n", num);
        if (num % 20 == 0) {
            tw_timer* timer = wh_timer.add_timer(1);
            timer->cb_func = do_time1;
        }
        char signals[1024];
        ret = recv(pipefd[0], signals, sizeof(signals), 0);
        if (ret == -1) {
            // handler the error
            continue;
        } else if (ret == 0) {
            continue;
        } else {
            for (int i = 0; i < ret; ++i) {
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
                        // stop_server = true;
                    }
                }
            }
        }
       
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }
}