#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <semaphore.h>

int soc = -1;
static const char *jidla[] = {
    "Smazeny syr",
    "Kureci rizek s hranolky",
    "Spagety Bolognese",
    "Zeleninovy salat",
    "Hamburger",
    "Hot dog",
    "Palacinky",
    "Ovocny pohar"

};
sem_t sem_w;
constexpr int pocet_jidel = sizeof(jidla) / sizeof(jidla[0]);

bool send_cmd(const char *cmd)
{
    return write(soc, cmd, strlen(cmd)) >= 0;
}

void *handle_server(void *arg)
{
    char buf[128];

    while (1)
    {
        ssize_t n = read(soc, buf, sizeof(buf) - 1);
        if (n <= 0)
            break;

        buf[n] = '\0';
        if (strncmp(buf, "OK", 2) == 0)
        {
            sem_post(&sem_w);
        }
        else if (strncmp("Role?", buf, 5) != 0)
        {
            send_cmd("OK\n");
        }
        printf("%s\n", buf);
    }
    return nullptr;
}

void *handle_console(void *arg)
{

    char line[128];
    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0')
            continue;

        const char *cmd = nullptr;

        int pocet = atoi(line);

        for (int i = 0; i < pocet; i++)
        {
            char out[128];
            snprintf(out, sizeof(out), "%s\n", jidla[rand() % pocet_jidel]);
            send_cmd(out);
            sem_wait(&sem_w);
        }
        if (pocet == 0)
        {
            char out[128];
            snprintf(out, sizeof(out), "%s\n", line);
            send_cmd(out);
        }

        if (strcmp(line, "QUIT") == 0)
        {
            return nullptr;
        }
    }
    send_cmd("QUIT\n");
    return nullptr;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        perror("ERROR: run the program with agrs <ip> <port>");
        return -1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    srand(time(nullptr));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        return EXIT_FAILURE;
    }

    soc = socket(AF_INET, SOCK_STREAM, 0);

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
    signal(SIGPIPE, SIG_IGN);

    pthread_t server, console;
    sem_init(&sem_w, 0, 0);
    if (pthread_create(&server, NULL, handle_server, nullptr) != 0 ||
        pthread_create(&console, NULL, handle_console, nullptr) != 0)
    {
        perror("THREAD CREATE ERROR");
        close(soc);
        return EXIT_FAILURE;
    }
    pthread_detach(console);
    pthread_join(server, nullptr);

    close(soc);
    return 0;
}
