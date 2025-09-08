// main.c
// Health Monitoring System with Tiva C and ESP8266
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Hardware definitions for Tiva C Series (TM4C123GH6PM)
#define SYSCTL_RCGCGPIO_R   (*((volatile uint32_t *)0x400FE608))
#define SYSCTL_RCGCUART_R   (*((volatile uint32_t *)0x400FE618))
#define GPIO_PORTA_DATA_R   (*((volatile uint32_t *)0x400043FC))
#define GPIO_PORTA_AFSEL_R  (*((volatile uint32_t *)0x40004420))
#define GPIO_PORTA_PCTL_R   (*((volatile uint32_t *)0x4000452C))
#define GPIO_PORTA_DEN_R    (*((volatile uint32_t *)0x4000451C))
#define GPIO_PORTA_AMSEL_R  (*((volatile uint32_t *)0x40004528))
#define GPIO_PORTF_DATA_R   (*((volatile uint32_t *)0x400253FC))
#define GPIO_PORTF_DIR_R    (*((volatile uint32_t *)0x40025400))
#define GPIO_PORTF_DEN_R    (*((volatile uint32_t *)0x4002551C))
#define GPIO_PORTF_LOCK_R   (*((volatile uint32_t *)0x40025520))
#define GPIO_PORTF_CR_R     (*((volatile uint32_t *)0x40025524))
	#define GPIO_PORTB_AFSEL_R  (*((volatile int *) 0x40005420))

// GPIO Port E definitions (for PE3 as analog input AIN0)
#define GPIO_PORTE_AFSEL_R   (*((volatile uint32_t *)0x40024420))
#define GPIO_PORTE_DEN_R     (*((volatile uint32_t *)0x4002451C))
#define GPIO_PORTE_AMSEL_R   (*((volatile uint32_t *)0x40024528))

// ADC0 EMUX register (ADC Event Multiplexer Select)
#define ADC0_EMUX_R          (*((volatile uint32_t *)0x40038014))

// UART definitions
#define UART0_DR_R          (*((volatile uint32_t *)0x4000C000))
#define UART0_FR_R          (*((volatile uint32_t *)0x4000C018))
#define UART0_IBRD_R        (*((volatile uint32_t *)0x4000C024))
#define UART0_FBRD_R        (*((volatile uint32_t *)0x4000C028))
#define UART0_LCRH_R        (*((volatile uint32_t *)0x4000C02C))
#define UART0_CTL_R         (*((volatile uint32_t *)0x4000C030))

// ADC definitions
#define SYSCTL_RCGCADC_R    (*((volatile uint32_t *)0x400FE638))
#define ADC0_ACTSS_R        (*((volatile uint32_t *)0x40038000))
#define ADC0_RIS_R          (*((volatile uint32_t *)0x40038004))
#define ADC0_IM_R           (*((volatile uint32_t *)0x40038008))
#define ADC0_ISC_R          (*((volatile uint32_t *)0x4003800C))
#define ADC0_SSPRI_R        (*((volatile uint32_t *)0x40038020))
#define ADC0_PSSI_R         (*((volatile uint32_t *)0x40038028))
#define ADC0_SSMUX3_R       (*((volatile uint32_t *)0x400380A0))
#define ADC0_SSCTL3_R       (*((volatile uint32_t *)0x400380A4))
#define ADC0_SSFIFO3_R      (*((volatile uint32_t *)0x400380A8))

// Global variables and RTOS handles
QueueHandle_t xSensorDataQueue;
QueueHandle_t xCommQueue;
SemaphoreHandle_t xI2CSemaphore;
TaskHandle_t xTempTaskHandle, xHeartRateTaskHandle, xCommTaskHandle, xDisplayTaskHandle;

// Health data structure
typedef struct {
    float temperature;
    uint16_t heartRate;
    uint32_t timestamp;
} HealthData_t;

// UART Send Character
void UART_SendChar(char data) {
    while(UART0_FR_R & 0x20); // Wait until TX buffer is not full
    UART0_DR_R = data;
}

// UART Send String
void UART_SendString(const char *str) {
    while(*str) {
        UART_SendChar(*str++);
    }
}

// UART Initialization
void UART_Init(void) {
    // Enable clocks
    SYSCTL_RCGCGPIO_R |= 0x01; // Enable clock to GPIOA (UART0)
    SYSCTL_RCGCUART_R |= 0x01; // Enable clock to UART0
    while((SYSCTL_RCGCGPIO_R & 0x01) == 0); // Wait for clock
    
    // Configure UART0 pins (PA0 and PA1)
    GPIO_PORTA_AFSEL_R |= 0x03; // Enable alternate function
    GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R & 0xFFFFFF00) | 0x00000011;
    GPIO_PORTA_DEN_R |= 0x03;   // Digital enable
    GPIO_PORTA_AMSEL_R &= ~0x03; // Disable analog
    
    // Configure UART
    UART0_CTL_R &= ~0x01;       // Disable UART
    UART0_IBRD_R = 104;         // 16MHz / (16 * 9600) = 104.1667
    UART0_FBRD_R = 11;          // 0.1667 * 64 + 0.5 = 11.1668
    UART0_LCRH_R = 0x60;        // 8N1
    UART0_CTL_R |= 0x01;        // Enable UART
}

