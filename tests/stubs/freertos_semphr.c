#include "freertos/semphr.h"

#include <pthread.h>
#include <stdlib.h>

/* Host-only FreeRTOS semaphore shim. These tests compile the actual store
 * implementation unchanged and exercise it with concurrent pthreads. */
struct mock_agenda_mutex {
    pthread_mutex_t value;
};

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    struct mock_agenda_mutex *mutex = malloc(sizeof(*mutex));
    if (!mutex) return NULL;
    if (pthread_mutex_init(&mutex->value, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t timeout)
{
    if (!mutex || timeout != portMAX_DELAY) return pdFALSE;
    return pthread_mutex_lock(&mutex->value) == 0 ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    return mutex && pthread_mutex_unlock(&mutex->value) == 0 ? pdTRUE : pdFALSE;
}
