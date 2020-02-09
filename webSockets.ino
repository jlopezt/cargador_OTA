/**********************************************/
/*                                            */
/*Funciones auxiliares y gestion de Websockets*/
/*                                            */
/**********************************************/
#define PUERTO_WEBSOCKET  88
#define NOT_CONNECTED     -1

//websockets
typedef struct
  {
  int8_t id=NOT_CONNECTED;//No hay cliuente conectado
  IPAddress IP={0,0,0,0};
  }cliente_t;
cliente_t cliente;

WebSocketsServer webSocket = WebSocketsServer(PUERTO_WEBSOCKET);

/**********************************************/
/*                                            */
/*   Trasnforma un mensaje hexadecimal en     */
/*   texto imprimible en pantalla             */
/*                                            */
/**********************************************/
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) 
  {
  const uint8_t* src = (const uint8_t*) mem;
  Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for(uint32_t i = 0; i < len; i++) 
    {
    if(i % cols == 0) 
      {
      Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
      }
    Serial.printf("%02X ", *src);
    src++;
    }
  Serial.printf("\n");
  }
  
/**********************************************/
/*                                            */
/*   Gestiona los mensajes recibidos por WS   */
/*                                            */
/**********************************************/
void WebSocket_init(boolean debug)
  {
  //Inicializo la estructura de cliente
  cliente.id=NOT_CONNECTED;
  cliente.IP={0,0,0,0};

  //*******Configuracion del WS server***********
  if (debug) Serial.printf("Iniciamos el servidor de websockets\n");
  webSocket.begin();
  if (debug) Serial.printf("Asignado gestor de mensajes\n");
  webSocket.onEvent(webSocketEvent);
  if (debug) Serial.printf("Finalizado\n");  
  }

void atiendeWebsocket(void)
  {
  webSocket.loop();  
  }

void gestionaMensajes(uint8_t cliente, String mensaje) //Tiene que implementar la maquina equivalente a la del loop de ESP-WHO. En funcion de g_state
  {
  Serial.printf("Procesando mensaje %s de %i\n",mensaje.c_str(),cliente);
	if(mensaje== "OTA") 
		{
 	  }

	if(mensaje== "Particiones") 
		{
    String cad="particiones";
    cad += SEPARADOR;
    cad +=leeParticiones();
    Serial.printf("Particiones leidas: \n%s", cad.c_str());
    webSocket.sendTXT(cliente, cad);
 	  }
  }

void webSocketEvent(uint8_t clienteId, WStype_t type, uint8_t * payload, size_t length) 
  {
  switch(type) 
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Desconectado\n", clienteId);
        cliente.id=NOT_CONNECTED;//Se ha Desconectado
        cliente.IP={0,0,0,0};
        break;
    case WStype_CONNECTED:
        {
        IPAddress ip = webSocket.remoteIP(clienteId);
        Serial.printf("[%u] Connectado desde IP %d.%d.%d.%d url: %s\n", clienteId, ip[0], ip[1], ip[2], ip[3], payload);
        //Nuevo cliente conectado
        cliente.id=clienteId;
        cliente.IP=ip;
        }
        break;
    case WStype_TEXT:
        {
        String datos="";
        for(uint8_t i=0;i<length;i++) datos+=(char)payload[i];

        Serial.printf("[%u] recibido el texto: %s | datos: %s\n", clienteId, payload,datos.c_str());
        gestionaMensajes(clienteId,datos);
        }
        break;
    case WStype_BIN:
        Serial.printf("[%u] recibidos datos binarios length: %u\n", clienteId, length);
        hexdump(payload, length);

        break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
    case WStype_PING:
    case WStype_PONG:
			break;
    }
  }

boolean enviarWSTXT(String mensaje)
  {
  boolean salida=false;  

  if(cliente.id!=NOT_CONNECTED) salida=webSocket.sendTXT(cliente.id, (const uint8_t *)mensaje.c_str());

  if (debugGlobal)
    {
    if (salida ) Serial.println(mensaje);
    else Serial.printf("Error en el envio de [%s] | id de cliente: %i | IP de cliente: %s\n",mensaje.c_str(),cliente.id,cliente.IP.toString().c_str());
    }

  return salida; 
  }
