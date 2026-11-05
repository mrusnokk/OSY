#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

constexpr int N = 5;
constexpr int MAX_LEN = 128;

struct ClientInfo
{
    int fd;
    int id;
};

static char queue_buf[N][MAX_LEN];
static int in_index = 0, out_index = 0;
static int count = 0;
static int put_seq = 0, get_seq = 0;

sem_t mutex_sem;
sem_t empty_sem;
sem_t full_sem;

static bool debug = false;

static thread_local int tl_id = 0;
static thread_local const char *tl_role = "?";

void producer(char *jidlo)
{
    sem_wait(&empty_sem);
    sem_wait(&mutex_sem);

    strncpy(queue_buf[in_index], jidlo, MAX_LEN - 1);
    queue_buf[in_index][MAX_LEN - 1] = '\0';
    in_index = (in_index + 1) % N;
    count++;
    int seq = ++put_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) vlozil jidlo c.%d: \"%s\" | pult: %d/%d\n", tl_id, tl_role, seq, jidlo, count, N);
    }
    sem_post(&mutex_sem);
    sem_post(&full_sem);
}

void consumer(char *jidlo_out)
{
    sem_wait(&full_sem);
    sem_wait(&mutex_sem);

    strncpy(jidlo_out, queue_buf[out_index], MAX_LEN);
    out_index = (out_index + 1) % N;
    count--;
    int seq = ++get_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) spapal jidlo c.%d: \"%s\" | pult: %d/%d\n", tl_id, tl_role, seq, jidlo_out, count, N);
    }
    sem_post(&mutex_sem);
    sem_post(&empty_sem);
}

static bool recv_line(int fd, std::string &line)
{
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return false;
    buf[n - 1] = '\0';
    line = buf;
    return true;
}

static void *client_thread(void *arg)
{
    ClientInfo cl = *static_cast<ClientInfo *>(arg);
    delete static_cast<ClientInfo *>(arg);
    tl_id = cl.id;

    std::string line;
    if (write(cl.fd, "Role?\n", 6) <= 0 || !recv_line(cl.fd, line))
    {
        printf("nepodarilo se zapsat");
        close(cl.fd);
        return nullptr;
    }
    if (line == "kuchar")
    {
        tl_role = "kuchar";
        printf("[SERVER] klient %d pripojen jako kuchar!\n", cl.id);
        while (recv_line(cl.fd, line))
        {

            if (line == "QUIT")
                break;

            char jidlo[MAX_LEN];

            strncpy(jidlo, line.c_str(), MAX_LEN - 1);
            jidlo[MAX_LEN - 1] = '\0';

            producer(jidlo);

            if (write(cl.fd, "OK\n", 3) <= 0)
                break;
        }
    }
    else if (line == "navstevnik")
    {
        tl_role = "navstevnik";
        printf("[SERVER] klient %d pripojen jako navstevnik!\n", cl.id);

        for (;;)
        {
            char jidlo[MAX_LEN];
            consumer(jidlo);
            std::string out = std::string(jidlo) + "\n";
            if (write(cl.fd, out.c_str(), out.size()) <= 0)
                break;
            if (!recv_line(cl.fd, line) || line != "OK")
                break;
        }
    }
    else
    {
        if (write(cl.fd, "Neznama role\n", 13) <= 0)
            perror("write");
    }

    printf("[SERVER] klinet %d (%s) odpojen!\n", tl_id, tl_role);
    close(cl.fd);
    return nullptr;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "./server <port> [-d]\n");
        return -1;
    }

    int port = atoi(argv[1]);

    if (argc == 3 && strcmp(argv[2], "-d") == 0)
        debug = true;

    signal(SIGPIPE, SIG_IGN);

    int soc = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;

    setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(soc, (sockaddr *)&addr, sizeof(addr)) < 0 || listen(soc, 5) < 0)
    {
        perror("bind/listen ");
        return 1;
    }

    printf("[SERVER] posloucha port %d\n", port);

    int id = 1;

    sem_init(&full_sem, 0, 0);
    sem_init(&empty_sem, 0, N);
    sem_init(&mutex_sem, 0, 1);

    for (;;)
    {
        int fd = accept(soc, nullptr, nullptr);
        if (fd < 0)
        {
            continue;
        }
        auto *cl = new ClientInfo{fd, id++};
        pthread_t p;

        if (pthread_create(&p, nullptr, client_thread, cl) != 0)
        {
            close(fd);
            delete cl;
            continue;
        }
        pthread_detach(p);
    }
    return 0;
}