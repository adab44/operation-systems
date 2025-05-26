#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

void write_log(const char* jobname, const char* status, int second)
 {
    FILE* fp = fopen("scheduler.log", "a");
    if(!fp) return;
    time_t raw = time(NULL);
    struct tm* t = localtime(&raw);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    if(second> 0)
        fprintf(fp, "[%s] [INFO] %s %s... second %d\n", ts, jobname, status, second);
    else
        fprintf(fp, "[%s] [INFO] %s %s\n", ts, jobname, status);
    fclose(fp);
}

int main(int argc, char *argv[])
{
    if(strstr(argv[0], "jobA")) 
    {
        int i = 1;
        while(i <= 6) 
        {
            sleep(1);
            i++;
        }
    }
    else if(strstr(argv[0], "jobB")) 
    {
        int i = 1;
        while (i <= 9) 
        {
            sleep(1);
            i++;
        }
    }
    else if(strstr(argv[0], "jobC")) 
    {
        int i = 1;
        while(i <= 4) 
        {
            sleep(1);
            i++;
        }
    }
    else 
    {
        write_log(argv[0], "unknown", 0);
    }

    return 0;
}
