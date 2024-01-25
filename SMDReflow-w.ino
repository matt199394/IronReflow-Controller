#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "max6675.h"
//#include <Thermocouple.h>
//#include <MAX6675_Thermocouple.h>
 

#define PREHEAT_TIME 60   // Time heater ON needed to preheat to 150°
#define PREHEAT_WAIT 27   // Time heater OFF needed to reach preheat temp 150°
#define REFLOW_TIME 40    // Time heater ON to up temp to reflow temp (POT)
#define REFLOW_WAIT 16    // Time heater OFF needed to reach reflow temp (POT)
#define COOLDOWN_TIME 250 // Time heater OFF and FAN ON to cool heater

#define OLED_RESET 0  // GPIO0
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int thermoDO = 6;  //SO
int thermoCS = 4;  // CS
int thermoCLK = 5; // SCK

//#define SCK_PIN   6  //  SCK
//#define SO_PIN    4  //  SO
//#define CS_PIN    5  //  CS

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
//Thermocouple* thermocouple;

const int button = 8;
const int solidstate = 3;
const int poti = A0;
const int fan = 7; 

const int temp_preheat = 150;
const int temp_reflow = 240;

int tread =0;
int temp_now = 0;
int temp_next = 0;
int temp_poti = 0;
int temp_poti_old = 0;
String state[] = {"OFF", "PREHEAT", "RAMP", "REFLOW", "REFLOW-W", "COOLING"};
int state_now = 0;

int time_count = 0;
int perc = 0;

int offset = 0;


void setup() {
  Serial.begin(9600);
  //thermocouple = new MAX6675_Thermocouple(SCK_PIN, CS_PIN, SO_PIN);

  pinMode(button, INPUT);
  pinMode(solidstate, OUTPUT);
  pinMode(fan, OUTPUT);
  digitalWrite(solidstate, LOW);
  digitalWrite(fan, LOW);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.fillScreen(BLACK);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(18,0);
  display.println("SMD IRON");
  display.setCursor(20,20);
  display.println("REFLOW");
  //display.setTextSize(1);
  display.setCursor(20,40);
  display.println("STATION");
  display.display();
  delay(3000);
  display.clearDisplay();
  display.display();
 }

long t = millis();
long t_solder = millis();
long t_cool = millis();

