#include <ESP8266WiFi.h>
#include <PubSubClient.h>

                                                                  //// РЕЖИМ ТОЧКИ ДОСТУПА ////
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <EEPROM.h>

const char* ssid_point = "ESP";                                   // Имя точки доступа
const char* password_point = "23112311";                          // Пароль точки доступа

String ssid_new;
String pass_new;

ESP8266WebServer server(80);

void(* resetFunc) (void) = 0;                                     // Функция перезагрузки МК

void blink_delay(int t)                                           // Индикация
{
  for (int i = 0; i < t; i++)
  {
    digitalWrite(13, LOW);  
    delay(200);      
    digitalWrite(13, HIGH);  
    delay(200);    
  }
}

void write_string_EEPROM (int addr, String string)                // Запись String в EEPROM
{
  byte lng = string.length();
  EEPROM.begin(128);
  
  if (lng > 31)  
    lng = 0;    
    
  EEPROM.write(addr , lng);    
  unsigned char* buf = new unsigned char[31];
  string.getBytes(buf, lng + 1);
  addr++;
  
  for(byte i = 0; i < lng; i++) 
  {
    EEPROM.write(addr + i, buf[i]); 
    delay(10);
  }
  
  EEPROM.end();
}

char *read_string_EEPROM (int addr)                               // Чтение String из EEPROM
{
  EEPROM.begin(128);
  byte lng = EEPROM.read(addr);
  
  if (lng > 31)
    lng = 0;
  
  char* buf = new char[31];
  addr++;
  
  for(byte i = 0; i < lng; i++) 
    buf[i] = char(EEPROM.read(i + addr));
  
  buf[lng] = '\x0';
  return buf;
}

int read_type(int address)                                        // Чтение Int из EEPROM
{
  EEPROM.begin(128);
  byte value = EEPROM.read(address);
  return value;
}

void write_type(int addr, byte value)                             // Запись Int в EEPROM
{
  EEPROM.begin(128);
  EEPROM.write(addr , value);
  delay(10);
  EEPROM.end();
}

void handleLogin()
{
  String msg;

  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD"))
  {
    ssid_new = server.arg("USERNAME");
    pass_new = server.arg("PASSWORD");
          
    Serial.print("SSID: ");
    Serial.println(ssid_new);
    
    Serial.print("Password: ");
    Serial.println(pass_new);
          
    write_string_EEPROM (0, ssid_new);
    delay(50);
    write_string_EEPROM (32, pass_new);
    
    msg = "   Data received. The microcontroller will be rebooted.";
    
    Serial.println("Data received. The microcontroller will be rebooted.");
    
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
    write_type(100,0);
    resetFunc();
  }
  
  String content = "<html><body><font face='courier'><form action='/login' method='POST'><p align='center'>Enter the name and password of the Wi-Fi</p>";
  content += "<p align='center'>SSID:<input type='text' name='USERNAME' placeholder='SSID'></p>";
  content += "<p align='center'>Pass:<input type='password' name='PASSWORD' placeholder='Password'></p>";
  content += "<p align='center'><input type='submit' name='SUBMIT' value='Submit'></form></p>";
  content += "<br><br><br><br><br> <p align='center'>" + msg + "</p> <br><br><br><br><br><br>";
  content += "<p align='center'><a href='/inline'></a> </p></font> </body></html>";
  
  server.send(200, "text/html", content);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


                                                                  //// ОСНОВНАЯ ПРОГРАММА ////
                                                                  
const char *ssid =  "default";                                    // Имя Wi-Fi точки доступа
const char *pass =  "default";                                    // Пароль от точки доступа

const char *mqtt_server = "m10.cloudmqtt.com";                    // Имя сервера MQTT
const int mqtt_port = 16029;                                      // Порт для подключения к серверу MQTT
const char *mqtt_user = "kzvcomct";                               // Логин от сервера
const char *mqtt_pass = "-mW311FSubSi";                           // Пароль от сервера

int touch = 0;                                                    // Состояние кнопки
int sch = 0;                                                      // Таймер 1 для кнопки
int sch_2 = 0;                                                    // Таймер 2 для кнопки 

bool change_wifi = false;                                         // Режим перезаписи имени/пароля к wi-fi сети

bool wifi_connect_flag = false;


bool state_1;
bool state_2;
bool permit = false;                                              // разрешение на переключение реле кнопкой
bool mqtt_send = false;
bool reboot_flag = false;                                         // при true перезагрузка неизбежна
bool reboot_flag_2 = false; 

#define BUFFER_SIZE 200


void callback(const MQTT::Publish& pub)                           // Функция получения данных от сервера MQTT
{
  String payload = pub.payload_string();

  if(String(pub.topic()) == "/switch/1")                          // Проверяем из нужного ли нам топика пришли данные
  {
    state_1 = payload.toInt();                               // Преобразуем полученные данные в тип integer
    digitalWrite(12, state_1);   
    Serial.println("switch 1");                                   // Отправляем данные по COM порту 
  }

  if(String(pub.topic()) == "/switch/2")      
  {
    state_2 = payload.toInt();           
    digitalWrite(13, !state_2); 
    Serial.println("switch 2");
  }
  
}

WiFiClient wclient;    
PubSubClient client(wclient, mqtt_server, mqtt_port);

void key()
{
  if (digitalRead(0) == LOW)
  {
    touch = 1;
//    Serial.println("down");
  } else
  {
    touch = 2;  
//    Serial.println("up");
    if ((sch < 20) and (sch_2 <= 1) and (permit))               //Для мгновенного изменения состояния розетки даже без подключения к wi-fi
    {
      state_1 = !state_1;
      digitalWrite(12, state_1);
      mqtt_send = true;
    }
    
    sch = 0;
    sch_2 = 0;
  }
  
}

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  
  attachInterrupt(0, key, CHANGE);
  
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  digitalWrite(12, LOW);
  digitalWrite(13, HIGH);

  ssid = read_string_EEPROM(0);
  pass = read_string_EEPROM(32);

  Serial.println("From EEPROM");
  Serial.println(ssid);
  Serial.println(pass);

  if (read_type(100) == 0)
    change_wifi = false;
  if (read_type(100) == 1)
    change_wifi = true;

  if (change_wifi)                                  // Индикация режима работы при включении
  {
    blink_delay(2);
  }
  else
  {
    blink_delay(1);
  }

  delay(1500);                                      // В течении 1.5 сек можно изменить тип зпгрузки
    if ((touch == 1) or (touch == 2))
    {
      change_wifi = !change_wifi;
      if (change_wifi)
        write_type(100,1);
      else
        write_type(100,0);
      delay(100);
      resetFunc();
    }
  permit = true;
}

