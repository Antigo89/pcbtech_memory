/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

Задание
Реализовать систему с динамическим выделением памяти по схеме heap_4. Основные условия:
1. Система создаётся с единственной задачей 1 (приоритет 1), размер стека
configMINIMAL_STACK_SIZE (по умолчанию 130 слов).
2. Задача 1 выполняет запрос свободного места в куче и выводит эту информацию через UART.
После этого она создаёт задачу 2 (приоритет 1, размер стека configMINIMAL_STACK_SIZE) и
переходит в блокированное состояние на 3 с (через vTaskDelay).

3. Задача 2 выполняет запрос свободного места в куче и выводит эту информацию через UART.

Далее, в ней происходит создание динамического массива размером 20 ячеек (тип данных uint16_t)
и вывод по UART информации о текущем состоянии кучи. Затем динамический массив
уничтожается, в этой задаче снова происходит опрос свободного места в куче и вывод
информации по UART. После этого задача 2 уничтожает сама себя.
4. Внутри файла «main.c» в виде блока комментариев привести описание и сравнение состояния кучи
на каждом шаге, где читается её состояние. Необходимо вычислить и объяснить количество
затраченной памяти (подробно на какие нужды и сколько памяти выделяется):
 - при создании задачи 1;
 - при создании задачи 2;
 - при создании динамического массива.
5. Выполнить сборку, запуск и тестирование проекта на отладочной плате.
Критерии готовности (Definition of Done)
1. Проект собирается и загружается в отладочную плату без ошибок и предупреждений.
2. По последовательному порту передаются сообщения от двух задач, в сообщениях есть
информация о том после какого события было получено состояние кучи.
3. Внутри файла «main.c» присутствует блок комментариев, в котором приведены корректные
расчёты с объяснением затраченной памяти.


USING STM32F407VET6 board:

SWD interface
UART1 (PA9-TX, PA10-RX)

Выделение памяти:
Total SRAM: 196608 byte
Total heap: 76800 byte

Heap size create task 1: 76792 
(-8 байта)  -8 байт заголовок на организацию нового выделения памяти (BlockLink_t), стэк и TCB статической задачи выделен в SRAM вне области heap с фиксированными адресами памяти
дополнительного выделения для выравнивания стэка или TCB не происходит, так как не вызывается функция pvMalloc()

Heap size create task 2: 76168 
(-624 байта) 
-520 байт на 130 слов по 4 байта стэк, 
-8 байт BlockLink_t стэка, -8 на выравнивание по формуле: "размер выравнивания" - ("запрашиваемый размер" & "Маска 0x0007") = 8 - (520 & 0x0007)
-72 на TaskControlBlock (TCB: приоритеты, адрес вершины стэка и прочее),
-8 байт BlockLink_t TCB, -8 на выравнивание по формуле: "размер выравнивания" - ("запрашиваемый размер" & "Маска 0x0007") = 8 - (72 & 0x0007)

Heap size create array: 76112 
(-56 байта) -40 байт на 20 двухбайтовых ячеек массива,
-8 байт BlockLink_t,  -8 на выравнивание по формуле: "размер выравнивания" - ("запрашиваемый размер" & "Маска 0x0007") = 8 - (40 & 0x0007)

Heap size clear array: 76168 
(+56 байта) освобождение ранее выделенной памяти под массив

*/
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usart_dbg.h"

#include <stdio.h>
#include <stdlib.h>
/************************global values********************************/
StaticTask_t xTask1ControlBlock;
StackType_t xTask1Stack[configMINIMAL_STACK_SIZE];
#define ARRAY_SIZE  (20)
uint16_t *pcArray;

/****************************func************************************/
void vTask2(void * pvParameters){
  while(1){
    size_t heapSizeInfo = xPortGetFreeHeapSize();
    printf("Heap size create task 2: %d\n",heapSizeInfo);
    pcArray = (uint16_t *) pvPortMalloc(ARRAY_SIZE * sizeof(uint16_t));
    for(uint8_t i; i<ARRAY_SIZE; i++){
      pcArray[i] = i*i;
    }
    heapSizeInfo = xPortGetFreeHeapSize();
    printf("Heap size create array: %d\n",heapSizeInfo);
    vPortFree(pcArray);
    heapSizeInfo = xPortGetFreeHeapSize();
    printf("Heap size clear array: %d\n",heapSizeInfo);
    vTaskDelete(NULL);
  }
  vTaskDelete(NULL);
}

void vTask1Static(void * pvParameters){
  while(1){
    size_t heapSizeInfo = xPortGetFreeHeapSize();
    printf("Heap size create task 1: %d\n",heapSizeInfo);
    xTaskCreate(vTask2, "vTask2", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskDelay(3000);
  }
  vTaskDelete(NULL);
}
/****************************main************************************/

int main(void) {
  usart1_init();
  printf("--- System startup ---\n");

  xTaskCreateStatic(vTask1Static, "vTask1", configMINIMAL_STACK_SIZE, NULL, 1, xTask1Stack, &xTask1ControlBlock);

  vTaskStartScheduler();

  while(1){}
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
   implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
   used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    /* If the buffers to be provided to the Idle task are declared inside this
       function then they must be declared static - otherwise they will be allocated on
       the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
       state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
       Note that, as the array is necessarily of type StackType_t,
       configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/*************************** End of file ****************************/
