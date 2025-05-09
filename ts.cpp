#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unordered_set>
#include <mutex>

// std::unordered_set<int> clients;
// std::mutex clients;

struct Clients : std::unordered_set<int> {
    std::mutex m;
} clients;

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

void usage() {
    printf("syntax: echo-server <port> [-e[-b]]\n");
    printf("  -e : echo\n");
    printf("  -b : broadcast\n");
    printf("sample: echo-server 1234 -e -b\n");
}

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        if (argc < 2) return false;
        port = atoi(argv[1]);

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
            }
            else if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
            }
        }
        return port != 0;
    }
} param;

void broadcastMessage(const char* message, size_t len, int exclude_sd) {
    std::lock_guard<std::mutex> lock(clients.m);
    for (int client_sd : clients) {
        if (client_sd != exclude_sd) {
            send(client_sd, message, len, 0);
        }
    }
}

void recvThread(int sd) {
    {
        std::lock_guard<std::mutex> lock(clients.m);
        clients.insert(sd);
    }

    printf("connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res <= 0) {
            fprintf(stderr, "recv return %zd", res);
            myerror(" ");
            break;
        }

        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);

        if (param.broadcast) {
            broadcastMessage(buf, res, sd);
        }
        else if (param.echo) {
            res = ::send(sd, buf, res, 0);
            if (res <= 0) {
                fprintf(stderr, "send return %zd", res);
                myerror(" ");
                break;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(clients.m);
        clients.erase(sd);
    }
    printf("disconnected\n");
    fflush(stdout);
    close(sd);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    // socket
    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }

    // setsockopt
    {
        int optval = 1;
        int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }

    // bind
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(param.port);

        ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1) {
            myerror("bind");
            return -1;
        }
    }

    // listen
    {
        int res = listen(sd, 5);
        if (res == -1) {
            myerror("listen");
            return -1;
        }
    }

    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
        if (newsd == -1) {
            myerror("accept");
            break;
        }
        std::thread* t = new std::thread(recvThread, newsd);
        t->detach();
    }

    ::close(sd);
    return 0;
}
