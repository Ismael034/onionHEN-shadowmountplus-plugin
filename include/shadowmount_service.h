#pragma once

#include <pthread.h>

typedef struct shadowmount_service {
    pthread_mutex_t mutex;
    pthread_t thread;
    int initialized;
    int running;
    int thread_created;
} shadowmount_service;

int shadowmount_service_init(shadowmount_service *service);
int shadowmount_service_start(shadowmount_service *service);
void shadowmount_service_request_scan(shadowmount_service *service);
int shadowmount_service_running(shadowmount_service *service);
void shadowmount_service_stop(shadowmount_service *service);
void shadowmount_service_destroy(shadowmount_service *service);
