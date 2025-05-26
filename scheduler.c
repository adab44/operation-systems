#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>


#define LOG_FILE "scheduler.log"
#define MAX_JOBS 10
#define MAX_NAME_LEN 32


int job_queue[MAX_JOBS];
int f = 0, r = 0, cout = 0;

void enqueue(int job_index) 
{
    job_queue[r] = job_index;
    r = (r + 1) % MAX_JOBS;
    cout++;
}

int is_queue_empty() 
{
    return cout == 0;
}

struct Job 
{
    pid_t process_id;
    int has_started;
    int is_completed;
    int priority_level;
    char job_name[MAX_NAME_LEN];
    int arrival_time;
    
    int total_exec_time;
    int time_left;
    

};

struct Job jobs[MAX_JOBS];
int job_count = 0;
int time_slice = 1;

void log_event(const char* message) 
{
    FILE* log = fopen(LOG_FILE, "a");
    if(!log) return;
    char time_str[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    fprintf(log, "[%s] [INFO] %s\n", time_str, message);
    fclose(log);
}

void read_jobs(const char* filename) 
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("jobs.txt");
        exit(1);
    }
    fscanf(file, "TimeSlice %d", &time_slice);
    while (fscanf(file, "%s %d %d %d",jobs[job_count].job_name,&jobs[job_count].arrival_time,&jobs[job_count].priority_level,&jobs[job_count].total_exec_time) == 4) 
    {
        jobs[job_count].time_left = jobs[job_count].total_exec_time;
        jobs[job_count].has_started = 0;
        jobs[job_count].is_completed = 0;
        job_count++;
    }
    fclose(file);
}

void start_job(int index) 
{
    pid_t pid = fork();
    if(pid == 0) 
    {
        char path[64];
        sprintf(path, "./%s", jobs[index].job_name);
        execl(path, jobs[index].job_name, (char*)NULL);
        perror("exec failed");
        exit(1);
    }else 
    {
        char msg[256];
        sprintf(msg,"Forking new process for %s", jobs[index].job_name);
        log_event(msg);
        jobs[index].process_id = pid;
        sprintf(msg,"Executing %s (PID: %d) using exec", jobs[index].job_name, pid);
        log_event(msg);
        kill(pid, SIGSTOP);
    }
}

int all_jobs_done() 
{
    for (int i = 0; i < job_count; i++) 
    {
        if (!jobs[i].is_completed) return 0;
    }
    return 1;
}

int select_next_job(int last_job_index) 
{
    int selected = -1;
    for(int i = 0; i < cout; i++) 
    {
        int index = job_queue[(f + i) % MAX_JOBS];
        if(jobs[index].is_completed) continue;
        if(index == last_job_index && cout > 1) continue;

        if(selected == -1) 
        {
            selected = index;
            continue;
        }

        struct Job *a = &jobs[index];
        struct Job *b = &jobs[selected];

        if(a->priority_level < b->priority_level) selected = index;
        else if(a->priority_level == b->priority_level) 
        {
            if(a->arrival_time < b->arrival_time) selected = index;
            else if(a->arrival_time == b->arrival_time) 
            {
                if(a->time_left < b->time_left) selected = index;
                else if(a->time_left == b->time_left)
                {
                    if(index < selected) selected = index;
                }
            }
        }
    }

    if(selected == -1) selected = last_job_index;

    for(int i = 0; i < cout; i++){
        int idx = (f + i) % MAX_JOBS;
        if (job_queue[idx] == selected) 
        {

            for (int j = i; j < cout - 1; j++){
                job_queue[(f + j) % MAX_JOBS] = job_queue[(f + j + 1) % MAX_JOBS];
            }
            r = (r - 1 + MAX_JOBS) % MAX_JOBS;
            cout--;
            break;
        }
    }

    return selected;
}

