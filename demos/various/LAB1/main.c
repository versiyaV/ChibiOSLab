#include "ch.h"
#include "hal.h"
#include "chprintf.h"

// Виртуальный UART (QEMU выводит его в терминал)
static BaseSequentialStream *serial = (BaseSequentialStream*) &SD1;

// Поток, который печатает сообщение
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
    (void)arg;
    while (true) {
        chprintf(serial, "Hello world!\r\n");
        chThdSleepMilliseconds(1000);
    }
}

int main(void) {
    halInit();      // Инициализация HAL
    chSysInit();    // Инициализация ядра

    // Включаем UART (по умолчанию USART1)
    sdStart(&SD1, NULL);

    // Запускаем поток
    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

    while (true) {
        chThdSleepMilliseconds(1000);
    }
}