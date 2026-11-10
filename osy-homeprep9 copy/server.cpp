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
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

constexpr int MAX = 4; // kapacita taca
constexpr int MAX_LEN = 128;
constexpr int OUT_LEN = MAX * (MAX_LEN + 8);

constexpr const char *SHM_NAME = "/zoo_shm";
constexpr const char *SEM_MUTEX = "/zoo_mutex";
constexpr const char *SEM_EMPTY = "/zoo_empty";
constexpr const char *SEM_FULL = "/zoo_full";

int soc = -1;

// tac sdileny mezi procesy
struct Tray
{
    char items[MAX][MAX_LEN];
    int state; // pocet polozek na taci
    int put_seq, get_seq;
};

Tray *tray = nullptr;
sem_t *mutex_sem = nullptr;
sem_t *empty_sem = nullptr;
sem_t *full_sem = nullptr;

int tl_id = 0;
const char *tl_role = "?";

bool debug = false;

void down(sem_t *s) { sem_wait(s); }
void up(sem_t *s) { sem_post(s); }

void uklid()
{
    close(soc);
    munmap(tray, sizeof(Tray));
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

// vlozi polozku na tac; na zacatek pripoji poradove cislo "N. "
void insert_item(const char *jidlo)
{
    down(empty_sem);
    down(mutex_sem);

    int st = tray->state;
    snprintf(tray->items[st], MAX_LEN, "%d. %s", st + 1, jidlo);
    tray->state = ++st;
    int seq = ++tray->put_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) vlozil jidlo c.%d: \"%s\" | tac: %d/%d\n", tl_id, tl_role, seq, tray->items[st - 1], st, MAX);
    }

    up(mutex_sem);
    if (st == MAX)
        up(full_sem); // tac je plny, muze k navstevnikovi
    else
        up(empty_sem); // je jeste misto
}

// vezme cely plny tac (odesle se navstevnikovi)
void remove_item(char *out)
{
    down(full_sem);
    down(mutex_sem);

    out[0] = '\0';
    for (int i = 0; i < tray->state; i++)
    {
        strcat(out, tray->items[i]);
        strcat(out, "\n");
    }
    tray->state = 0;
    int seq = ++tray->get_seq;
    if (debug)
    {
        printf("[DEBUG] klient %d (%s) odebral tac c.%d | tac: %d/%d\n", tl_id, tl_role, seq, tray->state, MAX);
    }

    up(mutex_sem);
    up(empty_sem);
}

bool recv_line(int fd, std::string &line)
{
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return false;
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        n--;
    buf[n] = '\0';
    line = buf;
    return true;
}

void client_proc(int fd, int id)
{
    tl_id = id;
    std::string line;

    // role urcuje server podle poradi pripojeni: lichy = kuchar, sudy = navstevnik
    bool kuchar = (id % 2 == 1);
    tl_role = kuchar ? "kuchar" : "navstevnik";
    const char *msg = kuchar ? "kuchar\n" : "navstevnik\n";

    if (write(fd, msg, strlen(msg)) <= 0)
    {
        printf("nepodarilo se zapsat\n");
        close(fd);
        return;
    }
    printf("[SERVER] klient %d pripojen jako %s!\n", id, tl_role);

    if (kuchar)
    {
        while (recv_line(fd, line))
        {
            if (line == "QUIT")
                break;

            insert_item(line.c_str());

            if (write(fd, "OK\n", 3) <= 0)
                break;
        }
    }
    else
    {
        for (;;)
        {
            char out[OUT_LEN];
            remove_item(out);
            if (write(fd, out, strlen(out)) <= 0)
                break;
            if (!recv_line(fd, line) || line != "OK")
                break;
        }
    }

    printf("[SERVER] klient %d (%s) odpojen!\n", tl_id, tl_role);
    close(fd);
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

    if (ftruncate(shm_fd, sizeof(Tray)) < 0)
    {
        perror("ftruncate");
        return 1;
    }
    tray = (Tray *)mmap(nullptr, sizeof(Tray), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (tray == MAP_FAILED)
    {
        perror("mmap fail");
        return 1;
    }
    close(shm_fd);
    memset(tray, 0, sizeof(Tray));

    // vsechny semafory jsou binarni
    mutex_sem = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0660, 1);
    empty_sem = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0660, 1);
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
            fprintf(stderr, "Nepodarilo se vytvorit proces!\n");
            close(fd);
            continue;
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
