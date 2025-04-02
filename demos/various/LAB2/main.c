#include "ch.h"
#include "hal.h"
#include "chprintf.h"

static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

#define BUFFER_SIZE 10
static int buffer[BUFFER_SIZE];
static size_t buffer_head = 0;
static size_t buffer_tail = 0;
static size_t buffer_count = 0;

// Таймеры для управления скоростью работы
static systime_t last_producer_time = 0;
static systime_t last_consumer_time = 0;
static size_t consumer_speed = 200;
static size_t produser_speed = 800;
static size_t default_timeout = 10;
static int number_counter = 1;

int main(void) {
    halInit();
    chSysInit();
    sdStart(&SD1, NULL);
    
    chprintf(serial, "\r\n=== Producer-Consumer (Main Loop) ===\r\n");
    chprintf(serial, "Producer: generates every %u ms\r\n", consumer_speed);
    chprintf(serial, "Consumer: processes every %u ms\r\n", produser_speed);
    chprintf(serial, "Buffer size: %d items\r\n\r\n", BUFFER_SIZE);

    while (true) {
        systime_t now = chVTGetSystemTime();
        
        if (now - last_producer_time >= TIME_MS2I(consumer_speed)) {
            if (buffer_count < BUFFER_SIZE) {
                int num = number_counter++;
                buffer[buffer_head] = num;
                buffer_head = (buffer_head + 1) % BUFFER_SIZE;
                buffer_count++;
                
                chprintf(serial, "[PRODUCER] Added: %3d (buffer: %2u/10)\r\n", num, buffer_count);
            } else {
                chprintf(serial, "[PRODUCER] Waiting (buffer full)\r\n");
            }
            last_producer_time = now;
        }
        
        if (now - last_consumer_time >= TIME_MS2I(produser_speed)) {
            if (buffer_count > 0) {
                int num = buffer[buffer_tail];
                buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
                buffer_count--;
                
                chprintf(serial, "[CONSUMER] Processed: %3d (buffer: %2u/10)\r\n", num, buffer_count);
            }
            else
            {
                chprintf(serial, "[CONSUMER] Waiting (buffer empty)\r\n");
            }
            last_consumer_time = now;
        }
        
        chThdSleepMilliseconds(default_timeout);
    }
}