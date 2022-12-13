
#include <Arduino_FreeRTOS.h>
#include <DHT.h>
#include <queue.h>
#include <semphr.h>
#include <Arduino.h>
#define DHTTYPE DHT11
#define DHT11_PIN 2
#include<LiquidCrystal.h> // include LCD library function


LiquidCrystal lcd (5,13,11,10,9,8); //RS, E, D4, D5, D6, D7
//define task handles
TaskHandle_t TaskTemp_Handler;
TaskHandle_t TaskLDR_Handler;
TaskHandle_t TaskLCD_Handler;
TaskHandle_t TaskControlUmidSolo_Handler; 
TaskHandle_t taskNotificationHandler = NULL;

int portLDR = A3;
int portSun = 3; // D3
int timeSunON = 2000; //ms
int timeSunOFF = 1000; //ms
int thresholdLight = 500;
bool sunPeriod = false;

int portHeater = 6; //D6
int portCooler = 7; // D7
float temperature = 0;
float supThresholdTemp = 31; // °C
float infThresholdTemp = 23; // °

int portUmidSoloDig = 4; //D4
int portUmidSoloAna = A2;
int portValvula = 12; //D12
float thresholdUmid = 600;

// define two tasks for Blink & Serial
void TaskTemp( void *pvParameters );
void TaskLCD(void* pvParameters);
void TriggerLuminosidade (void *pvParameters);
void ControleLuminosidade (void *pvParameters);
void ControleUmidSolo (void *pvParameters);
//void ControleUmidAr (void *pvParameters);

String myItem1 ;
//char *myItem1 = malloc(strlen(myData1)+1); 

//char *myItem2 = malloc(strlen(myData2)+1); 
/* filas (queues) */
QueueHandle_t xQueue_LCD_temp;
SemaphoreHandle_t xSemaphore;//Objeto do semaforo

//For DH11 Sensor
DHT dht(DHT11_PIN, DHTTYPE);

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  lcd.begin(16,2);
  dht.comeco();
    xQueue_LCD_temp = xQueueCreate(1, sizeof( char* ) );
  //  xQueue_LCD_umid = xQueueCreate(1, sizeof( float ) );
    /* Criação dos semaforos */
    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL)
    {
        Serial.println("Erro: nao e possivel criar o semaforo");
        while(1); /* Sem semaforo o funcionamento esta comprometido. Nada mais deve ser feito. */
    }
  lcd.createChar(0,grau);
   // initialize digital pin LED_BUILTIN as an output.
  pinMode(portLDR, INPUT);
  pinMode(DHT11_PIN,INPUT);
  pinMode(portSun, OUTPUT);
  pinMode(portUmidSoloDig, INPUT);

  pinMode(portHeater, OUTPUT);
  pinMode(portCooler, OUTPUT);
  pinMode(portValvula, OUTPUT);

  
  digitalWrite(portSun, LOW);
  digitalWrite(portCooler, HIGH);
  digitalWrite(portHeater, LOW);
  digitalWrite(portValvula, LOW);

  // Now set up five tasks to run independently.
   xTaskCreate(TaskLCD, "LCD",128 , NULL, 3,&TaskLCD_Handler );
   xTaskCreate(TriggerLuminosidade, "Trigger de Luminosidade",128, NULL, 3,&TaskLDR_Handler );
   xTaskCreate(ControleLuminosidade, "Controle de Luminosidade", 128, NULL, 3,  &taskNotificationHandler);
   xTaskCreate( TaskTemp, "Controle de Temperatura ", 128, NULL, 5,&TaskTemp_Handler);
   xTaskCreate( ControleUmidSolo, "Controle de Umidade do Solo ", 128, NULL, 5, &TaskControlUmidSolo_Handler);
      
}
    

