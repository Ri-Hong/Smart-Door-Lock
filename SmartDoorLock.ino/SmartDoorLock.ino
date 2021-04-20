#include <LiquidCrystal.h> //Used for the LCD display
#include <ESP32Servo.h> //Used for the servo
#include <WiFi.h> //Used to connect to wifi
#include "PubSubClient.h" //Used to interface with MQTT

//Declare all the MQTT functions beforehand
void handleRcvMsg( String sMsg );
void setup_wifi();
void publishMsg(char *pMsg);
void callback(char* topic, byte *payload, unsigned int length);
void reconnect();
void publishHeartbeat();

//Declare other functions (such as processing pin, turning the servo, etc.)
bool inArray(char targetStr, char *arr2[10]);
void handleKeyPress();
void validatePin();
void handleCommand();
void displayLocked();
void displayUnlocked();
void lockDoor();
void unlockDoor();
String generateId();
void updateCredentialArrays(String messageStr);
bool inArray(String targetStr, String arr2[10]);
int nextHeartbeatTime = 0; //Time in ms indicating when to send out next heartbeat. "heartbeat" is published regularly to show that this device is alive. Used in publishHeartbeat()
unsigned long wakeUpTime = millis(); //Any message received before this time is ignored. Used in handleRcvMsg()

// Update these with values suitable for your network.
const char* ssid = ""; //FILL IN WITH YOUR SSID
const char* password = ""; //FILL IN WITH YOUR NETWORK PASSWORD
const char* mqtt_server = "broker.hivemq.com"; //You can choose to use another server
#define mqtt_port 1883
#define MQTT_USER "Door" //You can choose to change the user if you want
#define MQTT_PASSWORD "" //leave blank if using a public broker
#define MQTT_PUBLISH_TOPIC "" // CHOOSE A TOPIC NAME. 
#define MQTT_SUBSCRIBE_TOPIC "" //SHOULD BE SAME AS MQTT_PUBLISH_TOPIC
WiFiClient wifiClient; //Create a wifi client
PubSubClient client(wifiClient); //Create a pub sub client

LiquidCrystal lcd(22,23,5,18,19,21); //Create an lcd object using the LCD pins
Servo servo;  //Create a servo object

int touchButtonPins[4] = {13, 12, 14, 27}; //Stores the corresponding pin values on the ESP32 that map to the pins on the HW-138
String names[10] = {}; //Stores the existing names of the users. Max # of users is capped at 10 for now
String ids[10] = {}; //Stores the existing ids of the users 
String pins[10] = {}; //Stores the existing pins of the users
int numUsers = 0; //Stores the amount of users currently registered
int userIndex; //Stores the index of the id, name and pin that is being procesed
String enteredPin; //Stores the numbers the user has currently entered. Pin will be 8 digits
bool locked = true; //Stores whether the door is locked

void setup() {
  Serial.begin(9600); //Setup the serial monitor

  //Initialize the pins connected to the keypad
  for (int i = 0; i < 5; i++){
    pinMode(touchButtonPins[i], INPUT);
  }

  //Initialize the LCD
  lcd.begin(16, 2); //16 chars, 2 lines each
  displayLocked();

  //Allow allocation of all timers for servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);    //Standard 50 hz servo
  servo.attach(2, 500, 2500); //Attaches the servo on pin 4 to the servo object
  //Set a min/max of 500us and 2400us

  //Setup wifi and MQTT connection
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port); //setup mqtt server and port
  client.setCallback(callback); //execture calback function when an incoming message is received
  reconnect();
}

void loop() {
  if (locked){ //If the door is locked
    //Check if any keys on the touchpad were pressed
    handleKeyPress();
  
    //Check if the pin needs to be processed
    if (enteredPin.length() == 8){
      delay(300); //Delay so that the last digit entered can be seen and it does not switch instantly to the pin handling screen
      validatePin(); //Check if pin is correct
    }
  }
  else{ //If the door is unlocked
    //Check if any keys on the touchpad were pressed
    handleKeyPress();

    //Check if the pin needs to be processed
    if (enteredPin.length() == 8){
      delay(300); //Delay so that the last digit entered can be seen and it does not switch instantly to the pin handling screen
      handleCommand(); //Check if pin is correct
    }
  }
  
  client.loop(); //Used to monitor the server and read incoming messages

  publishHeartbeat();
   
  if (millis() < 10000){ //Used to deal with rollover of the millis() function
    wakeUpTime = millis();
  }
  delay(10);
}

