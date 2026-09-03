#include "shadowmount_service.h"

#include <string.h>

#include "shadowmount_core.h"

static void *scanner_thread_main(void *context) {
    shadowmount_service *service = context;
    (void)shadowmount_core_run();

    pthread_mutex_lock(&service->mutex);
    service->running = 0;
    pthread_mutex_unlock(&service->mutex);
    return NULL;
}

int shadowmount_service_init(shadowmount_service *service) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    if (pthread_mutex_init(&service->mutex, NULL) != 0) return 0;
    service->initialized = 1;
    return 1;
}

int shadowmount_service_start(shadowmount_service *service) {
    if (!service || !service->initialized) return 0;
    shadowmount_service_stop(service);
    shadowmount_core_prepare();

    pthread_mutex_lock(&service->mutex);
    service->running = 1;
    const int result = pthread_create(
        &service->thread, NULL, scanner_thread_main, service);
    if (result == 0) {
        service->thread_created = 1;
    } else {
        service->running = 0;
    }
    pthread_mutex_unlock(&service->mutex);
    return result == 0;
}

void shadowmount_service_request_scan(shadowmount_service *service) {
    if (!shadowmount_service_running(service)) return;
    shadowmount_core_request_scan();
}

int shadowmount_service_running(shadowmount_service *service) {
    if (!service || !service->initialized) return 0;
    pthread_mutex_lock(&service->mutex);
    const int is_running = service->running;
    pthread_mutex_unlock(&service->mutex);
    return is_running;
}

void shadowmount_service_stop(shadowmount_service *service) {
    if (!service || !service->initialized) return;

    pthread_t thread = {0};
    int join_thread = 0;
    pthread_mutex_lock(&service->mutex);
    if (service->thread_created) {
        shadowmount_core_request_stop();
        thread = service->thread;
        join_thread = 1;
    }
    pthread_mutex_unlock(&service->mutex);

    if (join_thread) pthread_join(thread, NULL);

    pthread_mutex_lock(&service->mutex);
    service->running = 0;
    service->thread_created = 0;
    pthread_mutex_unlock(&service->mutex);
}

void shadowmount_service_destroy(shadowmount_service *service) {
    if (!service || !service->initialized) return;
    shadowmount_service_stop(service);
    pthread_mutex_destroy(&service->mutex);
    memset(service, 0, sizeof(*service));
}
