#include <TM1637.h>

#include <HCSR04.h>

#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>
 
byte old_state;
byte state;

long duration; 
int distance; 

long timer;

TM1637 tm(D3,D4);

WiFiClient client;
HTTPClient http;


const char* server = "192.168.0.2";
const int port = 12345;
int stateConn = 0; // state of connection to server
char idClient[9]; // my id given by server.
char lineServer[1024]; // to store lines sent by server
int idLineServer = 0; // the index of the last char stored in line
String req;

void resetLineServer() {
 idLineServer = 0;
 for(int idx = 0; idx < 1025; idx++){
  lineServer[idx] = 0;
 }
}

/* readLineServer() :
*  try to read a text line from the server and store it in line
*  If the \n is encountered, readLineServer() stops to read from server
*  and return true.
*  
*  CAUTION: if \n is not encountered until in 1023 char, it also returns
*  true, as if end of line was reached.
*/
bool readLineServer() {
  char c;
  Serial.readBytes(&c, 1);
  for(int i=0; i < 1024; i++){
    if(c!='\n'){
      Serial.println(c);
      lineServer[i] = c;
      return false;
    }
    else{
      return true;
    }
  }
  return true;
}

void serverConnection() {

  Serial.print("connecting to ");
  Serial.println(server);  
  // Use WiFiClient class to create TCP connections

  if (!client.connect(server, port)) {
    Serial.println("connection failed");
    stateConn = 0;
  }
  else {   
    stateConn = 1;  
    Serial.println("connected to server. Asking my id");
    memset(idClient,0,9);
    client.println("0"); // envoi requête GETID
    while (!readLineServer()) {}
    for(int i=0;i<9;i++) idClient[i] = lineServer[i];
    resetLineServer();
    Serial.println("my id is "+String(idClient));    
  }
}

void requestStore() {
  /* FULFILL:
    - send 3 successive lines of text :
      - the request id, which "1"
      - the client id
      - the request parameters, which is a line like "20.34,0,-1"
    - wait server's answer
    - print a message (ok/error) on Serial
 */
 Serial.println('1');
 Serial.println(idClient);
 Serial.println(distance + ',' + state);
}


void requestTriger() {
  Serial.print("[HTTP] begin...\n");

  if (state == 0) {
     http.begin(client, "http://192.168.0.2/set_state.php?state=0");
  } else {
    http.begin(client, "http://192.168.0.2/set_state.php?state=1");
  }
  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
   
  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
   
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } 
  else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
   
  http.end();
}

void setupWifi() {
     
  const char *ssid = "mobinum_objconn"; // the ssid of the AP        
  const char *password = "objconn_mobinum"; // the password of the AP  
  
  WiFi.setAutoConnect(false);  // see comments   
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // see comments

  WiFi.mode (WIFI_STA); // setup as a wifi client
  WiFi.begin(ssid,password); // try to connect
  while (WiFi.status() != WL_CONNECTED) { // check connection
    delay(500); 
    Serial.print(".");
  } 
  // debug messages
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}
 
void testSwitch(bool init) {
  state = digitalRead(D2);  
  if ((init) || (old_state != state)) {
    if (state == 0) {
      Serial.println("switch off => light off");
      digitalWrite(D1,LOW);
    }
    else {
      Serial.println("switch on => light on");
      digitalWrite(D1,HIGH);    
    }  
    requestTriger();
    requestStore();
    old_state = state;
  }
}

void blinkBassedOnDis() {
  analogWriteRange(10);
  analogWriteFreq(distanceToHz());
  analogWrite(D1,100); 
}

int distanceToHz() {
  float frequency;
  if(distance < 10) {
    frequency = 1;
  } else if(distance > 200) {
    frequency = 100;
  } else {
    frequency = distance / 10.0;
  }
  Serial.print("Frequency: ");
  Serial.println(frequency);
  return frequency;
}

void distanceSensor() {
    digitalWrite(D5, LOW);
    delayMicroseconds(2);
    // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
    digitalWrite(D5, HIGH);
    delayMicroseconds(10);
    digitalWrite(D5, LOW);
    // Reads the echoPin, returns the sound wave travel time in microseconds
    duration = pulseIn(D6, HIGH);
    // Calculating the distance
    distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)
    // Displays the distance on the Serial Monitor
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
}

void displayDistance() {
  tm.display(1, distance % 10);   
  tm.display(0, distance / 10 % 10);
}

void requestStoreIfDistance() {
   if (distance < 40 || distance > 80){
    requestStore();
  }
}

void setup() {
 
  Serial.begin(115200);
  delay(10);

  timer = millis();

  tm.init();
  // set brightness; 0-7
  tm.set(6);

  pinMode(D5, OUTPUT); // Sets the trigPin as an OUTPUT
  pinMode(D6, INPUT);
 
  // power off Wifi to save energy
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(10); 
  setupWifi();
 
  pinMode(D2,INPUT); // switch
  testSwitch(true); // first call to determine the initial state 
  pinMode(D1,OUTPUT); // LED
  digitalWrite(D1,LOW); // switch off the led at start
  delay(10); 
}

void loop() {
  testSwitch(false);

  long t = millis();
  if (t - timer > 500) {
    timer = t;
//
//    if(stateConn == 0) {
//      serverConnection();
//      if(stateConn == 0) {
//        delay(2000);
//      }
//    }

    if(state != 0 ) {
      distanceSensor();
  
      displayDistance();
  
      blinkBassedOnDis();
    }

   tm.display(3, state);
  }
  if (t - timer > 5000) {
    requestStore();
  }
  //requestStoreIfDistance();
}