//Functions declared below
//------------------------------------------------------
//Checks if a string is in an array of strings. Returns the index of the target string in the array if found, -1 otherwise.
bool inArray(String targetStr, String arr2[10]){
  for (int i = 0; i < 10; i++){
    if (targetStr == arr2[i]){ //If target is found, return the index
      return i;
    }
  }
  return -1; //If target is not found, return -1
}

//------------------------------------------------------
//Checks if any keys on the touchpad were pressed
void handleKeyPress(){ 
  //Checks for input from all 4 input pins
  for (int i = 0; i < 4; i++){
    if (digitalRead(touchButtonPins[i])){
      Serial.println("Button " + String(i + 1) + " was pressed"); //Output to serial monitor for debugging
      enteredPin.concat(i+1); //Add the pressed digit to the enteredPin array
      delay(300); //Delay to avoid double presses registering
    }
  }
  //Update display
  lcd.setCursor(4, 1); //Set cursor to row 1, column 4 (Indexed from 0)
  for (int i = 0; i < enteredPin.length(); i++){
    lcd.print(enteredPin[i]);
  }

}

//------------------------------------------------------
//Checks if pin is correct
void validatePin(){
  String id = enteredPin.substring(0, 4); //Assign the first 4 characters of the entered pin to id
  String pin = enteredPin.substring(4, 8); //Assign the last 4 characters of the entered pin to pin

  if (inArray(id, ids) >= 0){ //If the id entered is an existing id
    userIndex = inArray(id, ids); //Set the user index
    //Process pin
    if (pin == pins[userIndex]){ //If the pin is correct, then unlock the door
      locked = false; //Set locked to false
      unlockDoor();
      
      //Display unlocked
      displayUnlocked();
    }
    else{ //If the pin is incorrect
      //Print out the error message
      lcd.clear();
      lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
      lcd.print("Incorrect Pin");
      lcd.setCursor(3, 1); //Set cursor to row 1, column 3 (Indexed from 0)
      lcd.print("Try Again");
      //Wait for 2s to let the user read the message
      delay(2000);
      //Display locked
      displayLocked();
    }
  }
  else{ //If the id is incorrect
    //Print out the error message
    lcd.clear();
    lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
    lcd.print("Incorrect Pin");
    lcd.setCursor(3, 1); //Set cursor to row 1, column 3 (Indexed from 0)
    lcd.print("Try Again");

    //Wait for 2s to let the user read the message
    delay(2000);
    //Display locked
    displayLocked();
  }
  //Reset the enteredPin string for the next time
  enteredPin =  "";  
}

//------------------------------------------------------
//Processess commands when for when the door is unlocked
void handleCommand(){
  String id = enteredPin.substring(0, 4); //Assign the first 4 characters of the entered pin to id
  String pin = enteredPin.substring(4, 8); //Assign the last 4 characters of the entered pin to pin

  if (inArray(id, ids) >= 0){ //If the id entered is an existing id
    userIndex = inArray(id, ids); //Set the user index
    //Process pin
    if (pin == pins[userIndex]){ //If the pin is correct, then lock the door
      locked = true; //Set locked to false
      lockDoor();
      
      //Display locked
      displayLocked();
    }
    else{ //If the pin is incorrect
      //Print out the error message
      lcd.clear();
      lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
      lcd.print("Incorrect Pin");
      lcd.setCursor(3, 1); //Set cursor to row 1, column 3 (Indexed from 0)
      lcd.print("Try Again");
      //Wait for 2s to let the user read the message
      delay(2000);
      //Display locked
      displayUnlocked();
    }
  }
  else{ //If the id is incorrect
    //Print out the error message
    lcd.clear();
    lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
    lcd.print("Incorrect Pin");
    lcd.setCursor(3, 1); //Set cursor to row 1, column 3 (Indexed from 0)
    lcd.print("Try Again");

    //Wait for 2s to let the user read the message
    delay(2000);
    //Display locked
    displayLocked();
  }
  //Reset the enteredPin string for the next time
  enteredPin =  "";  
}