void check_button()
{
  if (touch == 1)
  {
    sch++;
    delay(100);
    Serial.println(sch);
  }
  
  if ((sch > 30) or (reboot_flag))                // благодаря следующим двум условиям перезагрузка произойдет только после отжатия кнопки boot mode не будет
  {
    digitalWrite(13, LOW);  
    reboot_flag = true;
    if (touch == 2)
    {    
      digitalWrite(13, HIGH);  
      delay(100);     
      Serial.println("Reboot. Type 1");
    
      touch = 0;
      sch = 0;
      
      write_type(100,1);
      delay(800);
      resetFunc();
    }
  }    
}

void check_button_2()
{
  if (touch == 1)
  {
    sch_2++;
    delay(100);
    Serial.print("'");
    Serial.println(sch_2);
  }
  
  if ((sch_2 > 1) or (reboot_flag_2))
  {
    digitalWrite(13, LOW);  
    reboot_flag_2 = true;
    
    if (touch == 2)
    {    
      digitalWrite(13, HIGH);  
      delay(100);     
      Serial.println("Reboot. Type 2");
    
      touch = 0;
      sch = 0;
      
      write_type(100,1);
      delay(800);
      resetFunc();
    }
  }  
}

void loop() 
{
  if (change_wifi)
  {
    digitalWrite(13, LOW);         

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_point, password_point);
    Serial.println("");
    
    // Wait for connection
    
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid_point);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address: ");
    Serial.println(myIP);
    
    server.on("/login", handleLogin);
    server.on("/inline", []()
    {
      server.send(200, "text/html", "<html><body><font face='courier'><p align='center'>Created by Kuchmiev Ivan, Moscow, 2017</a><p align='center'>adreal@mail.ru</a></font></body></html>");
    });
    
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("HTTP server started");
    
    while(true)
    {
      server.handleClient();
    }
  
  } else
    
  {
    WiFi.mode(WIFI_STA);
    
    if (WiFi.status() == WL_CONNECTED) 
    {
      check_button();
      if (wifi_connect_flag == false)
      {
        Serial.print("Connected to ");
        Serial.println(ssid);
        blink_delay(3);       
        wifi_connect_flag = true;
      }  
    } else
      check_button_2();
    
    if (WiFi.status() != WL_CONNECTED)                                              // подключаемся к wi-fi
    {      
      WiFi.begin(ssid, pass);
      if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return; 
    } 
    
    
    if (WiFi.status() == WL_CONNECTED) 
    {              
      if (!client.connected())                                                      // подключаемся к MQTT серверу
      {
        if (client.connect(MQTT::Connect("switsh").set_auth(mqtt_user, mqtt_pass))) 
        {
          client.set_callback(callback);
          client.subscribe("/switch/1");                                            // подписывааемся на нужный топик
          client.subscribe("/switch/2");
        }
      }
      
      if (client.connected())
      {
        client.loop();
      }
    } 

    if ((mqtt_send) and (WiFi.status() == WL_CONNECTED))
    {
      if (state_1)
        client.publish("/switch/1","1");
      else
        client.publish("/switch/1","0");    
      mqtt_send = false;
    }
     
  }
}
