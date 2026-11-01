#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <ctime>
#include <csignal>
#include <sys/wait.h>

#define STR_CLOSE "close"
#define STR_QUIT "quit"

void serve_player(int cl)
{
    char buffer[1024];
    char name[64] = "";
    bool registered = false;
    srand(time(nullptr) ^ getpid());
    int x = rand() % 10;
    int y = rand() % 10;
    char response[256];
    pollfd pfd{};
    pfd.fd = cl;
    pfd.events = POLLIN;

    while (1)
    {
        pfd.revents = 0;
        int pr = poll(&pfd, 1, -1);

        if (pr < 0)
        {
            if (errno == EINTR)
                continue;

            perror("poll");
            break;
        }

        if (pr == 0)
        {
            const char *msg = "ERROR Timeout, disconnecting\n";
            if (write(cl, msg, strlen(msg)) < 0)
                perror("write");
            break;
        }

        if (pfd.revents & (POLLERR | POLLNVAL))
        {
            fprintf(stderr, "Chyba na socketu klienta\n");
            break;
        }

        ssize_t bytes = read(cl, buffer, sizeof(buffer) - 1);

        if (bytes <= 0)
        {
            if (bytes < 0)
                perror("read");

            break;
        }
        buffer[bytes] = '\0';

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strncmp(buffer, "HELLO ", 6) == 0)
        {
            if (registered)
            {
                snprintf(response, sizeof(response),
                         "ERROR Already registered\n");
            }
            else if (buffer[6] == '\0')
            {
                snprintf(response, sizeof(response),
                         "ERROR Missing name\n");
            }
            else
            {
                snprintf(name, sizeof(name), "%s", buffer + 6);
                registered = true;

                snprintf(response, sizeof(response),
                         "WELCOME %s (%d, %d)\n", name, x, y);
            }
        }
        else if (strcmp(buffer, "QUIT") == 0)
        {
            snprintf(response, sizeof(response), "BYE %s\n", name);
        }
        else if (strcmp(buffer, "WHERE") == 0)
        {
            if (!registered)
            {
                snprintf(response, sizeof(response),
                         "ERROR Please register first\n");
            }
            else
            {
                snprintf(response, sizeof(response),
                         "POS %d %d\n", x, y);
            }
        }
        else if (strncmp(buffer, "MOVE ", 5) == 0)
        {
            if (!registered)
            {
                snprintf(response, sizeof(response),
                         "ERROR Please register first\n");
            }
            else
            {
                int new_x = x;
                int new_y = y;
                bool valid = true;

                const char *direction = buffer + 5;

                if (strcmp(direction, "NORTH") == 0)
                    new_y--;
                else if (strcmp(direction, "SOUTH") == 0)
                    new_y++;
                else if (strcmp(direction, "WEST") == 0)
                    new_x--;
                else if (strcmp(direction, "EAST") == 0)
                    new_x++;
                else
                    valid = false;

                if (!valid)
                {
                    snprintf(response, sizeof(response),
                             "ERROR Invalid direction\n");
                }
                else if (new_x < 0 || new_x > 9 ||
                         new_y < 0 || new_y > 9)
                {
                    snprintf(response, sizeof(response),
                             "ERROR Out of bounds\n");
                }
                else
                {
                    x = new_x;
                    y = new_y;

                    snprintf(response, sizeof(response),
                             "POS %d %d\n", x, y);
                }
            }
        }
        else if (strncmp(buffer, "INFO ", 5) == 0)
        {
            if (!registered)
            {
                snprintf(response, sizeof(response), "ERROR Please register first\n");
            }
            else if (buffer[5] == '\0')
            {
                snprintf(response, sizeof(response), "Error Missing Animal name!\n");
            }
            else
            {
                pid_t pol;
                if ((pol = fork()) < 0)
                {
                    snprintf(response, sizeof(response), "Error : Nevim uz!\n");
                }
                else if (pol == 0)
                {
                    dup2(cl, STDOUT_FILENO);
                    close(cl);
                    execlp("./zoo_info", "zoo_info", buffer + 5, nullptr);

                    perror("execl");
                    _exit(127);
                }
                else
                {
                    int status;
                    waitpid(pol, &status, 0);
                    continue;
                }
            }
        }
        else
        {
            snprintf(response, sizeof(response),
                     "ERROR Unknown command\n");
        }

        if (write(cl, response, strlen(response)) < 0)
        {
            perror("write");
            break;
        }
        if (strcmp(buffer, "QUIT") == 0)
            break;
    }

    close(cl);
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Zadej port!\n");
        return -1;
    }

    int PORT = atoi(argv[1]);

    srand(time(nullptr));
    signal(SIGCHLD, SIG_IGN);
    int soc;
    if ((soc = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        exit(EXIT_FAILURE);
    }

    in_addr addr_any = {INADDR_ANY};
    sockaddr_in srv_addr;
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(PORT);
    srv_addr.sin_addr = addr_any;

    int opt = 1;

    if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        fprintf(stdout, "Nepodariolo se nastavit socopts!\n");
    }

    if (bind(soc, (const sockaddr *)(&srv_addr), sizeof(srv_addr)) < 0)
    {
        fprintf(stdout, "Nepodarilo se bindnout port a soc!\n");
        close(soc);
        exit(EXIT_FAILURE);
    }
    if (listen(soc, 1) < 0)
    {
        fprintf(stdout, "Nedase poslouchat! :( \n");
        close(soc);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int cl = -1;
        pollfd read_poll{};

        read_poll.fd = soc;
        read_poll.events = POLLIN;

        while (1)
        {
            int l_poll = poll(&read_poll, 1, -1);

            if (l_poll < 0)
            {
                if (errno == EINTR)
                    continue;

                perror("poll");
                break;
            }
            if (read_poll.revents & POLLIN)
            {
                cl = accept(soc, nullptr, nullptr);

                if (cl < 0)
                {
                    perror("accept");
                    continue;
                }
                printf("client pripojen!\n");
                pid_t p;

                if ((p = fork()) < 0)
                {
                    close(cl);
                    exit(EXIT_FAILURE);
                }
                if (p == 0)
                {
                    close(soc);

                    serve_player(cl);
                    _exit(EXIT_SUCCESS);
                }
                else
                {
                    close(cl);
                }
            }
        }
    }

    return 0;
}