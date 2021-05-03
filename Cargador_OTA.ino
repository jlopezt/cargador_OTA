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
#define VERSION          "1.1.0"
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

#define ESP32CAM

#if defined(ESP32CAM)
  #define LED_BUILTIN                 33 //GPIO del led de la placa en los ESP32-CAM   
  //const int LED_BUILTIN=25;
//Para ESP32 y ESP-CAM
/*
  const boolean ENCENDIDO=LOW;  
  const boolean APAGADO=HIGH;
  */
//Para Heltec WiFi LORA 32 V2  
  const boolean ENCENDIDO=HIGH;  
  const boolean APAGADO=LOW;

#else
//  #define LED_BUILTIN                2 //GPIO del led de la placa en los ESP32   
  const int LED_BUILTIN=2;
  const boolean ENCENDIDO=HIGH;
  const boolean APAGADO=LOW;
#endif
/***************************** Defines *****************************/

/***************************** Includes *****************************/
//#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
/***************************** Includes *****************************/

/***************************** variables globales *****************************/
//Indica si el rele se activa con HIGH o LOW
String nombre_dispositivo="Loader";//(NOMBRE_FAMILIA);//Nombre del dispositivo, por defecto el de la familia
uint16_t vuelta = MAX_VUELTAS-100;//0; //vueltas de loop
int debugGlobal=0; //por defecto desabilitado
/***************************** variables globales *****************************/

/************************* FUNCIONES PARA EL BUITIN LED ***************************/
void configuraLed(void){pinMode(LED_BUILTIN, OUTPUT);}
void enciendeLed(void){digitalWrite(LED_BUILTIN, ENCENDIDO);}
void apagaLed(void){digitalWrite(LED_BUILTIN, APAGADO);}
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
  Serial.printf("*************** %s/%s ***************\n",NOMBRE_FAMILIA,VERSION);
  Serial.println("***************************************************************");
  Serial.println("*             Inicio del setup del modulo                     *");
  Serial.println("***************************************************************");

  Serial.printf("\n\nInit Ficheros ---------------------------------------------------------------------\n");
  //Ficheros - Lo primero para poder leer los demas ficheros de configuracion
  inicializaFicheros(debugGlobal);

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
    }
  else Serial.println("No se pudo conectar al WiFi");
  
  parpadeaLed(2);
  apagaLed();//Por si acaso...

  int *p=NULL;
  p=(int *)ps_malloc(240*320*3);
  if(p==NULL) Serial.println("malloc KO");
  else Serial.println("malloc OK");
  
  Serial.printf("\n\n");
  Serial.println("***************************************************************");
  Serial.println("*               Fin del setup del modulo                      *");
  Serial.println("***************************************************************");
  Serial.printf("\n\n");  
  }  

void loop()
  {  
  //referencia horaria de entrada en el bucle
  time_t EntradaBucle=0;
  EntradaBucle=millis();//Hora de entrada en la rodaja de tiempo

  //------------- EJECUCION DE TAREAS --------------------------------------
  //Acciones a realizar en el bucle   
  //Prioridad 0: OTA es prioritario.
  if ((vuelta % FRECUENCIA_OTA)==0) gestionaOTA(); //Gestion de actualizacion OTA
  //Prioridad 3: Interfaces externos de consulta    
  if ((vuelta % FRECUENCIA_SERVIDOR_WEB)==0) webServer(debugGlobal); //atiende el servidor web
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
