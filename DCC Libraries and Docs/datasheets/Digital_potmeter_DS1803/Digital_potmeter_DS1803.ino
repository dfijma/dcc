#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);


byte pot[2] = {1, 1};
byte controlByte = B0101000; // 7 bits, 
byte commandByte = B10101001; // last 2 bits is potmeter selection.
byte potValue[2];
int i = 0;
int deBounceTime = 10;
const int encoder_A = 8;
const int encoder_B = 9;
const int buttonPin = 2;
unsigned long newDebounceTime = 0;
unsigned long oldTime;
boolean pressed = 0;
boolean count = 1;


void setup() {
  Wire.begin();  
  Serial.begin(9600);
  pinMode(encoder_A, INPUT);
  pinMode(encoder_B, INPUT);
  pinMode(buttonPin, INPUT);
  newDebounceTime = millis();

  display.begin();
  display.setContrast(50);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,10);
  display.println("POT 1=");
  display.setCursor(0,22);
  display.println("POT 2=");
  display.display();

}

void loop() {

  interval();
  if(digitalRead(buttonPin)== 1 && (pressed == 0)){toggleBuffer();}
  if(digitalRead(buttonPin)== 0){pressed = 0;}

  
  if (digitalRead(encoder_A) == 0 && count == 0 && (millis() - newDebounceTime > deBounceTime)){
      if (digitalRead(encoder_B) == 0){
        pot[i]++;
        if(pot[i] > 25){pot[i] = 25;}
      }else{
        pot[i]--; 
        if(pot[i] < 1){pot[i] = 1;}
        }
      count = 1;
      newDebounceTime = millis();

      Wire.beginTransmission(controlByte);  // start transmitting
      Wire.write(commandByte);              // selection of potmeters
      Wire.write(pot[0] * 10);              // send first byte of potmeter data 
      Wire.write(pot[1] * 10);              // send second byte of potmeter data 
      Wire.endTransmission();               // stop transmitting
      
  }else if (digitalRead(encoder_A) == 1 && digitalRead(encoder_B) == 1 
                && count == 1 && (millis() - newDebounceTime > deBounceTime)){
    count = 0;
    newDebounceTime = millis();
  }  
}

void toggleBuffer(){
  pressed = 1;
  if (i == 0){i = 1;}else{i = 0;}
}

void writeToLCD(){
    Wire.requestFrom(controlByte, 2);
    potValue[0] = Wire.read();                // read first potmeter byte
    potValue[1] = Wire.read();                // read second potmeter byte
  
    display.fillRect(40, 0, 40, 45, WHITE);   // clear variable screen on LCD
    display.setCursor(40,10);                
    display.print(potValue[0]);               // write first potmeter value to LCD
    display.setCursor(40,22);
    display.print(potValue[1]);               // write second potmeter value to LCD
    display.setCursor(60,(10 + i * 12));
    display.print("<");
    display.display();  
}

void interval(){                              // interval timer to write data to LCD
    if ((millis() - oldTime) > 500 ) {
      writeToLCD();
      oldTime = millis();      
    }
}










