CC = gcc
CFLAGS = -Wall -pthread -O2
TARGET = market_sim
SRC = 820210314_market_sim.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) log.txt
