#include "ch.h"
#include "hal.h"
#include "chprintf.h"

static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

#define BUFFER_SIZE 10

// Первый кольцевой буфер
static int buffer1[BUFFER_SIZE];
static size_t buffer1_head = 0;
static size_t buffer1_tail = 0;
static size_t buffer1_count = 0;

// Второй кольцевой буфер
static int buffer2[BUFFER_SIZE];
static size_t buffer2_head = 0;
static size_t buffer2_tail = 0;
static size_t buffer2_count = 0;

// Таймеры для управления скоростью работы
static systime_t last_task1_time = 0;
static systime_t last_task2_time = 0;
static systime_t last_monitor_time = 0;

// Скорости работы задач
static size_t task1_speed = 200;    // Запись в buffer1 и чтение из buffer2
static size_t task2_speed = 300;    // Чтение из buffer1 и запись в buffer2
static size_t monitor_speed = 1000; // Вывод состояния
static size_t default_timeout = 10; // Таймаут основного цикла

static int number_counter = 1;

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
    
    chprintf(serial, "\r\n=== Dual Ring Buffer (Main Loop) ===\r\n");
    chprintf(serial, "Task1: writes to buffer1, reads from buffer2 every %u ms\r\n", task1_speed);
    chprintf(serial, "Task2: reads from buffer1, writes to buffer2 every %u ms\r\n", task2_speed);
    chprintf(serial, "Monitor: prints status every %u ms\r\n", monitor_speed);
    chprintf(serial, "Buffer size: %d items each\r\n\r\n", BUFFER_SIZE);

    while (true) {
        systime_t now = chVTGetSystemTime();
        
        // Задача 1: запись в buffer1 и чтение из buffer2
        if (now - last_task1_time >= TIME_MS2I(task1_speed)) {
            // 1. Запись в buffer1
            if (buffer1_count < BUFFER_SIZE) {
                int num = number_counter++;
                buffer1[buffer1_head] = num;
                buffer1_head = (buffer1_head + 1) % BUFFER_SIZE;
                buffer1_count++;
                chprintf(serial, "[TASK1] Added to buffer1: %3d (count: %2u/10)\r\n", num, buffer1_count);
            } else {
                chprintf(serial, "[TASK1] Buffer1 full, skipping write\r\n");
            }
            
            // 2. Чтение из buffer2
            if (buffer2_count > 0) {
                int num = buffer2[buffer2_tail];
                buffer2_tail = (buffer2_tail + 1) % BUFFER_SIZE;
                buffer2_count--;
                chprintf(serial, "[TASK1] Read from buffer2: %3d (count: %2u/10)\r\n", num, buffer2_count);
            }
            
            last_task1_time = now;
        }
        
        // Задача 2: чтение из buffer1 и запись в buffer2
        if (now - last_task2_time >= TIME_MS2I(task2_speed)) {
            // 1. Чтение из buffer1
            if (buffer1_count > 0) {
                int num = buffer1[buffer1_tail];
                buffer1_tail = (buffer1_tail + 1) % BUFFER_SIZE;
                buffer1_count--;
                chprintf(serial, "[TASK2] Read from buffer1: %3d (count: %2u/10)\r\n", num, buffer1_count);
            }
            
            // 2. Запись в buffer2
            if (buffer2_count < BUFFER_SIZE) {
                int num = number_counter++;
                buffer2[buffer2_head] = num;
                buffer2_head = (buffer2_head + 1) % BUFFER_SIZE;
                buffer2_count++;
                chprintf(serial, "[TASK2] Added to buffer2: %3d (count: %2u/10)\r\n", num, buffer2_count);
            } else {
                chprintf(serial, "[TASK2] Buffer2 full, skipping write\r\n");
            }
            
            last_task2_time = now;
        }
        
        // Мониторинг состояния буферов
        if (now - last_monitor_time >= TIME_MS2I(monitor_speed)) {
            chprintf(serial, "\r\n=== Buffer Status ===\r\n");
            print_buffer_state("Buffer1", buffer1, buffer1_head, buffer1_tail, buffer1_count);
            print_buffer_state("Buffer2", buffer2, buffer2_head, buffer2_tail, buffer2_count);
            chprintf(serial, "====================\r\n\r\n");
            last_monitor_time = now;
        }
        
        chThdSleepMilliseconds(default_timeout);
    }
}