//------------------------------------------------------
//Function displays the locked screen
void displayLocked(){
  //Print the locked message
  lcd.clear();
  lcd.print("Enter Your Pin:");
}

//------------------------------------------------------
//Function displays the unlocked screen
void displayUnlocked(){
  //Print the unlocked message
  lcd.clear();
  lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
  lcd.print("Unlocked"); 
}

//------------------------------------------------------
//Function turns the servo to lock the door
void lockDoor(){
  //Print out the locking message
  lcd.clear();
  lcd.setCursor(3, 0); //Set cursor to row 0, column 3 (Indexed from 0)
  lcd.print("Locking...");

  //Output to the server who locked the door
  String outputStr = "Door locked by " + names[userIndex];
  char outputCharArray[outputStr.length()];
  outputStr.toCharArray(outputCharArray, outputStr.length()+1);
  publishMsg(outputCharArray);
  
  //Turn the servo to lock the door
  for (int i = 180; i >= 0; i--){
    servo.write(i);
    delay(10);
  }
}

//------------------------------------------------------
//Function turns the servo to unlock the door
void unlockDoor(){
  //Print out the unlocking/welcome message
  lcd.clear();
  lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
  lcd.print("Welcome,");
  int nameLen = names[userIndex].length();
  int desiredIndex = (int) (16 - nameLen) / 2;
  lcd.setCursor(desiredIndex, 1); //Set cursor to row 0, column desiredIndex (Indexed from 0)
  lcd.print(names[userIndex] + "!");

  //Output to the server who unlocked the door
  String outputStr = "Door unlocked by " + names[userIndex];
  char outputCharArray[outputStr.length()];
  outputStr.toCharArray(outputCharArray, outputStr.length()+1);
  publishMsg(outputCharArray);
  
  //Turn the servo to unlock the door
  for (int i = 0; i <= 180; i++){
    servo.write(i);
    delay(10);
  }
}

//------------------------------------------------------
//Function takes in a string returned from the app and updates the names, pins and ids arrays
void updateCredentialArrays(String messageStr){

  int commaIndexes[3];
  int commaIndexesIndex = 0;
  //Get the index of the ","s
  for (int i = 0; i < messageStr.length(); i++){
    if (messageStr[i] == ','){
      commaIndexes[commaIndexesIndex] = i;
      commaIndexesIndex++;
    }
  }

  String namesString = messageStr.substring(0, commaIndexes[0]);
  String idsString = messageStr.substring(commaIndexes[0] + 1, commaIndexes[1]);
  String pinsString = messageStr.substring(commaIndexes[1] + 1, messageStr.length());

  Serial.println(namesString);
  Serial.println(idsString);;
  Serial.println(pinsString);
    
  //Find the correct array index for the id
  for (int i = 0; i < numUsers; i++){
    if (idsString == ids[i]){
      userIndex = i;
      break;
    }
  }

  //Update the names and pins array
  names[userIndex] = namesString;
  pins[userIndex] = pinsString;

  //Output for debugging
  for (int i = 0; i < numUsers; i++){
    Serial.print(names[i]);
    Serial.print(ids[i]);
    Serial.println(pins[i]);
  }
  
}

//------------------------------------------------------
//Function generates a unique id not already in use
String generateId(){
  //Loop through all combinations of ids
  for (int digit1 = 1; digit1 <= 4; digit1++){
    for (int digit2 = 1; digit2 <= 4; digit2++){
      for (int digit3 = 1; digit3 <= 4; digit3++){
        for (int digit4 = 1; digit4 <= 4; digit4++){
          String currentId = String(digit1) + String(digit2) + String(digit3) + String(digit4); //Convert the combination into a string
          //Check if the current id exists
          bool existing = false;
          for (int i = 0; i < numUsers; i++){
            if (currentId == ids[i]){
              existing = true;
            }
          }
          if (existing == false){ //If the id doesn't exist yet, update the ids array and numUsers and then return the id
            ids[numUsers] = currentId;
            numUsers++;
            return currentId;
          }
        }
      }
    }
  }
}