// GPIO Initialization
void GPIO_Init(void) {
    // Enable clock to PORTF
    SYSCTL_RCGCGPIO_R |= 0x20;
    while((SYSCTL_RCGCGPIO_R & 0x20) == 0);
    
    // Unlock and configure PORTF
    GPIO_PORTF_LOCK_R = 0x4C4F434B;
    GPIO_PORTF_CR_R = 0x1F;
    GPIO_PORTF_DIR_R = 0x0E;    // PF1-3 as output
    GPIO_PORTF_DEN_R = 0x1F;    // Digital enable PF0-4
}

// ADC Initialization
void ADC_Init(void) {
    // Enable clocks
    SYSCTL_RCGCGPIO_R |= (1<<4); // Port E
    SYSCTL_RCGCADC_R |= (1<<0);  // ADC0
    
    // Configure PE3 for AIN0
    GPIO_PORTE_AFSEL_R |= (1<<3);
    GPIO_PORTE_DEN_R &= ~(1<<3);
    GPIO_PORTE_AMSEL_R |= (1<<3);
    
    // Initialize ADC0 sequencer 3
    ADC0_ACTSS_R &= ~(1<<3);    // Disable SS3
    ADC0_EMUX_R &= ~0xF000;     // Software trigger
    ADC0_SSMUX3_R = 0;          // Channel 0
    ADC0_SSCTL3_R |= (1<<1)|(1<<2); // One sample, set flag
    ADC0_ACTSS_R |= (1<<3);     // Enable SS3
}

// Read ADC value
uint32_t ADC_Read(void) {
    ADC0_PSSI_R |= (1<<3);      // Start conversion
    while((ADC0_RIS_R & 8) == 0); // Wait for completion
    uint32_t value = ADC0_SSFIFO3_R; // Read result
    ADC0_ISC_R = 8;             // Clear flag
    return value;
}

// Temperature Sensor Task
void vTemperatureTask(void *pvParameters) {
    HealthData_t healthData;
    
    while(1) {
        uint32_t adcValue = ADC_Read();
        healthData.temperature = (adcValue * 3.3f / 4095) * 100; // Convert to Celsius
        healthData.timestamp = xTaskGetTickCount();
        
        xQueueSend(xSensorDataQueue, &healthData, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Heart Rate Sensor Task
void vHeartRateTask(void *pvParameters) {
    HealthData_t healthData;
    uint32_t pulseCount = 0;
    TickType_t lastTime = xTaskGetTickCount();
    
    while(1) {
        // Simulate pulse detection (use interrupts in real implementation)
        if(GPIO_PORTF_DATA_R & 0x10) { // Using PF4 as fake input
            pulseCount++;
        }
        
        if((xTaskGetTickCount() - lastTime) >= pdMS_TO_TICKS(15000)) {
            healthData.heartRate = (pulseCount * 4); // Convert to BPM
            pulseCount = 0;
            lastTime = xTaskGetTickCount();
            xQueueSend(xSensorDataQueue, &healthData, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Communication Task
void vCommunicationTask(void *pvParameters) {
    HealthData_t healthData;
    char jsonBuffer[128];
    
    while(1) {
        if(xQueueReceive(xSensorDataQueue, &healthData, portMAX_DELAY) == pdPASS) {
            snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"temp\":%.1f,\"hr\":%d,\"time\":%u}",
                healthData.temperature,
                healthData.heartRate,
                (unsigned int)healthData.timestamp);
            
            UART_SendString(jsonBuffer);
            UART_SendString("\n");
            
            xQueueSend(xCommQueue, &healthData, portMAX_DELAY);
            
            // Alert for abnormal values
            if(healthData.temperature > 38.0f || healthData.heartRate > 100) {
                GPIO_PORTF_DATA_R ^= 0x02; // Toggle red LED
            }
        }
    }
}

// Display Task
void vDisplayTask(void *pvParameters) {
    HealthData_t healthData;
    char displayBuffer[64];
    
    while(1) {
        if(xQueueReceive(xCommQueue, &healthData, portMAX_DELAY) == pdPASS) {
            snprintf(displayBuffer, sizeof(displayBuffer),
                "Temp: %.1fC, HR: %d BPM\n",
                healthData.temperature,
                healthData.heartRate);
            UART_SendString(displayBuffer);
        }
    }
}

// ESP8266 Initialization
void ESP8266_Init(void) {
    UART_SendString("AT+RST\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    UART_SendString("AT+CWMODE=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    UART_SendString("AT+CWJAP=\"SSID\",\"PASSWORD\"\r\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
}

int main(void) {
    // Hardware initialization
    GPIO_Init();
    UART_Init();
    ADC_Init();
    
    // Initialize ESP8266
    ESP8266_Init();
    
    // Create RTOS objects
    xSensorDataQueue = xQueueCreate(10, sizeof(HealthData_t));
    xCommQueue = xQueueCreate(10, sizeof(HealthData_t));
    xI2CSemaphore = xSemaphoreCreateMutex();
    
    // Create tasks
    xTaskCreate(vTemperatureTask, "TempSensor", configMINIMAL_STACK_SIZE, NULL, 2, &xTempTaskHandle);
    xTaskCreate(vHeartRateTask, "HeartRate", configMINIMAL_STACK_SIZE, NULL, 2, &xHeartRateTaskHandle);
    xTaskCreate(vCommunicationTask, "CommTask", configMINIMAL_STACK_SIZE*2, NULL, 3, &xCommTaskHandle);
    xTaskCreate(vDisplayTask, "Display", configMINIMAL_STACK_SIZE, NULL, 1, &xDisplayTaskHandle);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
    return 0;
}