int main() {
    //setenv("TZ", "Europe/Istanbul", 1); calısmadı
    //tzset();

    read_jobs("jobs.txt");

    FILE* log = fopen(LOG_FILE, "w");
    if (log) fclose(log);

    int current_time = 0;
    int prev_job_index = -1;

    for(int i = 0; i < job_count; i++) 
    {
        if (jobs[i].arrival_time <= current_time && !jobs[i].has_started) 
        {
            enqueue(i);
        }
    }

    while(!all_jobs_done() || !is_queue_empty()) 
    {
        if (is_queue_empty()) {
            sleep(1);
            current_time++;
            for(int i = 0; i < job_count; i++) 
            {
                if(!jobs[i].has_started && jobs[i].arrival_time <= current_time) 
                {
                    enqueue(i);
                }
            }
            continue;
        }

        int cur_job = select_next_job(prev_job_index);
        prev_job_index = cur_job;
        char msg[256];

        if(!jobs[cur_job].has_started) 
        {
            start_job(cur_job);
            jobs[cur_job].has_started = 1;
        } else {
            sprintf(msg, "Resuming %s (PID: %d) – SIGCONT", jobs[cur_job].job_name, jobs[cur_job].process_id);
            log_event(msg);
            kill(jobs[cur_job].process_id, SIGCONT);
        }

        int run_time = 0;
        for (int t = 0; t < time_slice; t++) {
            sleep(1);
            current_time++;
            jobs[cur_job].time_left--;
            run_time++;

            for (int i = 0; i < job_count; i++) {
                if (!jobs[i].has_started && jobs[i].arrival_time == current_time) {
                    enqueue(i);
                }
            }

            if (jobs[cur_job].time_left <= 0)
                break;
        }

        if (jobs[cur_job].time_left <= 0) {
            kill(jobs[cur_job].process_id, SIGTERM);
            waitpid(jobs[cur_job].process_id, NULL, 0);
            sprintf(msg, "%s completed execution. Terminating (PID: %d)", jobs[cur_job].job_name, jobs[cur_job].process_id);
            log_event(msg);
            jobs[cur_job].is_completed = 1;
        } else {
            kill(jobs[cur_job].process_id, SIGSTOP);
            sprintf(msg, "%s ran for %d seconds. Time slice expired – Sending SIGSTOP", jobs[cur_job].job_name, run_time);
            log_event(msg);
            enqueue(cur_job);
        }
    }

    log_event("Finished scheduler exiting.");
    return 0;
}

