#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include "liburing.h"
#include <sys/poll.h>


constexpr unsigned int BUF_SIZE = 4;
constexpr unsigned int BUF_COUNT = 1024;
constexpr unsigned int BGID = 0;     // buffer group id，随便取一个唯一值
constexpr unsigned int QUEUE_DEPTH = 128;

enum op_type {
    OP_ACCEPT = 1,
    OP_READ   = 2,
    OP_WRITE  = 3,
};

struct conn {
    int fd;
    char buf[BUF_SIZE];
    int buf_len;
    int closed;
};

struct io_uring_buf_ring *br;

static void fatal(const char *msg) {
    perror(msg);
    exit(1);
}

static struct conn conns[1024];

static void add_poll(io_uring *ring, int fd, unsigned poll_mask, __u64 user_data) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) fatal("get sqe");

    io_uring_prep_poll_multishot(sqe, fd, poll_mask);

    sqe->user_data = user_data;
    io_uring_submit(ring);
}

int main()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) fatal("socket");

    int val = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(9981),
        .sin_addr = {INADDR_ANY},
        .sin_zero = {},
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        fatal("bind");

    if (listen(listen_fd, SOMAXCONN) < 0)
        fatal("listen");

    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    auto ring = new io_uring;
    io_uring_queue_init(QUEUE_DEPTH, ring, 0);
    int ret = 0;
    br = io_uring_setup_buf_ring(ring, BUF_COUNT, BGID, 0, &ret);
    if (!br) {
        fprintf(stderr, "setup_buf_ring failed: %s\n", strerror(-ret));
        exit(1);
    }
    for (unsigned i = 0; i < BUF_COUNT; i++) {
        void *buf = aligned_alloc(4096, BUF_SIZE); 
        io_uring_buf_ring_add(br, buf, BUF_SIZE, BGID, 
            io_uring_buf_ring_mask(BUF_COUNT), i);;
    }
    // 4. 告诉内核：我加完了，更新尾指针
    io_uring_buf_ring_advance(br, BUF_COUNT);
    printf("Listening on :9981 with io_uring multishot poll\n");
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) fatal("get sqe accept");

    io_uring_prep_multishot_accept(sqe, listen_fd, NULL, NULL, 0);
    sqe->user_data = (uint64_t)OP_ACCEPT << 32 | (uint64_t)listen_fd;
    if (io_uring_submit(ring) < 0){
         fatal("io_uring_submit");  // 通常 accept 立即提交
    }
    struct io_uring_cqe *cqe;
    struct __kernel_timespec ts = { .tv_sec = 1, .tv_nsec = 500000000 };  // 1.5 秒
    while (1) {
        int ret = io_uring_wait_cqes(ring, &cqe, 1, &ts, NULL);
        if (ret < 0) {
            continue;
        }
        do {
             // unsigned int op = cqe->user_data >> 32;
            int bid = cqe->flags >> 16; // ← 这里得到 buffer id
            int len = cqe->res;
            auto op = cqe->user_data >> 32;
            auto idx_or_fd = cqe->user_data & 0xFFFFFFFFU;
            switch (op) {
                case OP_ACCEPT: {
                    int client_fd = cqe->res;
                    if (client_fd >= 0) {
                        printf("→ new connection: fd=%d\n", client_fd);

                        fcntl(client_fd, F_SETFL, O_NONBLOCK);

                        if (client_fd >= 65536) {
                            close(client_fd);
                            break;
                        }

                        conns[client_fd].fd = client_fd;
                        conns[client_fd].closed = 0;

                        // 注册 poll（模拟 epoll_ctl EPOLL_CTL_ADD EPOLLIN|EPOLLET）
                        add_poll(ring, client_fd,
                                        POLLIN | POLLERR | POLLHUP,
                                        (uint64_t)OP_READ << 32 | client_fd);

                        // 或者直接投 multishot read（更推荐）
                        // add_multishot_read(client_fd, client_fd);
                    }
                    // 继续保持 multishot accept 活跃（内核会自动重投）
                    break;
                }

                case OP_READ: {
                    int fd = idx_or_fd;
                    if (cqe->res <= 0) {
                        // 连接关闭或错误
                        close(fd);
                        conns[fd].closed = 1;
                        break;
                    }

                    // 收到数据（边缘触发，可能一次只给部分）
                    // 这里简单 echo
                    conns[fd].buf_len = read(fd, conns[fd].buf, BUF_SIZE);
                    if (conns[fd].buf_len <= 0) {
                        if (conns[fd].buf_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                            close(fd);
                            conns[fd].closed = 1;
                        }
                        break;
                    }
                    conns[fd].buf[conns[fd].buf_len] ='\0';
                    printf("收到 %d 字节: %s...\n", conns[fd].buf_len, conns[fd].buf);

                    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
                    io_uring_prep_write(sqe, fd, conns[fd].buf, conns[fd].buf_len, 0);
                    sqe->user_data = (uint64_t)OP_WRITE << 32 | fd;

                    if (io_uring_submit(ring) < 0){
                        fatal("io_uring_submit");
                    }

                    // 继续保持 multishot read 活跃（如果用了 multishot read 方式）
                    break;
                }

                case OP_WRITE: {
                    // 写完成，通常不用再做什么（如果缓冲区还有剩余可以继续写）
                    break;
                }

                default:
                    fprintf(stderr, "Unknown op: %llu\n", op);
            };
            // printf("收到 %d 字节 from buffer %d: %s...\n", len, bid, buf);

            // // 处理完数据后，**必须归还 buffer**
            // io_uring_buf_ring_add(br, buf, BUF_SIZE, bid, 0, bid);
            // io_uring_buf_ring_advance(br, 1);

            // 继续保持 multishot：再提交一个相同的 recv（或依赖内核自动保持）
            io_uring_cqe_seen(ring, cqe);   // 每个都要 seen！
            // 尝试拿下一个（不阻塞）
        } while (io_uring_peek_cqe(ring, &cqe) == 0);
    }
    return 0;
}
