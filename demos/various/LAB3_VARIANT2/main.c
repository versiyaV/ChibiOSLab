#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "chmtx.h"
#include "chevents.h"

static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

#define BUFFER_SIZE 10

// Первый кольцевой буфер и его переменные состояния
static int buffer1[BUFFER_SIZE];
static size_t buffer1_head = 0;
static size_t buffer1_tail = 0;
static size_t buffer1_count = 0;

// Второй кольцевой буфер и его переменные состояния
static int buffer2[BUFFER_SIZE];
static size_t buffer2_head = 0;
static size_t buffer2_tail = 0;
static size_t buffer2_count = 0;

// Скорости работы задач
static size_t task1_speed = 200;
static size_t task2_speed = 300;
static size_t monitor_speed = 100;

// Мьютексы для синхронизации
static mutex_t buffer1_mutex;
static mutex_t buffer2_mutex;
static mutex_t print_mutex;

// События для синхронизации
static event_source_t buffer1_event;
static event_source_t buffer2_event;

static int number_counter = 1;

static void print_buffer_state(const char* name, int* buffer, size_t head, size_t tail, size_t count);

// Задача 1: запись в буфер 1, чтение из буфера 2
static THD_WORKING_AREA(waTask1, 256);
static THD_FUNCTION(Task1, arg) {
    (void)arg;
    event_listener_t el;
    chEvtRegister(&buffer2_event, &el, 0);
    
    while (true) {
        // 1. Сначала запись в буфер 1
        int num = number_counter++;
        
        chMtxLock(&buffer1_mutex);
        if (buffer1_count == BUFFER_SIZE) {
            chMtxUnlock(&buffer1_mutex);
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[TASK1] Buffer1 full, skipping write\r\n");
            chMtxUnlock(&print_mutex);
        } else {
            buffer1[buffer1_head] = num;
            buffer1_head = (buffer1_head + 1) % BUFFER_SIZE;
            buffer1_count++;
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[TASK1] Added to buffer1: %3d (count: %2u/10)\r\n", num, buffer1_count);
            chMtxUnlock(&print_mutex);
            
            chEvtBroadcast(&buffer1_event);
            chMtxUnlock(&buffer1_mutex);
        }
        
        // 2. Затем чтение из буфера 2 (без ожидания внутри мьютекса)
        eventmask_t evt = chEvtWaitOneTimeout(EVENT_MASK(0), TIME_MS2I(10));
        if (evt != 0) {
            chMtxLock(&buffer2_mutex);
            if (buffer2_count > 0) {
                int num = buffer2[buffer2_tail];
                buffer2_tail = (buffer2_tail + 1) % BUFFER_SIZE;
                buffer2_count--;
                
                chMtxLock(&print_mutex);
                chprintf(serial, "[TASK1] Read from buffer2: %3d (count: %2u/10)\r\n", num, buffer2_count);
                chMtxUnlock(&print_mutex);
            }
            chMtxUnlock(&buffer2_mutex);
        }
        
        chThdSleepMilliseconds(task1_speed);
    }
}

// Задача 2: чтение из буфера 1, запись в буфер 2
static THD_WORKING_AREA(waTask2, 256);
static THD_FUNCTION(Task2, arg) {
    (void)arg;
    event_listener_t el;
    chEvtRegister(&buffer1_event, &el, 0);
    
    while (true) {
        // 1. Сначала чтение из буфера 1 (без ожидания внутри мьютекса)
        eventmask_t evt = chEvtWaitOneTimeout(EVENT_MASK(0), TIME_MS2I(10));
        if (evt != 0) {
            chMtxLock(&buffer1_mutex);
            if (buffer1_count > 0) {
                int num = buffer1[buffer1_tail];
                buffer1_tail = (buffer1_tail + 1) % BUFFER_SIZE;
                buffer1_count--;
                
                chMtxLock(&print_mutex);
                chprintf(serial, "[TASK2] Read from buffer1: %3d (count: %2u/10)\r\n", num, buffer1_count);
                chMtxUnlock(&print_mutex);
            }
            chMtxUnlock(&buffer1_mutex);
        }
        
        // 2. Затем запись в буфер 2
        int num = number_counter++;
        
        chMtxLock(&buffer2_mutex);
        if (buffer2_count == BUFFER_SIZE) {
            chMtxUnlock(&buffer2_mutex);
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[TASK2] Buffer2 full, skipping write\r\n");
            chMtxUnlock(&print_mutex);
        } else {
            buffer2[buffer2_head] = num;
            buffer2_head = (buffer2_head + 1) % BUFFER_SIZE;
            buffer2_count++;
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[TASK2] Added to buffer2: %3d (count: %2u/10)\r\n", num, buffer2_count);
            chMtxUnlock(&print_mutex);
            
            chEvtBroadcast(&buffer2_event);
            chMtxUnlock(&buffer2_mutex);
        }
        
        chThdSleepMilliseconds(task2_speed);
    }
}

