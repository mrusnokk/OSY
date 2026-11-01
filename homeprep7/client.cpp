#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "zoo_map.h"

constexpr int idle_timeout = 5000;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        perror("ERROR: run the program with agrs <ip> <port>");
        return -1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    printf("Name: ");
    char jmeno[64];
    if (!fgets(jmeno, sizeof(jmeno), stdin))
    {
        perror("FGETS ERROR : Cannot process name!\n");
        return EXIT_FAILURE;
    }
    jmeno[strcspn(jmeno, "\r\n")] = '\0';

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        return EXIT_FAILURE;
    }

    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc < 0)
    {
        perror("Socket ERROR\n");
        return EXIT_FAILURE;
    }

    if (connect(soc, (const sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Cnnect ERROR");
        close(soc);
        return EXIT_FAILURE;
    }

    srand(time(nullptr));
    map_init(10, 10);

    char hello_buf[128];
    snprintf(hello_buf, sizeof(hello_buf), "HELLO %s\n", jmeno);
    if (write(soc, hello_buf, strlen(hello_buf)) < 0)
    {
        perror("Write ERROR");
        close(soc);
        return EXIT_FAILURE;
    }

    const char *moves[] = {"MOVE NORTH\n", "MOVE SOUTH\n", "MOVE WEST\n", "MOVE EAST\n"};

    pollfd fds[2]{};
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = soc;
    fds[1].events = POLLIN;

    char buf[1024];

    while (1)
    {
        int pr = poll(fds, 2, idle_timeout);
        if (pr < 0)
        {
            if (errno == EINTR)
                continue;
            perror("Poll ERROR");
            break;
        }

        if (pr == 0)
        {
            const char *mv = moves[rand() % 4];
            if (write(soc, mv, strlen(mv)) < 0)
            {
                perror("WRITE ERROR");
                break;
            }
            continue;
        }

        if (fds[0].revents & POLLIN)
        {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0)
                break;
            if (write(soc, buf, n) < 0)
            {
                perror("WRITE ERROR");
                break;
            }
        }

        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP))
        {
            ssize_t n = read(soc, buf, sizeof(buf) - 1);
            if (n <= 0)
                break;
            buf[n] = '\0';

            char *line = strtok(buf, "\n");
            while (line != nullptr)
            {
                int px, py;
                char area[128] = {0};
                if (sscanf(line, "POS %d %d AREA %127s", &px, &py, area) == 3)
                {
                    area[strcspn(area, "\r\n")] = '\0';
                    update_map(px, py, area);
                    draw_map();
                }
                else
                {
                    printf("%s\n", line);
                }
                line = strtok(nullptr, "\n");
            }
        }
    }

    close(soc);
    return EXIT_SUCCESS;
}