//Functions below deal with wifi and MQTT communication
//------------------------------------------------------
//Function used to publish a message "heartbeat" onto the server every 10 seconds
void publishHeartbeat(){
  int heartbeatInterval = 10000; //the delay between each heartbeat message in milliseconds
  reconnect(); //reconnect to the server if we are somehow disconnected
  //nextHeartbeatTime stores the time at which the next heartbeat should be sent. If the current time is greater then the next heartbeat time, then a heartbeat will be published. It then updates nextHeartbeatTime and also flashes the built in LED on the ESP01
  if(millis() > nextHeartbeatTime){ 
    if (locked){
      client.publish(MQTT_PUBLISH_TOPIC, "Locked");
      Serial.print("State: Locked");
    }
    else {
      client.publish(MQTT_PUBLISH_TOPIC, "Unlocked");
      Serial.print("State: Unlocked");
    }
    nextHeartbeatTime += heartbeatInterval;
  }
}


//------------------------------------------------------
//Used to publish an acknowledgement to a request to open the door
void publishMsg(char *pMsg){ 
  if (!client.connected()) { //Will check if connected to server. If not, it will attempt to reconnect.
    reconnect();
  }
  client.publish(MQTT_PUBLISH_TOPIC, pMsg);
}

//------------------------------------------------------
//Used to setup wifi 
void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

//------------------------------------------------------
//Used to reconnect to the MQTT server and topic if not already connected
void reconnect() {
  static int reconnectionAttempts = 0;
  char pMsg[50];
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...

      sprintf( pMsg, "Hello from ESP: %d. Just connected", reconnectionAttempts++ );
      client.publish(MQTT_PUBLISH_TOPIC, pMsg);
      // ... and resubscribe
      client.subscribe(MQTT_SUBSCRIBE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//------------------------------------------------------
//Handles the first stage of incoming messages from the server
// when this function is called, the following is passed to this function:
// topic: the subscribed topic
// payload: the content of the received msg
// length: number of bytes in the msg
void callback(char* topic, byte *payload, unsigned int length) {
  char pPayload[50] = "";
  
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");  
  Serial.write(payload, length);
  Serial.println();

  for( int i=0; i<length; i++ ) //convert the received bytes to chars
  {
    sprintf( pPayload, "%s%c", pPayload, payload[i] ); 
  }

  //Once a request to open the door is received, all other messages will be ignored for the miliseconds specified by msSleepTime found in handleRcvMsg()
  if(millis() >= wakeUpTime){
    handleRcvMsg( (String) pPayload );
  }
  else{
    Serial.println("Sleeping, message ignored");
  }
}

//------------------------------------------------------
//Handles the second stage of incoming messages from the server
void handleRcvMsg( String sMsg ){
  int msSleepTime = 3000; //How long after each request will all incoming requests for the next n miliseconds be ignored. Currently 3 sec. Purpose is to avoid accidential multiple presses of the button in the app
  Serial.println(sMsg);
  //Handle messages
  if (sMsg == "Locked"){ //Update locked
    if (locked == false){ //Update display if incoming state is different than current state
      lockDoor();
      displayLocked();
    }
    locked = true;
  }
  else if (sMsg == "Unlocked"){ //Update locked
    if (locked == true){ //Update display if incoming state is different than current state
      unlockDoor();
      displayUnlocked();
    }
    locked = false;
  }
  else if(sMsg == "query state"){// App querying the state of the lock on startup
    if (locked){
      publishMsg("Locked");
    }
    else{
      publishMsg("Unlocked");
    }
  }
  else if (sMsg.substring(0, 18) == "update credentials"){
    Serial.println("Updating Credentials...");
    updateCredentialArrays(sMsg.substring(19, sMsg.length()));
  }
  else if (sMsg.substring(0, 11) == "generate id"){
    String outputMessage = "unique id " + generateId();
    //Convert the string to a char array because publishMsg only accepts char arrays
    char messageCharArray[outputMessage.length()];
    outputMessage.toCharArray(messageCharArray, outputMessage.length()+1);
    publishMsg(messageCharArray);
  }
  else{ //for unrecognizable requests
    Serial.print("Unrecognized code: ");
    Serial.println(sMsg);
  }
}
