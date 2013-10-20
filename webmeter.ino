
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEBUG true  // Turn on/off print_dbg messages
#define BUS 2       // PIN for DATA wire of the DS18B20
#define VIN A0      // AnalogRead of VIN for power source

int pins[] = { 4, 5, 6, 7, 8, 9, A2, A3 };
volatile int state = HIGH;
boolean lastConnected = false;
int cnt = 0;
int flip = 0;
boolean display = false;
String j_post = "/json";
String d_get = "/display";
String src = "PoE";
String err = "OK";

byte mac[] = { 0x00, 0xAB, 0xBC, 0xCD, 0xCD, 0xDE };
char server[] = "192.168.20.127";
IPAddress dhcp1 = (192,168,20,126); 
int port = 4567;
EthernetClient client;

float temp;
OneWire one_wire(BUS);
DallasTemperature sensor(&one_wire);

void print_dbg(String str) { if(DEBUG) { Serial.print(str); } }

void setup() 
{
  attachInterrupt(1, interrupt, CHANGE);
  pinMode(3, INPUT); 
  digitalWrite(3, HIGH); 
  for(cnt = 0; cnt < 8; cnt++){
    pinMode(pins[cnt], OUTPUT);
    digitalWrite(pins[cnt], LOW);  
  }
  Serial.begin(9600);
  sensor.begin();
  
  while (Ethernet.begin(mac) == 0) {
    //Ethernet.begin(mac, dhcp1);
    print_dbg("DHCP - F\n");
  }
  delay(500);
  print_dbg("CNT\n");
}

void loop()
{
  if ( client.connect(server, port) ){
    get_temp();
    if( error() ){
      post(create_json());
      delay(100);
      print_dbg(err);
    }
    else {
      if (!client.connected() && lastConnected) {
        client.stop();
      }
      if( flip == 0 ){
        get();
        delay(10);
        client_response();
        flip = 1;
        client.stop();
      }
      else{
        post(create_json());
        flip = 0;
      }
    }
  }
  delay(100);
  lastConnected = client.connected();
}

void interrupt(){
  state = !state;
  if( state == HIGH ){
    char buf[12];
    String bin = String((int)temp, BIN);
    int length = bin.length();
    int start = 8 - length;
    
    for(int i = 0; i < start; i++){
      bin = "0" + bin; 
    }
    for(cnt = 0; cnt < 8; cnt++){  
      if(bin.charAt(cnt) == '1' ){
        digitalWrite(pins[cnt], HIGH);   
      }
      else {
       digitalWrite(pins[cnt], LOW); 
      }
    }
  }
  else{
    for(cnt = 0; cnt < 8; cnt++){  
      digitalWrite(pins[cnt], LOW);   
    }
  }
}

boolean error(){
  // R1 = 300 Ohms and R2 = 200 Ohms
  // 12v around 4.7v, 9v around 3.6v, and 5v around 2v
  int vol_div = analogRead(VIN);
  float voltage = vol_div * (5.0 / 1023.0); //set to battery voltage
  if ( voltage < 1.7 ){
    src = "PoE";
    Serial.println(voltage);
  }
  else {
    src = "Battery";
  }
  if ( temp < -100 ){
    err = "No Probe";
    return true;
  }
  print_dbg(src);
  err = "OK";
  return false;
}

void display_on(){
  char buf[12];
  String bin = String((int)temp, BIN);
  int length = bin.length();
  int start = 8 - length;
  
  for(int i = 0; i < start; i++){
    bin = "0" + bin; 
  }
  for(cnt = 0; cnt < 8; cnt++){  
    if(bin.charAt(cnt) == '1' ){
      digitalWrite(pins[cnt], HIGH);   
    }
    else {
     digitalWrite(pins[cnt], LOW); 
    }
  }
}

void display_off(){
  for(cnt = 0; cnt < 8; cnt++){  
      digitalWrite(pins[cnt], LOW);   
  }
}

void get_temp(){
  sensor.requestTemperatures();
  temp = sensor.getTempCByIndex(0);
  print_dbg("Temperature for Device 1 is: ");
  print_dbg(String((int)temp));
  print_dbg("\n");
}

void client_response(){
  String response = "";
  while (client.available()) {
    char c = client.read();
    response += c;
    if (c == '\n') {
      response = "";
    }
    if (response.equals("ON")) {
        display_on(); 
    }
    if (response.equals("OFF")) {
        display_off(); 
    }  
  }
}

String create_json()
{
  return "{\"plot\":\""+String((int)temp)+"\",\"error\":\""+err+"\",\"source\":\""+src+"\"}";
}

void get()
{
    client.println("GET /display HTTP/1.1");
    client.println("Host: 192.168.20.127:4567");
    client.println("Connection: close");
    client.println("");
}

void post(String json)
{
    // Make a HTTP request:
    client.println("POST /json HTTP/1.1");
    client.println("User-Agent: Arduino");
    client.println("Host: 192.168.20.127:4567");
    client.print("Accept: *"); client.print("/"); client.println("*");
    client.print("Content-Length: "); client.println(json.length()+2);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: close");
    client.println("");
    client.println(json);
    client.stop();
}