/* bu kod gerekli araştırmalar sonucu mlq yapısına uymuyor fakat düzgün çalışıyor üstüne geliştir.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define MAX_JOBS 100
#define MAX_NAME_LEN 32
#define LOG_FILE "scheduler.log"

int ready_queue[MAX_JOBS];
int front = 0, rear = 0, qcount = 0;

void enqueue(int job_index) {
    ready_queue[rear] = job_index;
    rear = (rear + 1) % MAX_JOBS;
    qcount++;
}

int is_queue_empty() {
    return qcount == 0;
}

struct Job {
    char job_name[MAX_NAME_LEN];
    int arrival_time;
    int priority_level;
    int total_exec_time;
    int time_left;
    pid_t process_id;
    int has_started;
    int is_completed;
};

struct Job jobs[MAX_JOBS];
int job_count = 0;
int time_slice = 1;

void log_event(const char* message) {
    FILE* log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    fprintf(log, "[%s] [INFO] %s\n", time_str, message);
    fclose(log);
}

void read_jobs(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("jobs.txt");
        exit(1);
    }
    fscanf(file, "TimeSlice %d", &time_slice);
    while (fscanf(file, "%s %d %d %d",
                  jobs[job_count].job_name,
                  &jobs[job_count].arrival_time,
                  &jobs[job_count].priority_level,
                  &jobs[job_count].total_exec_time) == 4) {
        jobs[job_count].time_left = jobs[job_count].total_exec_time;
        jobs[job_count].has_started = 0;
        jobs[job_count].is_completed = 0;
        job_count++;
    }
    fclose(file);
}

void start_job(int index) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sleep", "sleep", "1000", NULL);
        perror("exec failed");
        exit(1);
    } else {
        char msg[256];
        sprintf(msg, "Forking new process for %s", jobs[index].job_name);
        log_event(msg);
        jobs[index].process_id = pid;
        sprintf(msg, "Executing %s (PID: %d) using exec", jobs[index].job_name, pid);
        log_event(msg);
        kill(pid, SIGSTOP);
    }
}

int all_jobs_done() {
    for (int i = 0; i < job_count; i++) {
        if (!jobs[i].is_completed) return 0;
    }
    return 1;
}

int select_next_job(int last_job_index) {
    int selected = -1;
    for (int i = 0; i < qcount; i++) {
        int index = ready_queue[(front + i) % MAX_JOBS];
        if (jobs[index].is_completed) continue;
        if (index == last_job_index && qcount > 1) continue;

        if (selected == -1) {
            selected = index;
            continue;
        }

        struct Job *a = &jobs[index];
        struct Job *b = &jobs[selected];

        if (a->priority_level < b->priority_level) selected = index;
        else if (a->priority_level == b->priority_level) {
            if (a->arrival_time < b->arrival_time) selected = index;
            else if (a->arrival_time == b->arrival_time) {
                if (a->time_left < b->time_left) selected = index;
                else if (a->time_left == b->time_left) {
                    if (index < selected) selected = index;
                }
            }
        }
    }

    if (selected == -1) selected = last_job_index;

    // Remove selected from queue
    for (int i = 0; i < qcount; i++) {
        int idx = (front + i) % MAX_JOBS;
        if (ready_queue[idx] == selected) {
            for (int j = i; j < qcount - 1; j++) {
                ready_queue[(front + j) % MAX_JOBS] = ready_queue[(front + j + 1) % MAX_JOBS];
            }
            rear = (rear - 1 + MAX_JOBS) % MAX_JOBS;
            qcount--;
            break;
        }
    }

    return selected;
}

int main() {
    setenv("TZ", "Europe/Istanbul", 1);
    tzset();

    read_jobs("jobs.txt");

    FILE* log = fopen(LOG_FILE, "w");
    if (log) fclose(log);

    int current_time = 0;
    int prev_job_index = -1;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].arrival_time <= current_time && !jobs[i].has_started) {
            enqueue(i);
        }
    }

    while (!all_jobs_done() || !is_queue_empty()) {
        if (is_queue_empty()) {
            sleep(1);
            current_time++;
            for (int i = 0; i < job_count; i++) {
                if (!jobs[i].has_started && jobs[i].arrival_time <= current_time) {
                    enqueue(i);
                }
            }
            continue;
        }

        int cur_job = select_next_job(prev_job_index);
        prev_job_index = cur_job;
        char msg[256];

        if (!jobs[cur_job].has_started) {
            start_job(cur_job);
            jobs[cur_job].has_started = 1;
        } else {
            sprintf(msg, "Resuming %s (PID: %d) – SIGCONT", jobs[cur_job].job_name, jobs[cur_job].process_id);
            log_event(msg);
            kill(jobs[cur_job].process_id, SIGCONT);
        }

        int run_time = 0;
        for (int t = 0; t < time_slice; t++) {
            sleep(1);
            current_time++;
            jobs[cur_job].time_left--;
            run_time++;

            for (int i = 0; i < job_count; i++) {
                if (!jobs[i].has_started && jobs[i].arrival_time == current_time) {
                    enqueue(i);
                }
            }

            if (jobs[cur_job].time_left <= 0)
                break;
        }

        if (jobs[cur_job].time_left <= 0) {
            kill(jobs[cur_job].process_id, SIGTERM);
            waitpid(jobs[cur_job].process_id, NULL, 0);
            sprintf(msg, "%s completed execution. Terminating (PID: %d)", jobs[cur_job].job_name, jobs[cur_job].process_id);
            log_event(msg);
            jobs[cur_job].is_completed = 1;
        } else {
            kill(jobs[cur_job].process_id, SIGSTOP);
            sprintf(msg, "%s ran for %d seconds. Time slice expired – Sending SIGSTOP", jobs[cur_job].job_name, run_time);
            log_event(msg);
            enqueue(cur_job);
        }
    }

    log_event("All jobs completed. Scheduler exiting.");
    return 0;
}
*/