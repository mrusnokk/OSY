#include "log.h"
#include <fcntl.h>

bool log_acess(std::string path)
{
    if (access(path.c_str(), R_OK) < 0)
    {
        // fprintf(stderr, "access error for %s\n", path.c_str());
        return false;
    }
    else
    {
        // printf("read access OK\n");
        return true;
    }
}
void log_stat(std::string path)
{
    struct stat buf;

    if (stat(path.c_str(), &buf) < 0)
    {
        fprintf(stderr, "Chyba stat(%s): %s\n", path.c_str(), strerror(errno));
        return;
    }

    fprintf(stdout, "Soubor: %s\nvelikost souboru: %lu\nUID: %d\nGID: %d\nHardlinky: %lu\n", path.c_str(), buf.st_size, buf.st_uid, buf.st_gid, buf.st_nlink);
}

void log_lstat(std::string path)
{
    struct stat buf;
    std::string out;

    if (lstat(path.c_str(), &buf) < 0)
    {
        fprintf(stderr, "Chyba lstat(%s): %s\n", path.c_str(), strerror(errno));
        return;
    }

    if (S_ISREG(buf.st_mode))
        out = "regular";
    else if (S_ISDIR(buf.st_mode))
        out = "directory";
    else if (S_ISCHR(buf.st_mode))
        out = "character special";
    else if (S_ISBLK(buf.st_mode))
        out = "block special";
    else if (S_ISFIFO(buf.st_mode))
        out = "fifo";
    else if (S_ISLNK(buf.st_mode))
        out = "symbolic link";
    else if (S_ISSOCK(buf.st_mode))
        out = "socket";
    else
        out = "** unknown mode **";
    printf("soubor: %s\ntyp souboru: %s\nvelikost: %lu\ncislo inodu: %lu\n", path.c_str(), out.c_str(), buf.st_size, buf.st_ino);
}

void log_fstat(std::string path)
{
    struct stat buf;
    int file = open(path.c_str(), O_RDWR);

    if (file < 0)
    {
        fprintf(stderr, "Chyba open(%s): %s\n", path.c_str(), strerror(errno));
        return;
    }

    fstat(file, &buf);
    fprintf(stdout, "soubor: %s\nvelikost: %ld\n cislo inodu: %lu\n", path.c_str(), buf.st_size, buf.st_ino);
    close(file);
}