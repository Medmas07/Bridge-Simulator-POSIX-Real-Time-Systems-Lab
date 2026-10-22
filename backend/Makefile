CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -pthread
INCLUDES = -Iinclude
SOURCES = src/main.c src/bridge.c src/events.c src/vehicle.c src/websocket_server.c src/simulation.c

all: bridge_server bridge_tests
bridge_server: $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@
bridge_tests: tests/test_bridge.c src/bridge.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
test: bridge_tests
	./bridge_tests
clean:
	rm -f bridge_server bridge_tests
