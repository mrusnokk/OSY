#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

constexpr int N = 5;
constexpr int MAX_LEN = 128;

constexpr const char *SHM_NAME = "/zoo_shm";
constexpr const char *SEM_MUTEX = "/zoo_mutex";
constexpr const char *SEM_EMPTY = "/zoo_empty";
constexpr const char *SEM_FULL = "/zoo_full";

int soc = -1;

struct Pult
{
    char queue_buf[N][MAX_LEN];
    int in_index, out_index;
    int count;
    int put_seq, get_seq;
};

Pult *pult = nullptr;
sem_t *mutex_sem = nullptr;
sem_t *empty_sem = nullptr;
sem_t *full_sem = nullptr;

int tl_id = 0;
const char *tl_role = "?";

bool debug = false;

void uklid()
{
    close(soc);
    munmap(pult, sizeof(Pult));
    sem_close(mutex_sem);
    sem_close(empty_sem);
    sem_close(full_sem);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    shm_unlink(SHM_NAME);
}

void on_sig(int)
{
    uklid();
    _exit(0);
}
void producer(char *jidlo)
{
    sem_wait(empty_sem);
    sem_wait(mutex_sem);

    strncpy(pult->queue_buf[pult->in_index], jidlo, MAX_LEN - 1);
    pult->queue_buf[pult->in_index][MAX_LEN - 1] = '\0';
    pult->in_index = (pult->in_index + 1) % N;
    pult->count++;
    int seq = ++pult->put_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) vlozil jidlo c.%d: \"%s\" | pult: %d/%d\n", tl_id, tl_role, seq, jidlo, pult->count, N);
    }
    sem_post(mutex_sem);
    sem_post(full_sem);
}

void consumer(char *jidlo_out)
{
    sem_wait(full_sem);
    sem_wait(mutex_sem);

    strncpy(jidlo_out, pult->queue_buf[pult->out_index], MAX_LEN);
    pult->out_index = (pult->out_index + 1) % N;
    pult->count--;
    int seq = ++pult->get_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) spapal jidlo c.%d: \"%s\" | pult: %d/%d\n", tl_id, tl_role, seq, jidlo_out, pult->count, N);
    }
    sem_post(mutex_sem);
    sem_post(empty_sem);
}

bool recv_line(int fd, std::string &line)
{
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return false;
    buf[n - 1] = '\0';
    line = buf;
    return true;
}

void client_proc(int fd, int id)
{
    tl_id = id;
    std::string line;
    if (write(fd, "Role?\n", 6) <= 0 || !recv_line(fd, line))
    {
        printf("nepodarilo se zapsat");
        close(fd);
        return;
    }
    if (line == "kuchar")
    {
        tl_role = "kuchar";
        printf("[SERVER] klient %d pripojen jako kuchar!\n", id);
        while (recv_line(fd, line))
        {

            if (line == "QUIT")
                break;

            char jidlo[MAX_LEN];

            strncpy(jidlo, line.c_str(), MAX_LEN - 1);
            jidlo[MAX_LEN - 1] = '\0';

            producer(jidlo);

            if (write(fd, "OK\n", 3) <= 0)
                break;
        }
    }
    else if (line == "navstevnik")
    {
        tl_role = "navstevnik";
        printf("[SERVER] klient %d pripojen jako navstevnik!\n", id);

        for (;;)
        {
            char jidlo[MAX_LEN];
            consumer(jidlo);
            std::string out = std::string(jidlo) + "\n";
            if (write(fd, out.c_str(), out.size()) <= 0)
                break;
            if (!recv_line(fd, line) || line != "OK")
                break;
        }
    }
    else
    {
        if (write(fd, "Neznama role\n", 13) <= 0)
            perror("write");
    }

    printf("[SERVER] klinet %d (%s) odpojen!\n", tl_id, tl_role);
    close(fd);
    return;
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
    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, on_sig);

    // smazani starych semaforu a pameti kdyby program byl ukoncen padem
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0660);
    if (shm_fd < 0)
    {
        perror("shm_open");
        return 1;
    }

    ftruncate(shm_fd, sizeof(Pult));
    pult = (Pult *)mmap(nullptr, sizeof(Pult), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (pult == MAP_FAILED)
    {
        perror("mmap fail");
        return 1;
    }
    close(shm_fd);
    memset(pult, 0, sizeof(Pult));

    mutex_sem = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0660, 1);
    empty_sem = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0660, N);
    full_sem = sem_open(SEM_FULL, O_CREAT | O_EXCL, 0660, 0);

    if (mutex_sem == SEM_FAILED || empty_sem == SEM_FAILED || full_sem == SEM_FAILED)
    {
        perror("sem_open fail");
        return 1;
    }

    soc = socket(AF_INET, SOCK_STREAM, 0);
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
    pid_t p;

    for (;;)
    {
        int fd = accept(soc, nullptr, nullptr);
        if (fd < 0)
        {
            continue;
        }
        if ((p = fork()) < 0)
        {
            fprintf(stderr, "Nepodarilo se vytvorit proces!");
        }
        if (p == 0)
        {
            close(soc);
            client_proc(fd, id);
            exit(0);
        }
        close(fd);
        id++;
    }
    return 0;
}