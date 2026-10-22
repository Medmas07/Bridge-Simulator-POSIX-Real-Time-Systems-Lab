#include "events.h"
#include "simulation.h"

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_handle;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_handle;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSE_SOCKET close
#endif

enum { BRIDGE_WEBSOCKET_PORT = 8081 };

static void report_socket_error(const char *operation) {
#ifdef _WIN32
    fprintf(stderr, "WebSocket: %s echoue (code %d)\n", operation, WSAGetLastError());
#else
    fprintf(stderr, "WebSocket: %s echoue (%s)\n", operation, strerror(errno));
#endif
}

typedef struct {
    uint32_t state[5];
    uint64_t bit_count;
    unsigned char buffer[64];
} Sha1;

static uint32_t rotate_left(uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32U - bits));
}

static void sha1_transform(Sha1 *sha, const unsigned char block[64]) {
    uint32_t words[80];
    uint32_t a, b, c, d, e;
    for (int i = 0; i < 16; i++) {
        words[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) words[i] = rotate_left(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
    a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3]; e = sha->state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t function, constant;
        if (i < 20) { function = (b & c) | ((~b) & d); constant = 0x5A827999U; }
        else if (i < 40) { function = b ^ c ^ d; constant = 0x6ED9EBA1U; }
        else if (i < 60) { function = (b & c) | (b & d) | (c & d); constant = 0x8F1BBCDCU; }
        else { function = b ^ c ^ d; constant = 0xCA62C1D6U; }
        uint32_t temporary = rotate_left(a, 5) + function + e + constant + words[i];
        e = d; d = c; c = rotate_left(b, 30); b = a; a = temporary;
    }
    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d; sha->state[4] += e;
}

static void sha1_init(Sha1 *sha) {
    sha->state[0] = 0x67452301U; sha->state[1] = 0xEFCDAB89U; sha->state[2] = 0x98BADCFEU;
    sha->state[3] = 0x10325476U; sha->state[4] = 0xC3D2E1F0U; sha->bit_count = 0;
}

static void sha1_update(Sha1 *sha, const unsigned char *data, size_t length) {
    size_t index = (size_t)((sha->bit_count >> 3) & 63U);
    sha->bit_count += (uint64_t)length << 3;
    for (size_t i = 0; i < length; i++) {
        sha->buffer[index++] = data[i];
        if (index == 64) { sha1_transform(sha, sha->buffer); index = 0; }
    }
}

