/*
 * Actuador con E/S
 *
 * Actuador remoto
 * 
 * Reles de conexion y entradas digitales
 * 
 * Servicio web levantado en puerto PUERTO_WEBSERVER
 */
 
/***************************** Defines *****************************/
//Defines generales
#define NOMBRE_FAMILIA   "OTA Loader"
#define VERSION          "0.0.6"
#define SEPARADOR        '|'
#define SUBSEPARADOR     '#'
#define KO               -1
#define OK                0
#define MAX_VUELTAS      UINT16_MAX// 32767 

//Ficheros de configuracion
#define GLOBAL_CONFIG_FILE               "/Config.json"
#define GLOBAL_CONFIG_BAK_FILE           "/Config.json.bak"
#define WIFI_CONFIG_FILE                 "/WiFiConfig.json"
#define WIFI_CONFIG_BAK_FILE             "/WiFiConfig.json.bak"

// Una vuela de loop son ANCHO_INTERVALO segundos 
#define ANCHO_INTERVALO         100 //Ancho en milisegundos de la rodaja de tiempo
#define FRECUENCIA_OTA            5 //cada cuantas vueltas de loop atiende las acciones
#define FRECUENCIA_SERVIDOR_WEB   1 //cada cuantas vueltas de loop atiende el servidor web

//configuracion del watchdog del sistema
#define TIMER_WATCHDOG        0 //Utilizo el timer 0 para el watchdog
#define PREESCALADO_WATCHDOG 80 //el relog es de 80Mhz, lo preesalo entre 80 para pasarlo a 1Mhz
#define TIEMPO_WATCHDOG      1000*1000*ANCHO_INTERVALO //Si en N ANCHO_INTERVALO no se atiende el watchdog, saltara. ESTA EN microsegundos

#define LED_BUILTIN                2 //GPIO del led de la placa en los ESP32   
/***************************** Defines *****************************/

/***************************** Includes *****************************/
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <rom/rtc.h> //<esp32/rom/rtc.h>
/***************************** Includes *****************************/

/***************************** variables globales *****************************/
//Indica si el rele se activa con HIGH o LOW
hw_timer_t *timer = NULL;//Puntero al timer del watchdog
String nombre_dispositivo;//(NOMBRE_FAMILIA);//Nombre del dispositivo, por defecto el de la familia
uint16_t vuelta = MAX_VUELTAS-100;//0; //vueltas de loop
int debugGlobal=0; //por defecto desabilitado
/***************************** variables globales *****************************/

