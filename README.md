1-File content and layout

Makefile:
Compiles the scheduler and job_runner files. It also creates symbolic links to job_runner for jobA, jobB, jobC.

scheduler.c:
A preemptive job scheduler. Queues jobs, starts, stops and runs them according to the time frame.

job_runner.c:
A program that runs jointly for jobA, jobB, jobC. Simulates sleep operation for the specified time.

jobs.txt:
Input file where jobs are defined. Contains arrival time, priority level and run time.

scheduler.log:
Events of the scheduler (start, stop, finish etc.) are recorded here with a timestamp.

2-Preparing the Necessary Environment:

A Linux-based system, virtual machine or Docker container can be used as a development environment (f0r our homework we used docker...). If you are connecting to a remote machine via SSH connection, you can log in with the following command:
ssh <username>@localhost -p <port>

Switch to Project Folder:
After logging in via SSH, switch to the directory where the project is located:
cd /file/path/hostvolume
cd ~/hostvolume

3-Build Process:
Run make command to compile all files and create the necessary links:

"make" this command:
Converts the file scheduler to an executable file named scheduler.
Generates the file job_runner from job_runner.c.
Creates symbolic links for job_runner named jobA, jobB, jobC.
Start Scheduler
Run the scheduler after the compilation is complete:
./scheduler

Cleaning (Optional)
To delete compiled files and links:
make clean

