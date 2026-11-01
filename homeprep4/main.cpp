#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>

struct ChatMsg
{
    int orig_line;
    int seq_num;
    char text[1024];
};

struct FinalMsg
{
    char text[2048];
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Zadali jste nizky pocet argumentu!\n");
        return -1;
    }

    pid_t p1, p2, p3;

    int pipe_p1p2[2];
    int pipe_p2p3[2];
    int pipe_p3parent[2];

    if (pipe(pipe_p1p2) == -1 || pipe(pipe_p2p3) == -1 || pipe(pipe_p3parent) == -1)
    {
        fprintf(stderr, "Chyba pri vytvareni pipe!\n");
        return -1;
    }

    std::srand(std::time(0));

    if ((p1 = fork()) < 0)
        return -1;
    if (p1 == 0)
    {
        close(pipe_p1p2[0]);
        close(pipe_p2p3[0]);
        close(pipe_p2p3[1]);
        close(pipe_p3parent[0]);
        close(pipe_p3parent[1]);

        FILE *f = fopen(argv[1], "r");
        if (!f)
        {
            fprintf(stderr, "Proces 1: Nepodarilo se nacist soubor!\n");
            return -1;
        }

        std::vector<std::string> messages;
        char line[1024];

        while (fgets(line, sizeof(line), f) != NULL)
        {
            line[strcspn(line, "\n")] = 0;
            messages.push_back(line);
        }
        fclose(f);

        int pocet_zprav = atoi(argv[2]);
        if (!messages.empty())
        {
            for (int i = 0; i < pocet_zprav; i++)
            {
                int r_num = std::rand() % messages.size();

                ChatMsg msg;
                msg.orig_line = r_num + 1;
                msg.seq_num = 0;
                strncpy(msg.text, messages[r_num].c_str(), sizeof(msg.text) - 1);
                msg.text[sizeof(msg.text) - 1] = '\0';

                printf("[PID %d] Přijímám zprávu: \"%s\" (řádek %d)\n", getpid(), msg.text, msg.orig_line);

                write(pipe_p1p2[1], &msg, sizeof(msg));
            }
        }

        close(pipe_p1p2[1]);
        return 0;
    }

    if ((p2 = fork()) < 0)
        return -1;
    if (p2 == 0)
    {
        close(pipe_p1p2[1]);
        close(pipe_p2p3[0]);
        close(pipe_p3parent[0]);
        close(pipe_p3parent[1]);

        ChatMsg msg;
        int seq = 1;

        while (read(pipe_p1p2[0], &msg, sizeof(msg)) > 0)
        {
            msg.seq_num = seq++;
            printf("[PID %d] Čísluji zprávu: %d. %s (řádek %d)\n", getpid(), msg.seq_num, msg.text, msg.orig_line);

            write(pipe_p2p3[1], &msg, sizeof(msg));
        }

        close(pipe_p1p2[0]);
        close(pipe_p2p3[1]);
        return 0;
    }

    if ((p3 = fork()) < 0)
        return -1;
    if (p3 == 0)
    {
        close(pipe_p1p2[0]);
        close(pipe_p1p2[1]);
        close(pipe_p2p3[1]);
        close(pipe_p3parent[0]);

        ChatMsg msg;
        char result[2048];

        while (read(pipe_p2p3[0], &msg, sizeof(msg)) > 0)
        {
            size_t length = strlen(msg.text);
            printf("[PID %d] Analyzuji zprávu: %d. %s (%zu) – řádek %d\n", getpid(), msg.seq_num, msg.text, length, msg.orig_line);

            snprintf(result, sizeof(result), "%d. %s (%zu) – řádek %d", msg.seq_num, msg.text, length, msg.orig_line);

            write(pipe_p3parent[1], result, sizeof(result));
        }

        close(pipe_p2p3[0]);
        close(pipe_p3parent[1]);
        return 0;
    }

    close(pipe_p1p2[0]);
    close(pipe_p1p2[1]);
    close(pipe_p2p3[0]);
    close(pipe_p2p3[1]);
    close(pipe_p3parent[1]);

    char result[2048];

    while (read(pipe_p3parent[0], result, sizeof(result)) > 0)
    {
        printf("[PID %d] Přijata zpráva: %s\n", getpid(), result);
    }

    close(pipe_p3parent[0]);

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    waitpid(p3, NULL, 0);

    return 0;
}