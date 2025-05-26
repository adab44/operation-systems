This project is simulation of an online grocery store payment and inventory system where multiple customer flows are processed simultaneously. The customer adds the product to the cart, attempts to pay, and either waits or fails if stock is low or the payment slot is full.

Documentation:
.
├── 820210314_market_sim.c #main C code
├── input.txt #Input file(customer and stock information)
├── log.txt #Output log file(automatically generated)
├── Makefile #compile and run automation
├── Dockerfile #Docker environment of the project
└── market_sim #compiled program (created after make)
└──Report.pdf

Requirements:
GCC (gcc)
POSIX thread (pthread)
make
(Optional) Docker
Linux environment (recommended)

Compile and Run:
1. If you just want to compile:
make
2. Compile and run:
make run
3. To manually run the compiled binary:
./market_sim
4. To see the log output:
cat log.txt