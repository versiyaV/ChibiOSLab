#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include <stdlib.h>

static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

#define BUFFER_SIZE 10
#define USER_TASKS 10
#define MONITOR_INTERVAL 1000 // 1 second

// Структура для буфера
typedef struct {
    int data[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    int readers;
    int writers;
} TicketBuffer;

// Структура для пользовательской задачи
typedef struct {
    systime_t last_run;
    systime_t interval;
    int task_num;
} UserTask;

// Два буфера
static TicketBuffer buffer1, buffer2;

// Пользовательские задачи
static UserTask user_tasks[USER_TASKS];

// Инициализация буфера
static void buffer_init(TicketBuffer *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->readers = 0;
    buf->writers = 0;
}

// Попытка чтения из буфера
static bool buffer_read(TicketBuffer *buf, int *value) {
    if (buf->writers > 0) {
        return false; // Есть писатель - чтение невозможно
    }
    
    if (buf->count == 0) {
        return false; // Буфер пуст
    }
    
    buf->readers++;
    *value = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;
    buf->readers--;
    
    return true;
}

// Попытка записи в буфер
static bool buffer_write(TicketBuffer *buf, int value) {
    if (buf->readers > 0 || buf->writers > 0) {
        return false; // Есть читатели или писатели - запись невозможна
    }
    
    if (buf->count == BUFFER_SIZE) {
        return false; // Буфер полон
    }
    
    buf->writers++;
    buf->data[buf->head] = value;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;
    buf->writers--;
    
    return true;
}

// Вывод состояния буфера
static void buffer_print(TicketBuffer *buf, const char *name) {
    chprintf(serial, "%s: count=%2u, head=%2u, tail=%2u\r\n", 
             name, buf->count, buf->head, buf->tail);
    chprintf(serial, "Contents: ");
    
    if (buf->count == 0) {
        chprintf(serial, "empty");
    } else {
        size_t idx = buf->tail;
        for (size_t i = 0; i < buf->count; i++) {
            chprintf(serial, "%3d ", buf->data[idx]);
            idx = (idx + 1) % BUFFER_SIZE;
        }
    }
    chprintf(serial, "\r\n");
}

// Инициализация пользовательских задач
static void init_user_tasks(void) {
    for (int i = 0; i < USER_TASKS; i++) {
        user_tasks[i].task_num = i + 1;
        user_tasks[i].interval = 100 + rand() % 400; // Случайный интервал 100-500 мс
        user_tasks[i].last_run = 0;
    }
}

// Обработка одной пользовательской задачи
static void process_user_task(UserTask *task) {
    systime_t now = chVTGetSystemTime();
    
    if (now - task->last_run >= TIME_MS2I(task->interval)) {
        // Случайный выбор буфера (1 или 2)
        TicketBuffer *buf = (rand() % 2) ? &buffer1 : &buffer2;
        const char *buf_name = (buf == &buffer1) ? "Buffer1" : "Buffer2";
        
        // Случайное действие (чтение или запись)
        if (rand() % 2) {
            // Запись
            static int counter = 1;
            int value = counter++;
            if (buffer_write(buf, value)) {
                chprintf(serial, "[Task %d] Wrote to %s: %d\r\n", 
                         task->task_num, buf_name, value);
            } else {
                chprintf(serial, "[Task %d] %s is busy or full\r\n", 
                         task->task_num, buf_name);
            }
        } else {
            // Чтение
            int value;
            if (buffer_read(buf, &value)) {
                chprintf(serial, "[Task %d] Read from %s: %d\r\n", 
                         task->task_num, buf_name, value);
            } else {
                chprintf(serial, "[Task %d] %s is busy or empty\r\n", 
                         task->task_num, buf_name);
            }
        }
        
        task->last_run = now;
        task->interval = 100 + rand() % 400; // Новый случайный интервал
    }
}

int main(void) {
    halInit();
    chSysInit();
    sdStart(&SD1, NULL);
    
    // Инициализация буферов
    buffer_init(&buffer1);
    buffer_init(&buffer2);
    
    // Инициализация пользовательских задач
    init_user_tasks();
    
    chprintf(serial, "\r\n=== Ticket System with Two Buffers ===\r\n");
    chprintf(serial, "Running %d user tasks in main loop...\r\n\r\n", USER_TASKS);
    
    systime_t last_monitor_time = 0;
    
    while (true) {
        systime_t now = chVTGetSystemTime();
        
        // Обработка всех пользовательских задач
        for (int i = 0; i < USER_TASKS; i++) {
            process_user_task(&user_tasks[i]);
        }
        
        // Периодический вывод состояния буферов
        if (now - last_monitor_time >= TIME_MS2I(MONITOR_INTERVAL)) {
            chprintf(serial, "\r\n=== Buffer Status ===\r\n");
            buffer_print(&buffer1, "Buffer1");
            buffer_print(&buffer2, "Buffer2");
            chprintf(serial, "====================\r\n\r\n");
            last_monitor_time = now;
        }
        
        // Небольшая задержка для уменьшения нагрузки на CPU
        chThdSleepMilliseconds(10);
    }
}