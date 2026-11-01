#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <chrono>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

constexpr int idle = 5000;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Zadej ip a port!\n");
        return -1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "Neplatna IP adresa: %s\n", ip);
        return EXIT_FAILURE;
    }

    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(soc, (const sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(soc);
        return EXIT_FAILURE;
    }

    srand(time(nullptr));

    const char *moves[] = {"MOVE NORTH\n", "MOVE SOUTH\n",
                           "MOVE WEST\n", "MOVE EAST\n"};

    pollfd fds[2]{};
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = soc;
    fds[1].events = POLLIN;

    char buf[1024];

    while (1)
    {
        int pr = poll(fds, 2, idle);

        if (pr < 0)
        {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        if (pr == 0)
        {
            const char *mv = moves[rand() % 4];
            printf("> %s", mv);
            fflush(stdout);

            if (write(soc, mv, strlen(mv)) < 0)
            {
                perror("write");
                break;
            }
            continue;
        }

        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0)
                break;

            if (write(soc, buf, n) < 0)
            {
                perror("write");
                break;
            }
        }

        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR))
        {
            ssize_t n = read(soc, buf, sizeof(buf));
            if (n < 0)
            {
                perror("read");
                break;
            }
            if (n == 0)
                break;

            if (write(STDOUT_FILENO, buf, n) < 0)
            {
                perror("write");
                break;
            }
        }
    }

    close(soc);
    return EXIT_SUCCESS;
}