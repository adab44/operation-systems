CC=gcc
CFLAGS=-Wall

all: scheduler jobA jobB jobC

scheduler: scheduler.c
	$(CC) $(CFLAGS) scheduler.c -o scheduler

job_runner: job_runner.c
	$(CC) $(CFLAGS) job_runner.c -o job_runner

jobA: job_runner
	ln -sf job_runner jobA

jobB: job_runner
	ln -sf job_runner jobB

jobC: job_runner
	ln -sf job_runner jobC

clean:
	rm -f scheduler job_runner jobA jobB jobC scheduler.log
