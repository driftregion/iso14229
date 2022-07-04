/**
 * @file test_example.c
 * @brief 保证例子能够正常运行
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "server.h"
#include "client.h"
#include "port.h"
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

const char *argv[] = {"foo", "vcan0"};

int server_pid = 0, client_pid = 0;
bool timed_out = false;

void *timeout(void *arg) {
    (void)arg;
    printf("watchdog thread started. . .\n");
    sleep(3);
    timed_out = true;
    printf("terminating PIDs %d %d\n", server_pid, client_pid);
    int err = 0;
    err = kill(server_pid, SIGINT);
    printf("err: %d\n", err);
    err = kill(client_pid, SIGINT);
    printf("err: %d\n", err);
    return NULL;
}

int main() {
    int result = 0;
    server_pid = fork();
    if (0 == server_pid) {
        portSetup(2, (char **)argv);
        result = run_server_blocking();
        assert(result == 0);
        exit(0);
    }
    client_pid = fork();
    if (0 == client_pid) {
        portSetup(2, (char **)argv);
        result = run_client_blocking();
        assert(result == 0);
        exit(0);
    }

    bool should_exit = false;
    pthread_t handle;
    pthread_create(&handle, NULL, timeout, &should_exit);

    printf("server pid %d, client pid:%d\n", server_pid, client_pid);
    int server_status = 0, client_status = 0;
    waitpid(client_pid, &client_status, 0);
    waitpid(server_pid, &server_status, 0);

    pthread_cancel(handle);

    assert(client_status == 0);
    assert(server_status == 0);
    assert(timed_out == false);
}