void loop()
{
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/


void TaskTemp (void *pvParameters){
  (void) pvParameters;
  TickType_t proxTime;
  proxTime = xTaskGetTickCount();
  //
  while(1){   
    //Tarefe periódica com período de 500ms 
    vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS));
    if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {  //Solicita Mutex 
       float temperature = dht.ler_Temperatura();
       String enviar = String("Temp: ");
       String value = String(temperature);
       enviar.concat(value);
       xQueueSend(xQueue_LCD_temp, &enviar, portMAX_DELAY);
       xSemaphoreGive(xSemaphore); // Libera o mutex
    if (temperature >= supThresholdTemp){
      digitalWrite(portCooler, LOW);
    }else{
      digitalWrite(portCooler, HIGH);
    }

    if (temperature <= infThresholdTemp){
      digitalWrite(portHeater, HIGH);
    }else{
      digitalWrite(portHeater, LOW);
    }
    //Tarefe periódica com período de 500ms 
    vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS));  
    
  }
  }
}

void TaskLCD(void *pvParameters){
    (void) pvParameters;
      TickType_t proxTime;
      proxTime = xTaskGetTickCount();
      char *myReceivedItem1;
      char *myReceivedItem2;
  for (;;) // A Task shall never return or exit.
  {
        //Tarefe periódica com período de 100ms 
        vTaskDelayUntil(&proxTime, (100/portTICK_PERIOD_MS));
        xQueueReceive( xQueue_LCD_temp, &myReceivedItem1, portMAX_DELAY);
        xQueueReceive( xQueue_LCD_temp, &myReceivedItem2, portMAX_DELAY);
      //  xQueueReceive( xQueue_LCD_temp, &myReceivedItem3, portMAX_DELAY);
        lcd.clear();
        lcd.setCursor(0,0); // linha 0 coluna 0
        lcd.print(myReceivedItem1);
        lcd.setCursor(0,1); // linha 1 coluna 0
        lcd.print(myReceivedItem2);
        
  }
}

void TriggerLuminosidade (void *pvParameters){
     (void) pvParameters;
      TickType_t proxTime;
      proxTime = xTaskGetTickCount(); 
  for(;;){
     xTaskNotifyGive(taskNotificationHandler); // gera notificações periodicamente
     vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS));
    if (sunPeriod){
      if (analogRead(portLDR) < thresholdLight){ //baixa luminosidade
        digitalWrite(portSun, HIGH);
      }else{
        digitalWrite(portSun, LOW);
      }
    }else{
      digitalWrite(portSun, LOW);
    }
    vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS));
  }
}

void ControleLuminosidade (void *pvParameters){
  (void) pvParameters;
  TickType_t proxTime;
  proxTime = xTaskGetTickCount();
  
  for(;;){       
     if(ulTaskNotifyTake(pdTRUE,(TickType_t)portMAX_DELAY) > 0){ // verifica se alguma notificação foi gerada
         sunPeriod = true;
         vTaskDelayUntil(&proxTime, (timeSunON/portTICK_PERIOD_MS));
         sunPeriod = false;
         vTaskDelayUntil(&proxTime, (timeSunOFF/portTICK_PERIOD_MS));
      } 
  }  
}

void ControleUmidSolo (void *pvParameters){
  (void) pvParameters;
  TickType_t proxTime;
  proxTime = xTaskGetTickCount();
  float umidity=0.0;
 
  while(1){   
 
      vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS)); 
      umidity = analogRead(portUmidSoloAna);
    if(umidity < thresholdUmid){
      digitalWrite(portValvula, HIGH);
      myItem1 = "UMID SOLO:ALTA  "; 
    }else{
      digitalWrite(portValvula, LOW);
      myItem1 = "UMID SOLO:BAIXA ";
    }
   
    if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {  //Solicita Mutex    
       xQueueSend(xQueue_LCD_temp, &myItem1, portMAX_DELAY);
       xSemaphoreGive(xSemaphore);                            //Libera Mutex                       
   }
   vTaskDelayUntil(&proxTime, (500/portTICK_PERIOD_MS)); 
  }  
}    
