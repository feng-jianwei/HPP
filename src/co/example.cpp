#include <exception>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <coroutine>
#include <iostream>
#include <stdexcept>

struct Task {
    struct promise_type {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> h;
    explicit Task(std::coroutine_handle<promise_type> h_) : h(h_) {}
    ~Task() { if (h) h.destroy(); }
    void resume() { if (h && !h.done()) h.resume(); }
    bool done() const { return !h || h.done(); }
};

class Reactor {
public:
    Reactor() {
        epfd = epoll_create1(0);
        if (epfd == -1) throw std::runtime_error("epoll_create1 failed");
    }

    ~Reactor() { close(epfd); }

    void run() {
        while (true) {
            // 处理就绪协程队列
            while (!ready_queue.empty()) {
                auto h = ready_queue.front();
                ready_queue.pop_front();
                h.resume();
            }

            epoll_event events[128];
            int n = epoll_wait(epfd, events, 128, -1);
            if (n == -1) break;

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                // uint32_t ev = events[i].events; // 可以根据 ev 处理错误等

                auto it = waiting_coros.find(fd);
                if (it != waiting_coros.end()) {
                    ready_queue.push_back(it->second);
                    // 不清除 waiting_coros, 因为协程消费后会再次 co_await 设置
                }
            }
        }
    }

    struct AwaitFd {
        Reactor& reactor;
        int fd;
        uint32_t events;

        bool await_ready() { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            reactor.waiting_coros[fd] = h;

            if (reactor.registered_fds.find(fd) == reactor.registered_fds.end()) {
                epoll_event ev{};
                ev.events = events | EPOLLET;
                ev.data.fd = fd;
                if (epoll_ctl(reactor.epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                    throw std::runtime_error("epoll_ctl add failed");
                }
                reactor.registered_fds.insert(fd);
            }
            // 如果 events 变了，可以 EPOLL_CTL_MOD，但这里假设 events 固定
        }

        void await_resume() {}
    };

    AwaitFd await_event(int fd, uint32_t events) {
        return {*this, fd, events};
    }

private:
    int epfd;
    std::deque<std::coroutine_handle<>> ready_queue;
    std::unordered_map<int, std::coroutine_handle<>> waiting_coros;
    std::unordered_set<int> registered_fds;
};

Task echo_server(Reactor& reactor, int client_fd) {
    char buf[1024];
    while (true) {
        co_await reactor.await_event(client_fd, EPOLLIN | EPOLLET);

        // 循环读直到 EAGAIN (ET 要求)
        ssize_t n = 0;
        while (true) {
            ssize_t r = read(client_fd, buf + n, sizeof(buf) - n);
            if (r > 0) {
                buf[n + 1] = '\0';
                printf("read: %s", buf);
                n += r;
            } else if (r == 0) {
                break;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                n = -1; // 错误
                break;
            }
        }
        if (n <= 0) break;

        // 循环写直到 EAGAIN
        ssize_t sent = 0;
        while (sent < n) {
            co_await reactor.await_event(client_fd, EPOLLOUT);

            ssize_t w = 0;
            while (true) {
                ssize_t ww = write(client_fd, buf + sent + w, n - sent - w);
                if (ww > 0) {
                    w += ww;
                } else if (ww == 0) {
                    break;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    w = -1;
                    break;
                }
            }
            if (w <= 0) break;
            sent += w;
        }
    }
    close(client_fd);
}

Task acceptor(Reactor& reactor, int listen_fd) {
    while (true) {
        co_await reactor.await_event(listen_fd, EPOLLIN);

        // 循环 accept 直到 EAGAIN (ET 要求)
        while (true) {
            int client_fd = accept(listen_fd, nullptr, nullptr);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                continue;
            }

            fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);

            // 启动 echo 协程
            Task echo = echo_server(reactor, client_fd);
            echo.resume();
        }
    }
}

int main() {
    try {
        Reactor reactor;

        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) throw std::runtime_error("socket failed");
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL) | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) == -1) throw std::runtime_error("bind failed");
        if (listen(listen_fd, 128) == -1) throw std::runtime_error("listen failed");

        // 启动 acceptor 协程，并初始挂起等待事件
        Task acc = acceptor(reactor, listen_fd);
        reactor.await_event(listen_fd, EPOLLIN).await_suspend(acc.h);

        std::cout << "Server listening on :8080" << std::endl;

        reactor.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}