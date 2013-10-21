/*****************************************************
Project: EE Design Lab #2
Programmers: Pasquale D'Agostino & James Couch
Program Name: webmeter.ino
Description: 
This sketch is for an Arduino w/ Ethernet capabilities
to communicate with a web app. The Arduino reads from
a DS18B20 temperature probe using the OneWire and Dallas
Temperature libraries. The Arduino then makes either a
HTTP POST or GET request to the server. The Ethernet
connections are handled by the Ethernet library from
Arduino.
*****************************************************/

#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEBUG true  // Turn on/off print_dbg messages
#define BUS 2       // PIN for DATA wire of the DS18B20
#define VIN A0      // AnalogRead of VIN for power source

int pins[] = { 4, 5, 6, 7, 8, 9, A2, A3 };           // Pins used by LED Display 
boolean lastConnected = false;
boolean last = true;
int cnt = 0;
int flip = 0;

// Strings to format different queries
String src = "PoE";
String err = "OK";

// Ethernet connection globals
byte mac[] = { 0x00, 0xAB, 0xBC, 0xCD, 0xCD, 0xDE }; // MAC address of the Arduino
char server[] = "192.168.0.127";                     // IP Address of the Server
IPAddress dhcp1 = (192,168,20,126);                  // Backup DHCP Address for Stand Alone Mode
int port = 4567;                                     // Web app port on Server
EthernetClient client;

// Temperature Probe globals, code from Dallas example
float temp;
OneWire one_wire(BUS);
DallasTemperature sensor(&one_wire);

void print_dbg(String str) { if(DEBUG) { Serial.print(str); } } // Toggle Debug messages

void setup() 
{
  pinMode(A5,OUTPUT);
  Serial.begin(9600);
  sensor.begin();
  
  for(cnt = 0; cnt < 8; cnt++){
    pinMode(pins[cnt], OUTPUT); 
  }
  
  Ethernet.begin(mac);
  delay(500);
  print_dbg("CNT\n");
}

/*
  Loop Logic: The loop() in Arduino code is the part that
  is always executed after setup. Once in the loop() it will
  repeat the tasks unless if told otherwise. So we implemented
  our state machine here.

  Check Errors -> error ->   POST to Server   -> Check Errors
               -> none  -> GET/POST to Server -> Check Errors

  In each loop() if we are going to communicate with the Server
  we only process one request per loop. This is because we need 
  to make sure that the request is handled properly and does not
  receive another requests response from the server.
*/
void loop()
{
  get_temp();                                      // Need current temp for errors, Stand Alone                                
  if( error() ){                                   // If any errors, handle only them
    delay(50); 
    print_dbg(err);
  }
  if ( client.connect(server, port) ){
    get_temp();                                    // Need current temp for errors, Non Stand Alone                                     
    if( error() ){                                 // If any errors, handle only them
      post(create_json());                         // POST errors to web server
      delay(50); 
      print_dbg(err);
    }
    else {
      if (!client.connected() && lastConnected) {  // Need to make sure previous connections are closed
        client.stop();
      }
      if( flip == 0 ){                             // Flip between POST & GET requests each time
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
  lastConnected = client.connected();             // Get connection state for next request
}

/*
  Function is boolean so we only process certain
  errors (return early).

  Process any errors and create messages. We check
  if the voltage is less than 3.7v to determine source.
  On PoE A0 will read 3.6v and on battery it will be
  around 4.7v. Next if the temperature is ever less
  than -100 we know that the probe is unplugged so we
  flash a message on the display before returning.   
*/
boolean error(){
  // R1 = 300 Ohms and R2 = 200 Ohms
  // PoE = 3.6 V
  // 12v around 4.7v, 9v around 3.6v, and 5v around 2v
  int vol_div = analogRead(VIN);
  float voltage = vol_div * (5.0 / 1023.0);
  if ( voltage < 3.7 ){
    src = "PoE";
    err = "No Battery";
    return true;
  }
  else {
    src = "Battery";
  }
  if ( temp < -100 ){
    err = "No Probe";
    digitalWrite(A5,HIGH);
    for(cnt = 0; cnt < 8; cnt++){  
      digitalWrite(pins[cnt], HIGH);
      delay(50);  
      digitalWrite(pins[cnt], LOW); 
    }
    return true;
  }
  else if (last == false){
   digitalWrite(A5,LOW); 
  }
  print_dbg(src);
  err = "OK";
  return false;
}

/*
  Convert the temperature into a binary string and 
  pad it with zeros for to have 8 bit resolution.
  This allows us to iterate over the string and check
  each character for a 1 or 0 and then reflect that
  on the display.
*/
void display_on(){
  char buf[12];
  String bin = String((int)temp, BIN);        // Creates a binary string of the temp
  int length = bin.length();
  int start = 8 - length;                     // Gives us number of 0s to pad string
  
  for(int i = 0; i < start; i++){
    bin = "0" + bin; 
  }
  for(cnt = 0; cnt < 8; cnt++){  
    if(bin.charAt(cnt) == '1' ){
      digitalWrite(pins[cnt], HIGH);          // Turn ON the LED connected to pins[cnt]
    }
    else {
     digitalWrite(pins[cnt], LOW);            // Turn OFF the LED connected to pins[cnt]
    }
  }
}
/*
  Get the current temperature from the probe 
  in degrees Celsius. Then write the current
  display value. We write the display because
  they only turn on when pin A5 is driven HIGH.
*/
void get_temp(){
  sensor.requestTemperatures();                 // code from Dallas example
  temp = sensor.getTempCByIndex(0);             // code from Dallas example
  print_dbg("Temperature for Device 1 is: ");
  print_dbg(String((int)temp));
  print_dbg("\n");
  display_on();
}

/*
  After making a request to the server we
  need to handle the response it sends back.
  This is important on the GET request because
  it tells the Arduino what to do with the 
  display. This function iterates over the 
  response and finds the content of the response.

  Modified from Ethernet example
*/
void client_response(){
  String response = "";
  while (client.available()) {
    char c = client.read();
    response += c;
    if (c == '\n') {
      response = "";
    }
    if (response.equals("ON")) {
        digitalWrite(A5, HIGH);
        last = true; 
    }
    if (response.equals("OFF")) {
        digitalWrite(A5, LOW);
        last = false; 
    }  
  }
}

/*
  Creates a JSON string that is sent to the Server
  with the current information to be displayed
*/
String create_json()
{
  return "{\"plot\":\""+String((int)temp)+"\",\"error\":\""+err+"\",\"source\":\""+src+"\"}";
}

/*
  Headers to be sent to the Server after the
  Arduino connects to the server. This allows
  us to see if the LED display needs to be
  turned ON or OFF.

  HTTP GET Request, client.println() -> code from Ethernet
*/
void get()
{
  client.println("GET /display HTTP/1.1");
  client.println("Host: 128.255.77.243:4567");
  client.println("Connection: close");
  client.println("");
}

/*
  Headers to be sent to the Server after the
  Arduino connects to the server. This allows
  us to send the JSON string with the current
  state of the Arduino to the web app.

  HTTP POST Request, client.println() -> code from Ethernet 
*/
void post(String json)
{
  client.println("POST /json HTTP/1.1");
  client.println("User-Agent: Arduino");
  client.println("Host: 192.168.0.127:4567");
  client.print("Accept: *"); client.print("/"); client.println("*");
  client.print("Content-Length: "); client.println(json.length()+2);
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Connection: close");
  client.println("");
  client.println(json);
  client.stop();
}