void loop() {
  tread++;
  if (tread > 4) {
    temp_now = thermocouple.readCelsius();
    tread=0;
    Serial.print("temp:");
    Serial.println(temp_now);
  }
  
//  temp_now = thermocouple->readCelsius();
  
  temp_poti = map(analogRead(poti), 1023, 0, temp_preheat, temp_reflow);

  if (temp_poti != temp_poti_old) {
    int v = 0;
    display.fillScreen(WHITE);
    display.setTextColor(BLACK);
    display.setTextSize(1);
    display.setCursor(X(1, 7), Y(1, 0.1));
    display.println("PREHEAT");
    display.setTextSize(2);
    display.setCursor(X(2, 3), Y(2, 0.5));

    while (v < 100) {
      temp_poti = map(analogRead(poti), 1023, 0, temp_preheat, temp_reflow);
      if (temp_poti > temp_poti_old + 1 || temp_poti < temp_poti_old - 1) {
        display.fillScreen(WHITE);
        display.setTextSize(1);
        display.setCursor(X(1, 6), Y(1, 0.1));
        display.println("REFLOW");
        display.setTextSize(2);
        display.setCursor(X(2, 3), Y(2, 0.5));
        display.println(String(temp_poti));
        display.display();
        temp_poti_old = temp_poti;
        v = 0;
      }
      v++;
      delay(20);
    }
    temp_poti_old = temp_poti;
  }

  if (millis() > t + 200 || millis() < t) {
    PrintScreen(state[state_now], temp_next, temp_now, time_count, perc);
    t = millis();
  }


  if (digitalRead(button) == 0) {
    delay(100);
    int c = 0;
    while (digitalRead(button) == 0) {
      c++;
      if (c > 150) {
        digitalWrite(solidstate, LOW);
        state_now = 0;
        display.fillScreen(WHITE);
        display.setTextColor(BLACK);
        display.setTextSize(2);
        display.setCursor(X(2, 3), Y(2, 0.5));
        display.println("OFF");
        display.display();
        while (digitalRead(button) == 0) delay(1);
        return;
      }
      delay(10);
    }

    t_solder = millis();
    perc = 0,
    state_now++;
    if (state_now == 0) temp_next = 0;
    else if (state_now == 6) {
      state_now = 0;
      temp_next = 0;
    }
  }


  if (state_now == 1) { //PREHEAT
    temp_next = temp_preheat;
    regulate_temp(temp_now, temp_preheat);
    perc = int((float(temp_now) / float(temp_preheat)) * 100.00);
    time_count = ((t_solder/10)  + PREHEAT_TIME * 100 - (millis()/10)) / 100;
    if (time_count <= 0 || perc >= 100) {
      t_solder = millis();
      state_now = 2;
    }
  }
  else if (state_now == 2) { //PREHEAT WAIT
    temp_next = temp_preheat;
    perc = int((float(temp_now) / float(temp_preheat)) * 100.00);
    time_count = ((t_solder/10)  + PREHEAT_WAIT * 100 - (millis()/10)) / 100;
    digitalWrite(solidstate, LOW);
    if (time_count <= 0 || perc >= 100) {
      t_solder = millis();
      state_now = 3;
    }  
  }
  else if (state_now == 3) { //REFLOW
    temp_next = temp_poti ;
    regulate_temp(temp_now, temp_poti);
    perc = int((float(temp_now) / float(temp_poti)) * 100.00);
    time_count = ((t_solder/10)  + REFLOW_TIME * 100 - (millis()/10)) / 100;
    if (time_count <= 0 || perc >= 100) {
      state_now = 4;
      t_solder = millis();
    }
  }

  else if (state_now == 4) { //REFLAW WAIT
    temp_next = temp_poti ;
    perc = int((float(temp_now) / float(temp_poti)) * 100.00);
    time_count = ((t_solder/10)  + REFLOW_WAIT * 100 - (millis()/10)) / 100;
    digitalWrite(solidstate, LOW);
    if (time_count <= 0 || perc >= 100) {
      t_solder = millis();
      state_now = 5;
      perc = 0;
    }  
  }
  
  
  else if (state_now == 5) { //COOLING
    temp_next = 0;
    digitalWrite(solidstate, LOW);
    digitalWrite(fan, HIGH);

    time_count = ((t_solder/10)  + COOLDOWN_TIME * 100 - (millis()/10)) / 100;
    
    if (time_count <= 0) {
      state_now = 0;
      digitalWrite(fan, LOW);
    }
  }
  else {
    digitalWrite(solidstate, LOW);
    digitalWrite(fan, LOW);
    time_count = 0;
  }

  delay(30);
}




void regulate_temp(int temp, int should) {
  if (should <= temp - offset) {
    digitalWrite(solidstate, LOW);
  }
  else if (should > temp + offset) {
    digitalWrite(solidstate, HIGH);
  }
}


void PrintScreen(String state, int soll_temp, int ist_temp, int tim, int percentage) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(state);

  display.setCursor(80, 0);
  String str = String(soll_temp) + " deg";
  display.println(str);

  if (tim != 0) {
    display.setCursor(0, 50);
    str = String(tim) + " sec";
    display.println(str);
  }
  
  if (percentage != 0) {
    display.setCursor(80, 50);
    str = String(percentage) + " %";
    display.println(str);
  }

  display.setTextSize(2);
  display.setCursor(30, 22);
  str = String(ist_temp) + " deg";
  display.println(str);

  display.display();
}


int X(int textgroesse, int n) {
  //gibt die X koordinate aus, damit text mit n zeichen mittig ist

  return (0.5 * (display.width() - textgroesse * (6 * n - 1)));
}//end int X

int Y(int textgroesse, float f) {
  //gibt die Y koordinate aus, damit text mittig ist

  return (f * display.height() - (textgroesse * 4));
}//end int Y
