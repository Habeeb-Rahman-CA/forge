#include "forge_bridge.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
#define REQ_BUF_SIZE 131072U
#define RES_BUF_SIZE 262144U

static const char *DEFAULT_STARTUP_ASM =
    "; Project Forge — Interactive Debugger Default Program\n"
    "; Computes Fibonacci numbers and stores them into memory [0x8000..]\n"
    ".equ COUNT, 8\n"
    ".equ TABLE_ADDR, 0x8000\n\n"
    "main:\n"
    "    MOV R0, 0           ; a = 0\n"
    "    MOV R1, 1           ; b = 1\n"
    "    MOV R2, COUNT       ; remaining terms\n"
    "    MOV R4, TABLE_ADDR  ; output pointer in RAM\n\n"
    "loop:\n"
    "    CMP R2, 0\n"
    "    JZ  done\n"
    "    STORE R0, [R4]      ; write current Fibonacci number to RAM\n"
    "    OUT R0              ; emit to console\n"
    "    MOV R3, R0\n"
    "    ADD R3, R1          ; next = a + b\n"
    "    MOV R0, R1          ; a = b\n"
    "    MOV R1, R3          ; b = next\n"
    "    ADD R4, 2           ; advance word pointer\n"
    "    SUB R2, 1           ; remaining--\n"
    "    JMP loop\n\n"
    "done:\n"
    "    HALT\n";

static void send_http_response(int client_fd, int status, const char *json_body) {
    const char *status_text = (status == 200) ? "OK" : (status == 204 ? "No Content" : "Bad Request");
    size_t body_len = (json_body != NULL) ? strlen(json_body) : 0U;

    char header[512];
    int hlen = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status,
        status_text,
        body_len
    );
    if (hlen > 0) {
        (void)write(client_fd, header, (size_t)hlen);
    }
    if (body_len > 0U && json_body != NULL) {
        (void)write(client_fd, json_body, body_len);
    }
}

int main(int argc, char **argv) {
    uint16_t port = DEFAULT_PORT;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--port <port>]\n", argv[0]);
            return 0;
        }
    }

    forge_bridge_session_t *session = (forge_bridge_session_t *)malloc(sizeof(forge_bridge_session_t));
    if (session == NULL) {
        fprintf(stderr, "Failed to allocate bridge session\n");
        return 1;
    }
    forge_bridge_init(session);
    forge_bridge_assemble(session, DEFAULT_STARTUP_ASM);
    forge_bridge_add_watch(session, 0x8000U);
    forge_bridge_add_watch(session, 0x8002U);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        free(session);
        return 1;
    }

    int opt = 1;
    (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        free(session);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen");
        close(server_fd);
        free(session);
        return 1;
    }

    printf("Forge-16 Native Bridge listening on http://127.0.0.1:%u\n", port);
    fflush(stdout);

    char *req_buf = (char *)malloc(REQ_BUF_SIZE);
    char *res_buf = (char *)malloc(RES_BUF_SIZE);
    if (req_buf == NULL || res_buf == NULL) {
        close(server_fd);
        free(req_buf);
        free(res_buf);
        free(session);
        return 1;
    }

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }

        ssize_t total_read = 0;
        ssize_t n = read(client_fd, req_buf, REQ_BUF_SIZE - 1U);
        if (n > 0) {
            total_read = n;
            req_buf[total_read] = '\0';

            /* Check if Content-Length header specifies more body bytes */
            char *hdr_end = strstr(req_buf, "\r\n\r\n");
            if (hdr_end != NULL) {
                const char *cl = strstr(req_buf, "Content-Length:");
                if (cl == NULL) cl = strstr(req_buf, "content-length:");
                if (cl != NULL && cl < hdr_end) {
                    long content_len = strtol(cl + 15, NULL, 10);
                    size_t header_bytes = (size_t)(hdr_end - req_buf) + 4U;
                    while ((size_t)total_read < header_bytes + (size_t)content_len &&
                           (size_t)total_read + 1U < REQ_BUF_SIZE) {
                        ssize_t more = read(
                            client_fd,
                            req_buf + total_read,
                            REQ_BUF_SIZE - 1U - (size_t)total_read
                        );
                        if (more <= 0) break;
                        total_read += more;
                        req_buf[total_read] = '\0';
                    }
                }
            }

            char method[16] = {0};
            char uri[256] = {0};
            if (sscanf(req_buf, "%15s %255s", method, uri) == 2) {
                if (strcmp(method, "OPTIONS") == 0) {
                    send_http_response(client_fd, 204, NULL);
                } else {
                    const char *body = "";
                    if (hdr_end != NULL) {
                        body = hdr_end + 4;
                    }
                    int status = forge_bridge_handle_request(
                        session,
                        method,
                        uri,
                        body,
                        res_buf,
                        RES_BUF_SIZE
                    );
                    send_http_response(client_fd, status, res_buf);
                }
            }
        }
        close(client_fd);
    }

    free(req_buf);
    free(res_buf);
    close(server_fd);
    free(session);
    return 0;
}

