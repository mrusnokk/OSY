#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Animal
{
    const char *key;
    const char *name;
    const char *height;
    const char *weight;
    const char *food;
};

static const Animal animals[] = {
    {"mouse", "myš", "5 cm", "150 g", "všežravec"},
    {"elephant", "slon", "300 cm", "5000 kg", "býložravec"},
    {"lion", "lev", "120 cm", "190 kg", "masožravec"},
    {"giraffe", "žirafa", "550 cm", "1200 kg", "býložravec"},
    {"penguin", "tučňák", "100 cm", "30 kg", "masožravec"},
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("ERROR : zoo_info <animal>\n");
        return 1;
    }

    for (const Animal &a : animals)
    {
        if (strcmp(argv[1], a.key) == 0)
        {
            printf("Zvíře: %s\n", a.name);
            printf("Výška: %s\n", a.height);
            printf("Hmotnost: %s\n", a.weight);
            printf("Potrava: %s\n", a.food);
            return 0;
        }
    }

    printf("ERROR Unknown animal\n");
    return 1;
}