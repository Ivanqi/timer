### Timer
#### 基于升序链表定时器
- 说明
  - 升序定时器链表的实际应用 -- 处理非活动连接
  - 服务器程序通常要定时处理非活动连接：给客户端发一个重连请求，或者关闭该连接，或者是其他的
  - 头文件: "<src/lst_timer.h>"
- 数据结构
  - 定时器链表：用于存储定时器类的任务。双向链表，且带有头节点和尾节点
  - 定时器类：
    - 任务超时时间
    - 任务回调函数
    - 指向前后定时器
  - 代码展示
    ```
    // 定时器类
    class util_timer 
    {
        public:
            util_timer(): prev(NULL), next(NULL) {}
        public:
            time_t expire;                      // 任务的超时时间，这里使用绝对时间
            void (*cb_func)(client_data*);      // 任务回调函数

            // 回调函数处理的客户数据，由定时器的执行者传递给回调函数
            client_data* user_data;
            util_timer* prev;                   // 指向前一个定时器
            util_timer* next;                   // 指向下一个定时器
    };
    ```
- 定期器函数
  - 增加定时器
    - 新的定时器超时时间 < 当前链表中所有定时器的超时时间
      - 把新定时器插入链表头部，成为链表新的头节点
    - 新的定时器超时时间 > 当前链表中所有定时器的超时时间
      - 从头节点开始遍历，寻找比新定时器大的定时器。然后插入到寻找到链表之前
      - 如果遍历所有定时器都找不到，就加入到尾节点。成为新的尾节点
    - 定时器链表顺序
      - 早 -> 晚（小 -> 大）
    - 调用方法
        ```
        users[connfd].address = client_address;
        users[connfd].sockfd = connfd;
        util_timer* timer = new util_timer;
        timer->user_data = &users[connfd];
        timer->cb_func = cb_func;
        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        users[connfd].timer =  timer;
        timer_lst.add_timer(timer);
        ```
  - 删除定时器
    - 链表中只有一个节点
      - 直接删除要删除的定时器
    - 要删除的定时器是头节点
      - 头节点的下一个节点，替换头节点。并把上一个节点置空
      - 直接删除要删除的定时器
    - 要删除的定时器是尾节点
      - 尾节点的上一个节点，替换尾节点。并把下一个节点置空
      - 直接删除要删除的定时器
    - 其他
      - 要删除的定时器的上下节点交换。典型的链表的节点交换
      - 删除要删除的定时器
    - 调用方法
      ```
      timer_lst.del_timer(timer);
      ```
  - 调整定时器
    - 超时时间检测
      - 如果被调整的定时器的超时时间小于它的下一个定时器的超时时间。则不用调整
    - 如果标定时器是链表的头节点
      - 则将该定时器从链表中取出，删除原来该定时器的位置
      - 并重新插入链表
    - 如果该定时器不是链表的头节点
      - 则将该定时器从链表中取出，删除原来该定时器的位置
      - 后插入其原来的位置之后的部分链表中
    - 调用方法
      ```
      time_t cur = time(NULL);
      timer->expire = cur + 3 * TIMESLOT;
      timer_lst.adjust_time(timer);
      ```
  - 触发定时器
    - 通过alarm(5).函数设置系统闹钟,每5秒触发一次闹钟
    - 检测链表的合法性。链表是否还没有初始化或者为空
    - 获取当前时间，遍历定时器链表。判断当前链表超时时间是否大于当前时间
      - 大于
        - 执行当前定时器的回调函数
        - 从定时器链表中删除
      - 小于则直接退出
    - 调用方法
    ```
    alarm(5);
    timer_lst.tick();
    ```
#### 时间轮
#### 时间堆