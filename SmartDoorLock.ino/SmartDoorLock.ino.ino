#include <LiquidCrystal.h> //Used for the LCD display
#include <ESP32Servo.h> //Used for the servo

LiquidCrystal lcd(22,23,5,18,19,21); //Create an lcd object using the LCD pins
Servo servo;  //Create a servo object

int touchButtonPins[4] = {13, 12, 14, 27}; //Stores the corresponding pin values on the ESP32 that map to the pins on the HW-138
int correctPin[4] = {1, 2, 3, 4}; //Stores the correct pin
int enteredPin[4]; //Stores the numbers the user has currently entered. Pin will be 4 digits
int pinLength = 0; //Stores the amount of numbers in the enteredPin array. Will be used to add new numbers to enteredPin
bool locked = true;

//Compares two arrays and checks if they are equal
bool arrayIsEqual(int arr1[4], int arr2[4]){
  for (int i = 0; i < 4; i++){
    if (arr1[i] != arr2[i]){
      return false;
    }
  }
  return true;
}

//Checks if any keys on the touchpad were pressed
void handleKeyPress(){ 
  //Checks for input from all 4 input pins
  for (int i = 0; i < 4; i++){
    if (digitalRead(touchButtonPins[i])){
      Serial.println("Button " + String(i + 1) + " was pressed"); //Output to serial monitor for debugging
      enteredPin[pinLength] = i+1; //Add the pressed digit to the enteredPin array
      pinLength++; //Increment the index by 1
      delay(300); //Delay to avoid double presses registering
    }
  }
  //Update display
  lcd.setCursor(6, 1); //Set cursor to row 1, column 4 (Indexed from 0)
  for (int i = 0; i < pinLength; i++){
    lcd.print(enteredPin[i]);
  }
}

//Checks if pin is correct
void validatePin(){
  if (arrayIsEqual(enteredPin, correctPin)){ //If the pin is correct, then unlock the door
    //Print out the welcome message
    lcd.clear();
    lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
    lcd.print("Welcome!");
    locked = false; //Set locked to false

    //Turn the servo to unlock the door
    for (int i = 0; i <= 180; i++){
      servo.write(i);
      delay(10);
    }
    //Display unlocked
    lcd.clear();
    lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
    lcd.print("Unlocked");
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
    //Ask the user to enter pin
    lcd.clear();
    lcd.print("Enter Your Pin:");
  }

  //Reset the enteredPin array and length for the next time
  memset(enteredPin, 0, sizeof(enteredPin)); //Set the entered pin array to be 0s
  pinLength = 0;
  
}

void getNewPin(){
  //Reset the enteredPin array and length to allow user to enter new pin
  memset(enteredPin, 0, sizeof(enteredPin)); //Set the entered pin array to be 0s
  pinLength = 0;
  
  while (pinLength != 4){
    handleKeyPress();
  }
}

void handleCommand(){
  int lockCode[4] = {1, 1, 1, 1}; //Stores the code for locking the door
  int resetCode[4] = {2, 2, 2,2}; //Stores the code for resetting the pin

  if (arrayIsEqual(enteredPin, lockCode)){ //If the pin is 1 1 1 1, then lock the door
    locked = true; //Set locked to true
    //Print out the locking message
    lcd.clear();
    lcd.setCursor(3, 0); //Set cursor to row 0, column 3 (Indexed from 0)
    lcd.print("Locking...");
    
    //Turn the servo to lock the door
    for (int i = 180; i >= 0; i--){
      servo.write(i);
      delay(10);
    }

    //Ask the user to enter pin for the next iteration of the main loop
    lcd.clear();
    lcd.print("Enter Your Pin:");
  }
  else if (arrayIsEqual(enteredPin, resetCode)){ //If the pin is 2 2 2 2, then reset the password
    //Keep getting new pin until pin is valid
    bool pinInvalid = true;
    while (pinInvalid){
      //Print out the password reset message
      lcd.clear();
      lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
      lcd.print("Enter New Pin:");
      getNewPin();
      delay(300); //Delay so that the last digit entered can be seen and it does not switch the screen as soon as the 4th character is inputted

      if (arrayIsEqual(enteredPin, resetCode) || arrayIsEqual(enteredPin, lockCode)){ //If pin is invalid
        //Print out an error message
        lcd.clear();
        lcd.setCursor(2, 0); //Set cursor to row 0, column 2 (Indexed from 0)
        lcd.print("Invalid Pin");
        lcd.setCursor(0, 1); //Set cursor to row 0, column 1 (Indexed from 0)
        lcd.print("Please Try Again");
  
        //Wait so the user can read the message
        delay(2000);
      }
      else{ //If pin is valid
        //Print out a success message
        lcd.clear();
        lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
        lcd.print("Success!");
        
        //Wait so the user can read the message
        delay(2000);
        pinInvalid = false;
      }
    }

    memcpy(correctPin, enteredPin, sizeof(enteredPin)); //Copy the pin from the enteredPin array to the correctPin array
    
    //Print out the password reset success message
    lcd.clear();
    lcd.setCursor(1, 0); //Set cursor to row 0, column 1 (Indexed from 0)
    lcd.print("Your New Pin:");
    lcd.setCursor(6, 1); //Set cursor to row 1, column 6 (Indexed from 0)
    for (int i = 0; i < 4; i++){
      lcd.print(correctPin[i]);
    }
    //Delay for 2s so the user can see the message
    delay(4000);

    //Reprint the unlocked message
    lcd.clear();
    lcd.setCursor(4, 0); //Set cursor to row 0, column 4 (Indexed from 0)
    lcd.print("Unlocked");
  }

  //Reset the enteredPin array and length for the next time
  memset(enteredPin, 0, sizeof(enteredPin)); //Set the entered pin array to be 0s
  pinLength = 0;
 
}


void setup() {
  Serial.begin(9600); //Setup the serial monitor

  //Initialize the pins connected to the keypad
  for (int i = 0; i < 5; i++){
    pinMode(touchButtonPins[i], INPUT);
  }

  //Initialize the LCD
  lcd.begin(16, 2); //16 chars, 2 lines each
  lcd.clear();
  lcd.print("Enter Your Pin:");

  //Allow allocation of all timers for servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);    //Standard 50 hz servo
  servo.attach(2, 500, 2400); //Attaches the servo on pin 4 to the servo object
  //Set a min/max of 500us and 2400us
}



void loop() {
  if (locked){ //If the door is locked
    //Check if any keys on the touchpad were pressed
    handleKeyPress();
  
    //Check if the pin needs to be processed
    if (pinLength == 4){
      delay(300); //Delay so that the last digit entered can be seen and it does not switch instantly to the pin handling screen
      validatePin(); //Check if pin is correct
    }
  }
  else{ //If the door is unlocked
    //Check if any keys on the touchpad were pressed
    handleKeyPress();

    //Check if the pin needs to be processed
    if (pinLength == 4){
      delay(300); //Delay so that the last digit entered can be seen and it does not switch instantly to the pin handling screen
      handleCommand(); //Check if pin is correct
    }
  }
}
