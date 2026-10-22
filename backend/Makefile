CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -pthread
INCLUDES = -Iinclude -Iscenarios
SCENARIO_SOURCES = scenarios/scenario_common.c scenarios/normal.c scenarios/race_condition.c scenarios/starvation.c scenarios/deadlock.c scenarios/busy_waiting.c scenarios/priority_inversion.c
SOURCES = src/main.c src/bridge.c src/events.c src/vehicle.c src/websocket_server.c src/simulation.c $(SCENARIO_SOURCES)
ifeq ($(OS),Windows_NT)
LDLIBS += -lws2_32
endif

all: bridge_server bridge_tests
bridge_server: $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@ $(LDLIBS)
bridge_tests: tests/test_bridge.c src/bridge.c src/events.c src/vehicle.c src/simulation.c $(SCENARIO_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
test: bridge_tests
	./bridge_tests
clean:
	rm -f bridge_server bridge_tests
