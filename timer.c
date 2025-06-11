#include <unistd.h>

int main(int argnum, char **args)
{
    char *binpath;
    if (argnum < 2) { write(2, "\033[31mneed time to sleep\n\033[0m", 28); return 0; }
    if (argnum == 3) { binpath = args[2]; }
    else { binpath = "./autoping"; }

    int time = 0;
    for (int i = 0; args[1][i] != '\0'; i++) { time = time * 10 + (args[1][i] - '0'); }

    while(1)
    {
        sleep(time);
        if(access(binpath, F_OK) == 0)
        {
            if(fork() == 0)
            {
                execv(binpath, (char *[3]){binpath, "ping", NULL});
                _exit(1);
            }
        }
        else { write(2, "\033[31merror, ping binary missing\n\033[0m", 28); }
    }
    return 0;
}
