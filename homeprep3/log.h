#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

bool log_acess(std::string path);
void log_stat(std::string path);
void log_lstat(std::string path);
void log_fstat(std::string path);