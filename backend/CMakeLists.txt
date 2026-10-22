cmake_minimum_required(VERSION 3.16)
project(str_bridge C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
find_package(Threads REQUIRED)
include_directories(include scenarios)
set(SCENARIO_SOURCES
    scenarios/scenario_common.c
    scenarios/normal.c
    scenarios/race_condition.c
    scenarios/starvation.c
    scenarios/deadlock.c
    scenarios/busy_waiting.c
    scenarios/priority_inversion.c
)
add_executable(bridge_server src/main.c src/bridge.c src/events.c src/vehicle.c src/websocket_server.c src/simulation.c ${SCENARIO_SOURCES})
target_link_libraries(bridge_server PRIVATE Threads::Threads)
if(WIN32)
  target_link_libraries(bridge_server PRIVATE ws2_32)
endif()
add_executable(bridge_tests tests/test_bridge.c src/bridge.c src/events.c src/vehicle.c src/simulation.c ${SCENARIO_SOURCES})
target_link_libraries(bridge_tests PRIVATE Threads::Threads)
enable_testing()
add_test(NAME monitor COMMAND bridge_tests)