static void sha1_final(Sha1 *sha, unsigned char digest[20]) {
    unsigned char length_bytes[8];
    uint64_t bits = sha->bit_count;
    for (int i = 7; i >= 0; i--) { length_bytes[i] = (unsigned char)(bits & 0xffU); bits >>= 8; }
    unsigned char marker = 0x80;
    sha1_update(sha, &marker, 1);
    unsigned char zero = 0;
    while (((sha->bit_count >> 3) & 63U) != 56U) sha1_update(sha, &zero, 1);
    sha1_update(sha, length_bytes, 8);
    for (int i = 0; i < 5; i++) {
        digest[i * 4] = (unsigned char)(sha->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(sha->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(sha->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)sha->state[i];
    }
}

static void base64_encode(const unsigned char *input, size_t length, char *output, size_t output_size) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t source = 0, destination = 0;
    while (source < length && destination + 4 < output_size) {
        uint32_t value = (uint32_t)input[source++] << 16;
        int remaining = (int)(length - source);
        if (remaining >= 1) value |= (uint32_t)input[source++] << 8;
        if (remaining >= 2) value |= input[source++];
        output[destination++] = alphabet[(value >> 18) & 63U];
        output[destination++] = alphabet[(value >> 12) & 63U];
        output[destination++] = remaining >= 1 ? alphabet[(value >> 6) & 63U] : '=';
        output[destination++] = remaining >= 2 ? alphabet[value & 63U] : '=';
    }
    output[destination] = '\0';
}

static int send_all(socket_handle socket, const void *data, size_t length) {
    const char *bytes = data;
    while (length > 0) {
        int sent = send(socket, bytes, (int)length, 0);
        if (sent <= 0) return 0;
        bytes += sent;
        length -= (size_t)sent;
    }
    return 1;
}

static int receive_all(socket_handle socket, void *data, size_t length) {
    unsigned char *bytes = data;
    while (length > 0) {
        int received = recv(socket, (char *)bytes, (int)length, 0);
        if (received <= 0) return 0;
        bytes += received;
        length -= (size_t)received;
    }
    return 1;
}

static int websocket_send_text(socket_handle client, const char *text) {
    unsigned char header[10];
    size_t length = strlen(text);
    size_t header_length;
    header[0] = 0x81;
    if (length < 126) { header[1] = (unsigned char)length; header_length = 2; }
    else if (length <= 65535) {
        header[1] = 126; header[2] = (unsigned char)(length >> 8); header[3] = (unsigned char)length; header_length = 4;
    } else return 0;
    return send_all(client, header, header_length) && send_all(client, text, length);
}

static int websocket_receive_text(socket_handle client, char *output, size_t output_size) {
    unsigned char header[2], mask[4];
    uint64_t length;
    if (!receive_all(client, header, 2)) return 0;
    int opcode = header[0] & 0x0f;
    int masked = header[1] & 0x80;
    length = header[1] & 0x7f;
    if (length == 126) {
        unsigned char extended[2];
        if (!receive_all(client, extended, 2)) return 0;
        length = ((uint64_t)extended[0] << 8) | extended[1];
    } else if (length == 127) return 0;
    if (!masked || length + 1 > output_size) return 0;
    if (!receive_all(client, mask, 4) || !receive_all(client, output, (size_t)length)) return 0;
    for (uint64_t i = 0; i < length; i++) output[i] ^= (char)mask[i % 4];
    output[length] = '\0';
    if (opcode == 8) return 0;
    return opcode == 1;
}

static int websocket_handshake(socket_handle client) {
    char request[4096] = {0};
    int received = recv(client, request, sizeof(request) - 1, 0);
    if (received <= 0) return 0;
    char *key_header = strstr(request, "Sec-WebSocket-Key:");
    if (!key_header) return 0;
    key_header += strlen("Sec-WebSocket-Key:");
    while (*key_header == ' ') key_header++;
    char *key_end = strstr(key_header, "\r\n");
    if (!key_end) return 0;
    char key[128];
    size_t key_length = (size_t)(key_end - key_header);
    if (key_length >= sizeof(key)) return 0;
    memcpy(key, key_header, key_length);
    key[key_length] = '\0';

    char combined[256];
    unsigned char digest[20];
    char accept[64];
    Sha1 sha;
    snprintf(combined, sizeof(combined), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    sha1_init(&sha);
    sha1_update(&sha, (const unsigned char *)combined, strlen(combined));
    sha1_final(&sha, digest);
    base64_encode(digest, sizeof(digest), accept, sizeof(accept));

    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", accept);
    return send_all(client, response, strlen(response));
}

static ScenarioKind parse_scenario(const char *command) {
    if (strstr(command, "RACE_CONDITION") || strstr(command, "RACE CONDITION")) return SCENARIO_RACE_CONDITION;
    if (strstr(command, "STARVATION") || strstr(command, "FAMINE")) return SCENARIO_STARVATION;
    if (strstr(command, "DEADLOCK")) return SCENARIO_DEADLOCK;
    if (strstr(command, "BUSY_WAITING") || strstr(command, "BUSY WAITING")) return SCENARIO_BUSY_WAITING;
    if (strstr(command, "PRIORITY_INVERSION") || strstr(command, "PRIORITY INVERSION")) return SCENARIO_PRIORITY_INVERSION;
    return SCENARIO_NORMAL;
}

static void handle_command(Simulation *simulation, const char *command) {
    if (strstr(command, "CREATE_VEHICLE")) {
        simulation_create_vehicle(simulation, strstr(command, "TRUCK") ? VEHICLE_TRUCK : VEHICLE_CAR);
    } else if (strstr(command, "START_SCENARIO")) {
        simulation_start_scenario(simulation, parse_scenario(command));
    } else if (strstr(command, "RESET")) simulation_reset(simulation);
    else if (strstr(command, "PAUSE")) simulation_pause(simulation);
    else if (strstr(command, "RESUME")) simulation_resume(simulation);
    else if (strstr(command, "SET_SPEED")) {
        if (strstr(command, "0.5")) simulation_set_speed(simulation, 0.5);
        else if (strstr(command, "2")) simulation_set_speed(simulation, 2.0);
        else simulation_set_speed(simulation, 1.0);
    } else if (strstr(command, "SET_STEP_MODE")) {
        simulation_set_step_mode(simulation, strstr(command, "true") || strstr(command, "TRUE"));
    } else if (strstr(command, "STEP")) simulation_step(simulation);
}

static void serve_client(socket_handle client, Simulation *simulation) {
    char message[8192];
    char event[8192];
    events_clear();
    simulation_snapshot_json(simulation, message, sizeof(message));
    if (!websocket_send_text(client, message)) return;
    for (;;) {
        while (events_pop(event, sizeof(event))) {
            if (!websocket_send_text(client, event)) return;
        }
        fd_set readers;
        struct timeval timeout = {0, 100000};
        FD_ZERO(&readers);
        FD_SET(client, &readers);
        int ready = select((int)client + 1, &readers, NULL, NULL, &timeout);
        if (ready == SOCKET_ERROR) return;
        if (ready > 0 && FD_ISSET(client, &readers)) {
            if (!websocket_receive_text(client, message, sizeof(message))) return;
            handle_command(simulation, message);
        }
    }
}

void websocket_server_run(Simulation *simulation) {
#ifdef _WIN32
    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        report_socket_error("WSAStartup");
        return;
    }
#endif
    socket_handle server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        report_socket_error("socket");
        return;
    }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(BRIDGE_WEBSOCKET_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR || listen(server, 4) == SOCKET_ERROR) {
        report_socket_error("bind/listen sur le port 8081");
        CLOSE_SOCKET(server);
        return;
    }
    printf("Bridge Simulator WebSocket: ws://localhost:%d\n", BRIDGE_WEBSOCKET_PORT);
    for (;;) {
        socket_handle client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        if (websocket_handshake(client)) serve_client(client, simulation);
        CLOSE_SOCKET(client);
    }
}
