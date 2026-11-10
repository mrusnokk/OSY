#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <csignal>
#include <semaphore.h>

constexpr int COOK_INTERVAL = 5; // kuchar generuje jedno jidlo za 5 vterin

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

// kuchar: kazdych 5 vterin posle jedno jidlo (pokud je na taci volno)
void *cook_thread(void *)
{
    for (;;)
    {
        char out[128];
        snprintf(out, sizeof(out), "%s\n", jidla[rand() % pocet_jidel]);
        if (!send_cmd(out))
            break;
        sem_wait(&sem_w); // server odpovi OK, az se jidlo vlozi na tac
        sleep(COOK_INTERVAL);
    }
    return nullptr;
}

// precte radek s roli (po jednom bajtu, aby se nezkonzumovalo nic navic)
static bool read_role(char *role, size_t len)
{
    size_t i = 0;
    while (i + 1 < len)
    {
        char c;
        if (read(soc, &c, 1) <= 0)
            return false;
        if (c == '\n')
            break;
        if (c != '\r')
            role[i++] = c;
    }
    role[i] = '\0';
    return true;
}

void *handle_server(void *)
{
    char role[32];
    if (!read_role(role, sizeof(role)))
        return nullptr;

    printf("Moje role: %s\n", role);
    fflush(stdout);

    bool kuchar = strcmp(role, "kuchar") == 0;
    if (kuchar)
    {
        pthread_t cook;
        if (pthread_create(&cook, nullptr, cook_thread, nullptr) != 0)
            return nullptr;
        pthread_detach(cook);
    }

    char buf[1024];
    while (1)
    {
        ssize_t n = read(soc, buf, sizeof(buf) - 1);
        if (n <= 0)
            break;

        buf[n] = '\0';
        if (kuchar)
        {
            if (strncmp(buf, "OK", 2) == 0)
                sem_post(&sem_w);
        }
        else
        {
            // navstevnik dostal tac
            printf("%s\n", buf);
            fflush(stdout);
            send_cmd("OK\n");
        }
    }
    return nullptr;
}

void *handle_console(void *)
{
    char line[128];
    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "QUIT") == 0)
        {
            send_cmd("QUIT\n");
            shutdown(soc, SHUT_RDWR);
            return nullptr;
        }
    }
    return nullptr;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "ERROR: run the program with args <ip> <port>\n");
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