// Задача мониторинга: вывод состояния буферов
static THD_WORKING_AREA(waMonitor, 256);
static THD_FUNCTION(Monitor, arg) {
    (void)arg;
    
    while (true) {
        // Сначала блокируем оба буфера, затем выводим
        chMtxLock(&buffer1_mutex);
        chMtxLock(&buffer2_mutex);
        
        chMtxLock(&print_mutex);
        chprintf(serial, "\r\n=== Buffer Status ===\r\n");
        
        print_buffer_state("Buffer1", buffer1, buffer1_head, buffer1_tail, buffer1_count);
        print_buffer_state("Buffer2", buffer2, buffer2_head, buffer2_tail, buffer2_count);
        
        chprintf(serial, "====================\r\n\r\n");
        chMtxUnlock(&print_mutex);
        
        chMtxUnlock(&buffer2_mutex);
        chMtxUnlock(&buffer1_mutex);
        
        chThdSleepMilliseconds(monitor_speed);
    }
}

// Функция для вывода состояния буфера
static void print_buffer_state(const char* name, int* buffer, size_t head, size_t tail, size_t count) {
    chprintf(serial, "%s: count=%2u, head=%2u, tail=%2u\r\n", name, count, head, tail);
    chprintf(serial, "Contents: ");
    
    if (count == 0) {
        chprintf(serial, "empty");
    } else {
        size_t idx = tail;
        for (size_t i = 0; i < count; i++) {
            chprintf(serial, "%3d ", buffer[idx]);
            idx = (idx + 1) % BUFFER_SIZE;
        }
    }
    chprintf(serial, "\r\n");
}

int main(void) {
    halInit();
    chSysInit();
    sdStart(&SD1, NULL);
    
    // Инициализация мьютексов
    chMtxObjectInit(&buffer1_mutex);
    chMtxObjectInit(&buffer2_mutex);
    chMtxObjectInit(&print_mutex);
    
    // Инициализация событий
    chEvtObjectInit(&buffer1_event);
    chEvtObjectInit(&buffer2_event);
    
    // Создание задач
    chThdCreateStatic(waTask1, sizeof(waTask1), NORMALPRIO, Task1, NULL);
    chThdCreateStatic(waTask2, sizeof(waTask2), NORMALPRIO, Task2, NULL);
    chThdCreateStatic(waMonitor, sizeof(waMonitor), NORMALPRIO-1, Monitor, NULL);

    // Вывод информации о запуске
    chMtxLock(&print_mutex);
    chprintf(serial, "\r\n=== Dual Ring Buffer Demo ===\r\n");
    chprintf(serial, "Task1 speed: %u ms (writes to buffer1, reads from buffer2)\r\n", task1_speed);
    chprintf(serial, "Task2 speed: %u ms (reads from buffer1, writes to buffer2)\r\n", task2_speed);
    chprintf(serial, "Monitor speed: %u ms\r\n", monitor_speed);
    chprintf(serial, "Buffer size: %d items each\r\n\r\n", BUFFER_SIZE);
    chMtxUnlock(&print_mutex);

    while (true) {
        chThdSleepMilliseconds(1000);
    }
}