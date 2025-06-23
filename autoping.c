/*
  Author: ulith
  Attribution appreciated in derivative works
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

char sleeptime[6] = {0};
char *iplist[64] = {0};
char logpath[4096] = {0};
char confpath[4096] = {0};

int kill(pid_t pid, int sig);
struct passwd { char *pw_name; char *pw_passwd; uid_t pw_uid; gid_t pw_gid; char *pw_gecos; char *pw_dir; char *pw_shell; };
struct passwd *getpwuid(uid_t uid);

int configexist() {
    if (confpath[0] == '\0') {
        if (getenv("XDG_CONFIG_HOME") != NULL)
        {
            char confighome[4096];
            strcpy(confighome, getenv("XDG_CONFIG_HOME"));
            strcpy(confpath, strcat(confighome, "/autoping"));
        }
        else if (getenv("HOME") != NULL)
        {
            char home[4096];
            strcpy(home, getenv("HOME"));
            strcpy(confpath, strcat(home, "/.config/autoping"));
        }
        else
        {
            struct passwd *pw = getpwuid(getuid());
            if (pw)
            {
                char pwd[4096];
                strcpy(pwd, pw->pw_dir);
                strcpy(confpath, strcat(pwd, "/.config/autoping"));
            }
            else { write(2, "\033[31mcould not find home\033[0m\n", 19); return 0; }
        }
    }

    struct stat s;
    if (stat(confpath, &s) == -1) { return 0; }

    char path[4096];
    strcpy(path, confpath);
    strcat(path, "/config");

    FILE *config = fopen(path, "r");
    if (!config) { return 0; }
    fclose(config);
    return 1;
}

int mkconfig()
{
    struct stat s;
    if (stat(confpath, &s) == -1) { if (mkdir(confpath, 0755) == -1) { write(2, "\033[31mcould not make config directory\033[0m\n", 42); return 0; } }
    char path[4096];
    strcpy(path, confpath);
    strcat(path, "/config");

    FILE *config = fopen(path, "w");
    if (!config) { write(2, "\033[31mcould not access config file\033[0m\n", 38); return 0; }
    if (fwrite("# autoping configuration file\n"
        "# time is in seconds\n"
        "# ping addresses should be put under [ips] and seperated with newlines\n"
        "[time]\n"
        "600\n\n"
        "[logfile]\n"
        "\"~/.pinglog\"\n\n"
        "[ips]\n\n", 1, 165, config) < 0) { write(2, "\033[31mcould not write to config file\033[0m\n", 40); fclose(config); return 0; }
    else { write(1, "\033[32msuccess\033[0m\n\n", 18); }
    fclose(config);
    return 1;
}

void readconfig()
{
    if (!configexist()) { return; }
    char path[4096];
    strcpy(path, confpath);
    strcat(path, "/config");
    FILE *config = fopen(path, "r");
    
    char line[256];
    int getips = 0; int gettime = 0; int getlog = 0;
    int ipc = 0;
    for (int i = 0; i < 64; ++i) { free(iplist[i]); iplist[i] = NULL; }

    while (fgets(line, 256, config))
    {
        char *hash = strchr(line, '#');
        if (hash) { *hash = '\0'; }
        if (!strcmp(line, "\n")) { continue; }
        for (int len = strlen(line); len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\n'); len--) { line[len - 1] = '\0'; }

        if (getips && !strncmp(line, "[", 1)) { getips = 0; }
        if (!strncmp(line, "[time]", 6)) { gettime = 1; continue; }
        if (gettime) { strcpy(sleeptime, line); gettime = 0; continue; }
        if (!strncmp(line, "[logfile]", 9)) { getlog = 1; continue; }
        if (getlog)
        {
            if (line[0] == '\"')
            {
                int len = 0;
                while (line[len] != '\0') { line[len] = line[len + 1]; len++; }
                line[len - 2] = '\0';
            }
            strcpy(logpath, line);
            getlog = 0;
            continue;
        }
        if (!strncmp(line, "[ips]", 5)) { getips = 1; continue; }
        if (getips) { iplist[ipc++] = strdup(line); }
    }
    fclose(config);
}

int ping(char *ip) // planning to replace with something more scalable
{
    char cmd[128] = "ping -c2 -s1 -i 0.2 ";
    strcat(cmd, ip);
    strcat(cmd, " > /dev/null 2>&1");
    return system(cmd);
}

void pingall()
{
    if (!configexist()) { write(2, "\033[31mconfig not found\033[0m\n", 26); return; }
    else { readconfig(); }
    if (iplist[0] == NULL)
    {
        write(2, "\033[31mno ips found\033[0m\n", 22);
        system("notify-send 'Autoping has nothing to ping'");
        return;
    }

    if (ping("8.8.8.8")) { system("notify-send 'Can't connect to internet'"); return; }
    else if (ping("google.com")) { system("notify-send 'Can't connect to nameserver'"); return; }
    
    char *failedpings[64] = {0};
    int failnum = 0;
    for(int i = 0; iplist[i]; i++) { if (ping(iplist[i])) { failedpings[failnum] = iplist[i]; failnum++; } }

    if (logpath[0] != '\0')
    {
        if(logpath[0] == '~')
        {
            char path[4096] = {0};
            if (getenv("XDG_CONFIG_HOME") != NULL) { strcpy(path, getenv("XDG_CONFIG_HOME")); }
            else if (getenv("HOME") != NULL) { strcpy(path, getenv("HOME")); }
            else
            {
                struct passwd *pw = getpwuid(getuid());
                if (pw) { strcpy(path, pw->pw_dir); }
                else { write(2, "\033[31mcould not find home\033[0m\n", 19); return; }
            }

            for (int i = 0; logpath[i] != '\0'; i++) { logpath[i] = logpath[i + 1]; }
            strcat(path, logpath);
            strcpy(logpath, path);
        }

        char par[4096] = {0};
        strcpy(par, logpath);
        char *pathend = strrchr(par, '/');
        if (pathend != NULL) { *pathend = '\0'; }

        struct stat s;
        if (stat(par, &s) == -1) { write(2, "\033[31mlogfile parent directory does not exist\033[0m\n", 49); }
        else
        {
            FILE *logfile = fopen(logpath, "a");
            if (!logfile) { write(2, "\033[31mlogfile does not exist\033[0m\n", 32); }
            else
            {
                write(1, "\033[35mlogging to ", 16);
                write(1, logpath, strlen(logpath));
                write(1, "\033[0m\n", 5);

                char log[256] = {0};
                for(int i = 0; i < failnum; i++)
                {
                    struct tm *tm = localtime(&(time_t){time(NULL)});
                    strftime(log, 256, "%d/%m/%Y %H:%M:%S", tm); // these 2 lines represent >20kb of libraries
                    strcat(log, " | > Ping failed for ");
                    strcat(log, failedpings[i]);
                    strcat(log, "\n");

                    fwrite(log, 1, strlen(log), logfile);
                    write(1, log, strlen(log));
                }
                fclose(logfile);
            }
        }
    }
    else { write(1, "no logfile set\n", 15); }
    
    for(int n = 0; n < failnum; n++)
    {
        char cmd[128] = "notify-send 'Ping Failed' 'For ";
        strcat(cmd, failedpings[n]);
        strcat(cmd, "'");
        system(cmd);
    }
}

int startTimer()
{
    if (!configexist()) { write(2, "\033[35merror: config not found\nno addresses to ping!\033[0m\n", 55); return 0; }
    else { readconfig(); }

    char binpath[4096] = {0};
    readlink("/proc/self/exe", binpath, 4096);
    char *dir = strrchr(binpath, '/');
    *(dir + 1) = '\0';

    char timerpath[4096];
    strcpy(timerpath, binpath);
    strcat(timerpath, "timer");
    FILE *timer = fopen(timerpath, "r");
    if (!timer) { return 0; }
    fclose(timer);

    char pidpath[4096];
    strcpy(pidpath, binpath);
    strcat(pidpath, "pid");
    FILE *pidfile = fopen(pidpath, "r");
    if (pidfile) { write(2, "\033[35mdaemon already running\033[0m\n", 32); fclose(pidfile); return 0; }

    pid_t pid = fork();
    if (pid < 0) { return 0; }
    if (pid > 0) { return 1; }
    if (setsid() < 0) { exit(1); }
    pid = fork();
    if (pid < 0) { exit(1); }
    if (pid > 0) { exit(0); }

    pidfile = fopen(pidpath, "w");
    int pidnum = getpid();
    char pidstr[16];
    int n = 1; int i = 0;
    while (pidnum / n >= 10) { n *= 10; }
    while (n > 0) { pidstr[i++] = '0' + (pidnum / n) % 10; n /= 10; }
    fwrite(pidstr, 1, i, pidfile);
    fclose(pidfile);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (sleeptime[0] == '\0') { strcpy(sleeptime, "600"); }
    char selfpath[4096];
    strcpy(selfpath, binpath);
    strcat(selfpath, "autoping");
    execv(timerpath, (char *[]){timerpath, sleeptime, selfpath, NULL});
    exit(1); return 1;
}
int stopTimer()
{
    char binpath[4096] = {0};
    readlink("/proc/self/exe", binpath, 4096);
    char *dir = strrchr(binpath, '/');
    *(dir + 1) = '\0';

    char pidpath[4096];
    strcpy(pidpath, binpath);
    strcat(pidpath, "pid");
    FILE *pidfile = fopen(pidpath, "r");
    if (pidfile)
    {
        pid_t pid;
        if (fscanf(pidfile, "%d", &pid) != 1) { write(2, "\033[31mpid file cannot be read\n\033[0m", 33); return 0; }
        kill(pid, 15);
        remove(pidpath);
        fclose(pidfile);
        return 1;
    }
    else { write(2, "\033[35mdaemon has not been started\033[0m\n", 37); return 0; }
}

int parseArgs(char *cmd, char *args[])
{
    int len = 0;
    cmd = strtok(NULL, " ");

    while(cmd != NULL)
    {
        args[len++] = cmd;
        cmd = strtok(NULL, " ");
        if(len >= 64) { write(2, "\033[31mmaximum of 64 reached\033[0m\n", 22); break; }
    }
    return len;
}

void cli()
{
    if (!configexist()) { write(1, "\033[35musing defaults..\033[0m\n", 26); }
    else { write(1, "\033[35mconfig file found\033[0m\n", 27); }

    char line[256];
    while(1)
    {
        write(1, "\033[36m~> \033[0m", 12);
        if (NULL == fgets(line, sizeof(line), stdin)) { break; }

        for (int len = strlen(line); len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\n'); len--) { line[len - 1] = '\0'; }
        char *cmd = strtok(line, " ");

        if (cmd != NULL)
        {
            if (!strcmp(cmd, "h") || !strcmp(cmd, "help"))
            {
                write(1, "usage:"
                    "\n- q / quit / exit       :  quit"
                    "\n- e / enable            :  start daemon"
                    "\n- d / disable           :  stop daemon"
                    "\n- l / list              :  list addresses"
                    "\n- a / add <address>     :  add space seperated list of addresses"
                    "\n- r / remove <address>  :  remove space seperated list of addresses"
                    "\n- t / time              :  set daemon ping frequency"
                    "\n- p / ping              :  ping all addresses\n\n", 393);
            }
            else if (!strcmp(cmd, "q") || !strcmp(cmd, "quit") || !strcmp(cmd, "exit")) { write(1, "exiting..\n", 10); break; }
            else if (!strcmp(cmd, "e") || !strcmp(cmd, "enable"))
            {
                if (startTimer()) { write(1, "\033[32msuccess\n\n\033[0m", 18); }
                else { write(2, "\033[31mfailed\n\n\033[0m", 17); }
            }
            else if (!strcmp(cmd, "d") || !strcmp(cmd, "disable"))
            {
                if (stopTimer()) { write(1, "\033[32msuccess\n\n\033[0m", 18); }
                else { write(2, "\033[31mfailed\n\n\033[0m", 17); }
            }
            else if (!strcmp(cmd, "l") || !strcmp(cmd, "list"))
            {
                if (!configexist()) { write(2, "\033[31mconfig not found\033[0m\n", 26); continue; }
                else {
                    readconfig();
                    int len = 0;
                    for (int i = 0; iplist[i]; i++) { len++; }

                    if (len < 1) { write(1, "\033[36mempty\033[0m\n\n", 16); continue; }
                    else
                    {
                        write(1, "\033[36m", 5);
                        for (int i = 0; iplist[i]; i++) { write(1, "\n", 1); write(1, iplist[i], strlen(iplist[i])); }
                        write(1, "\033[0m\n\n", 6);
                    }
                }
            }
            else if (!strcmp(cmd, "a") || !strcmp(cmd, "add"))
            {
                char *args[64];
                int arglen = parseArgs(cmd, args);
                if (!configexist())
                {
                    write(1, "\033[35mcreating config..\033[0m\n", 27);
                    if (!mkconfig()) { continue; }
                }

                char path[4096];
                strcpy(path, confpath);
                strcat(path, "/config");
                FILE *config = fopen(path, "r");
                char tmppath[4096];
                strcpy(tmppath, confpath);
                strcat(tmppath, "/temp");
                FILE *tmpconf = fopen(tmppath, "w");

                char line[128];
                while (fgets(line, sizeof(line), config))
                {
                    if (!strncmp(line, "[ips]", 5))
                    {
                        fputs(line, tmpconf);
                        for(int i = 0; i < arglen; i++) { fputs(args[i], tmpconf); fputs("\n", tmpconf); }
                    }
                    else { fputs(line, tmpconf); }
                }

                int fail = 0;
                if (fclose(config) || fclose(tmpconf) || remove(path) || rename(tmppath, path)) { fail = 1; }
                if (fail) { write(2, "\033[31mfailed\033[0m\n\n", 17); }
                else
                {
                    write(1, "\033[32msuccess\033[0m\n\n", 18);
                    for(int i = 0; i < arglen; i++)
                    {
                        write(1, "added ", 6);
                        write(1, args[i], strlen(args[i]));
                        write(1, "\n", 1);
                    }
                    write(1, "\n", 1);
                }
            }
            else if (!strcmp(cmd, "r") || !strcmp(cmd, "remove"))
            {
                char *args[64];
                int arglen = parseArgs(cmd, args);
                if (!configexist()) { write(2, "\033[31mconfig not found\033[0m\n", 26); continue; }

                char path[4096];
                strcpy(path, confpath);
                strcat(path, "/config");
                FILE *config = fopen(path, "r");
                char tmppath[4096];
                strcpy(tmppath, confpath);
                strcat(tmppath, "/temp");
                FILE *tmpconf = fopen(tmppath, "w");

                char line[128];
                int n;
                while (fgets(line, sizeof(line), config))
                {
                    n = 1;
                    for(int i = 0; i < arglen; i++)
                    {
                        if (!strncmp(line, args[i], strlen(args[i]))) { n = 0; break; }
                    }
                    if (n) { fputs(line, tmpconf); }
                }

                int fail = 0;
                if (fclose(config) || fclose(tmpconf) || remove(path) || rename(tmppath, path)) { fail = 1; }
                if (fail) { write(2, "\033[31mfailed\033[0m\n\n", 17); }
                else
                {
                    write(1, "\033[32msuccess\033[0m\n\n", 18);
                    for(int i = 0; i < arglen; i++)
                    {
                        write(1, "removed ", 8);
                        write(1, args[i], strlen(args[i]));
                        write(1, "\n", 1);
                    }
                    write(1, "\n", 1);
                }
            }
            else if (!strcmp(cmd, "t") || !strcmp(cmd, "time"))
            {
                char *args[64];
                if (parseArgs(cmd, args) != 1) { write(2, "\033[31minclude length in seconds\033[0m\n", 35); continue; }
                if (!configexist())
                {
                    write(1, "\033[35mcreating config..\033[0m\n", 27);
                    if (!mkconfig()) { continue; }
                }
                write(1, "\033[35msetting time to ", 21);
                write(1, args[0], strlen(args[0]));
                write(1, "\033[0m\n", 5);

                char path[4096];
                strcpy(path, confpath);
                strcat(path, "/config");
                FILE *config = fopen(path, "r");
                char tmppath[4096];
                strcpy(tmppath, confpath);
                strcat(tmppath, "/temp");
                FILE *tmpconf = fopen(tmppath, "w");
                
                char line[128];
                int gettime = 0;

                while (fgets(line, sizeof(line), config))
                {
                    if (!strcmp(line, "\n")) { fputs(line, tmpconf); continue; }
                    if (gettime) { fputs(args[0], tmpconf); fputs("\n", tmpconf); gettime = 0; continue; }
                    if (!strncmp(line, "[time]", 6)) { fputs(line, tmpconf); gettime = 1; continue; }
                    fputs(line, tmpconf);
                }

                int fail = 0;
                if (fclose(config) || fclose(tmpconf) || remove(path) || rename(tmppath, path)) { fail = 1; }
                if (fail) { write(2, "\033[31mfailed\033[0m\n\n", 17); }
                else { write(1, "\033[32msuccess\033[0m\n\n", 18); }
            }
            else if (!strcmp(cmd, "p") || !strcmp(cmd, "ping")) { pingall(); write(1, "\033[32mdone\033[0m\n\n", 15); }
            else if (!strcmp(cmd, "owo")) { write(1, "\033[35muwu\033[0m\n", 13); }
            else if (!strcmp(cmd, "uwu")) { write(1, "\033[35mowo\033[0m\n", 13); }
            else if (!strcmp(cmd, "awa")) { write(1, "\033[35mawawa\033[0m\n", 15); }
            else if (!strcmp(cmd, "nya")) { write(1, "\033[35m:3\033[0m\n", 12); }
            else { write(2, "\033[31merror: invalid\n\033[34m- try help or h\033[0m\n", 45); }
        }
        else { write(1, "\033[F\033[2K\033[E", 10); }
    }
}

int main(int argnum, char **args)
{
    if (argnum == 1) { write(1, "\033[35mopening cli..\033[0m\n", 23); cli(); }
    else if(!strcmp(args[1], "ping")) { pingall(); }
    else if (!strcmp(args[1], "help") || !strcmp(args[1], "-h"))
    {
        write(1,
            "\033[35m~ a slightly fancy auto pinging tool ~\033[0m\n"
            "\nusage:"
            "\n- \033[36mrun without args to open prompt\033[0m"
            "\n- \033[34mping\033[0m: ping your set ips"
            "\n- \033[34mstart\033[0m: start service timer"
            "\n- \033[34mstop\033[0m: stop service timer\n", 209);
    }
    else if(!strcmp(args[1], "start")) { startTimer(); }
    else if(!strcmp(args[1], "stop")) { stopTimer(); }
    else { write(2, "\033[31minvalid command\033[0m - \033[34mtry help\033[0m\n", 45); }
}
