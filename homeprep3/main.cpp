#include "log.h"
#include <filesystem>
namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Nebyl zadan vstupni soubor nebo flag (-a, -s, -l, -f)! ");
        return -1;
    }
    std::string mode;
    mode = fs::path(argv[1]).filename().string();

    if (mode == "-a")
    {
        if (log_acess(argv[2]))
            printf("soubor %s lze cist: ANO\n", argv[2]);
        else
            printf("soubor %s lze cist: NE\n", argv[2]);
    }
    if (mode == "-s")
    {
        log_stat(argv[2]);
    }
    if (mode == "-l")
    {
        log_lstat(argv[2]);
    }
    if (mode == "-f")
    {
        log_fstat(argv[2]);
    }
    return 0;
}