/************************* FUNCIONES PARA EL BUITIN LED ***************************/
void configuraLed(void){pinMode(LED_BUILTIN, OUTPUT);}
void enciendeLed(void){digitalWrite(LED_BUILTIN, HIGH);}
void apagaLed(void){digitalWrite(LED_BUILTIN, LOW);}
void parpadeaLed(uint8_t veces, uint16_t espera=100)
  {
  for(uint8_t i=0;i<2*veces;i++)
    {
    delay(espera/2);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
/***********************************************************************************/  

/*************************************** SETUP ***************************************/
void setup()
  {
  Serial.begin(115200);
  configuraLed();
  enciendeLed();
  
  Serial.printf("\n\n\n");
  Serial.printf("*************** %s ***************\n",NOMBRE_FAMILIA);
  Serial.printf("*************** %s ***************\n",VERSION);
  Serial.println("***************************************************************");
  Serial.println("*                                                             *");
  Serial.println("*             Inicio del setup del modulo                     *");
  Serial.println("*                                                             *");    
  Serial.println("***************************************************************");

  Serial.printf("\n\nInit Ficheros ---------------------------------------------------------------------\n");
  //Ficheros - Lo primero para poder leer los demas ficheros de configuracion
  inicializaFicheros(debugGlobal);

  //Configuracion general
  Serial.printf("\n\nInit General ---------------------------------------------------------------------\n");
  inicializaConfiguracion(debugGlobal);
  parpadeaLed(1);
  
  //Wifi
  Serial.println("\n\nInit WiFi -----------------------------------------------------------------------\n");
  if (inicializaWifi(1))//debugGlobal)) No tiene sentido debugGlobal, no hay manera de activarlo
    {
    parpadeaLed(5,200);
    /*----------------Inicializaciones que necesitan red-------------*/
    //OTA
    Serial.println("\n\nInit OTA ------------------------------------------------------------------------\n");
    iniializaOTA(debugGlobal);
    parpadeaLed(1);
    //WebServer
    Serial.println("\n\nInit Web ------------------------------------------------------------------------\n");
    inicializaWebServer();
    parpadeaLed(2);
    //Websocket
    Serial.println("\n\nInit websocket ---------------------------------------------------------------------\n");  
    WebSocket_init(true);    
    parpadeaLed(3);
    }
  else Serial.println("No se pudo conectar al WiFi");
  
  parpadeaLed(2);
  apagaLed();//Por si acaso...
  
  Serial.printf("\n\n");
  Serial.println("***************************************************************");
  Serial.println("*                                                             *");
  Serial.println("*               Fin del setup del modulo                      *");
  Serial.println("*                                                             *");    
  Serial.println("***************************************************************");
  Serial.printf("\n\n");  

  //activo el watchdog
  configuraWatchdog();    
  }  

void loop()
  {  
  //referencia horaria de entrada en el bucle
  time_t EntradaBucle=0;
  EntradaBucle=millis();//Hora de entrada en la rodaja de tiempo

  //reinicio el watchdog del sistema
  alimentaWatchdog(); // para el esp32 timerWrite(timer, 0);

  //------------- EJECUCION DE TAREAS --------------------------------------
  //Acciones a realizar en el bucle   
  //Prioridad 0: OTA es prioritario.
  if ((vuelta % FRECUENCIA_OTA)==0) gestionaOTA(); //Gestion de actualizacion OTA
  //Prioridad 3: Interfaces externos de consulta    
  if ((vuelta % FRECUENCIA_SERVIDOR_WEB)==0) webServer(debugGlobal); //atiende el servidor web
  if ((vuelta % FRECUENCIA_SERVIDOR_WEB)==0) atiendeWebsocket();
  //if ((vuelta % FRECUENCIA_ORDENES)==0) while(HayOrdenes(debugGlobal)) EjecutaOrdenes(debugGlobal); //Lee ordenes via serie
  //------------- FIN EJECUCION DE TAREAS ---------------------------------  

  //sumo una vuelta de loop, si desborda inicializo vueltas a cero
  vuelta++;//sumo una vuelta de loop  
      
  //Espero hasta el final de la rodaja de tiempo
  while(millis()<EntradaBucle+ANCHO_INTERVALO)
    {
    if(millis()<EntradaBucle) break; //cada 49 dias el contador de millis desborda
    delay(1);
    }
  }

/********************************** Funciones de configuracion global **************************************/
/************************************************/
/* Recupera los datos de configuracion          */
/* del archivo global                           */
/************************************************/
boolean inicializaConfiguracion(boolean debug)
  {
  String cad="";
  if (debug) Serial.println("Recupero configuracion de archivo...");

  //cargo el valores por defecto
  nombre_dispositivo=String(NOMBRE_FAMILIA); //Nombre del dispositivo, por defecto el de la familia
  }

/***************************************AUXILIARES**************************************************/
/****************************ESP32****************************/
/***************************************************************/
/*                                                             */
/*  Funcion de interrupcion del watchdog                       */
/*  reseteara el ESP si no se atiende el watchdog              */
/*                                                             */
/***************************************************************/
void IRAM_ATTR resetModule(void) {
  Serial.printf("Watchdog!!! reboot\n");
  esp_restart();
}

/***************************************************************/
/*                                                             */
/*  Configuracion del watchdog del sistema                     */
/*                                                             */
/***************************************************************/
void configuraWatchdog(void)
  {
  timer = timerBegin(TIMER_WATCHDOG, PREESCALADO_WATCHDOG, true); //timer 0, div 80 para que cuente microsegundos y hacia arriba         //hw_timer_t * timerBegin(uint8_t timer, uint16_t divider, bool countUp);
  timerAttachInterrupt(timer, &resetModule, true);                //asigno la funcion de interrupcion al contador                        //void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);
  timerAlarmWrite(timer, TIEMPO_WATCHDOG, false);                  //configuro el limite del contador para generar interrupcion en us    //void timerAlarmWrite(hw_timer_t *timer, uint64_t interruptAt, bool autoreload);
  timerWrite(timer, 0);                                           //lo pongo a cero                                                      //void timerWrite(hw_timer_t *timer, uint64_t val);
  timerAlarmEnable(timer);                                        //habilito el contador                                                 //void timerAlarmEnable(hw_timer_t *timer);
  }

void alimentaWatchdog(void) {timerWrite(timer, 0);}
  
