#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "chmtx.h"
#include "chevents.h"

static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

#define BUFFER_SIZE 10
static int buffer[BUFFER_SIZE];
static size_t buffer_head = 0;
static size_t buffer_tail = 0;
static size_t buffer_count = 0;
static size_t consumer_speed = 200;
static size_t produser_speed = 800;
static mutex_t buffer_mutex;
static mutex_t print_mutex;  // Отдельный мьютекс для вывода
static event_source_t buffer_event;

static int number_counter = 1;

static THD_WORKING_AREA(waProducer, 256);
static THD_FUNCTION(Producer, arg) {
    (void)arg;
    
    while (true) {
        int num = number_counter++;
        
        chMtxLock(&buffer_mutex);
        while (buffer_count == BUFFER_SIZE) {
            chMtxUnlock(&buffer_mutex);
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[PRODUCER] Waiting (buffer full)\r\n");
            chMtxUnlock(&print_mutex);
            
            chThdSleepMilliseconds(produser_speed);
            chMtxLock(&buffer_mutex);
        }
        
        buffer[buffer_head] = num;
        buffer_head = (buffer_head + 1) % BUFFER_SIZE;
        buffer_count++;
        
        chMtxLock(&print_mutex);
        chprintf(serial, "[PRODUCER] Added: %3d (buffer: %2u/10)\r\n", num, buffer_count);
        chMtxUnlock(&print_mutex);
        
        chEvtBroadcast(&buffer_event);
        chMtxUnlock(&buffer_mutex);
        
        chThdSleepMilliseconds(produser_speed);
    }
}

static THD_WORKING_AREA(waConsumer, 256);
static THD_FUNCTION(Consumer, arg) {
    (void)arg;
    event_listener_t el;
    chEvtRegister(&buffer_event, &el, 0);
    
    while (true) {
        chEvtWaitAny(EVENT_MASK(0));
        
        chMtxLock(&buffer_mutex);
        if (buffer_count > 0) {
            int num = buffer[buffer_tail];
            buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
            buffer_count--;
            
            chMtxLock(&print_mutex);
            chprintf(serial, "[CONSUMER] Processed: %3d (buffer: %2u/10)\r\n", num, buffer_count);
            chMtxUnlock(&print_mutex);
        }
        chMtxUnlock(&buffer_mutex);
        
        chThdSleepMilliseconds(consumer_speed);
    }
}

int main(void) {
    halInit();
    chSysInit();
    sdStart(&SD1, NULL);
    
    chMtxObjectInit(&buffer_mutex);
    chMtxObjectInit(&print_mutex);
    chEvtObjectInit(&buffer_event);
    
    chThdCreateStatic(waProducer, sizeof(waProducer), NORMALPRIO, Producer, NULL);
    chThdCreateStatic(waConsumer, sizeof(waConsumer), NORMALPRIO, Consumer, NULL);

    chMtxLock(&print_mutex);
    chprintf(serial, "\r\n=== Producer-Consumer Demo ===\r\n");
    chprintf(serial, "Producer: generates every %u ms\r\n", produser_speed);
    chprintf(serial, "Consumer: processes every %u ms\r\n", consumer_speed);
    chprintf(serial, "Buffer size: %d items\r\n\r\n", BUFFER_SIZE);
    chMtxUnlock(&print_mutex);

    while (true) {
        chThdSleepMilliseconds(1000);
    }
}