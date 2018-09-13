#include <Adafruit_NeoPixel.h>
#include <Servo.h>
#include <Wire.h>
#include <WiFi101.h>
#include <SPI.h>
#include <TelegramBot.h>
#include "arduino_secrets.h"
#include "Adafruit_TCS34725.h"
#include "Perceptron.h"

#define PIN 3
#define NUMPIXELS 12
#define GREEN A1
#define RED A2
#define BLUE A3

char ssid[] = SECRET_SSID;             // your network SSID (name)
char pass[] = SECRET_PASS;
const char BotToken[] = SECRET_BOT_TOKEN;

WiFiSSLClient client;
TelegramBot bot (BotToken, client);
TelegramKeyboard keyboard_one;
float guess = 1;

Servo myservo;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


int potpin = A1;  // analog pin used to connect the potentiometer
int val;    // variable to read the value from the analog pin
int defaultPos = 70;
int notesLoose[] = {2*261,2*246,2*233,2*220,2*207};
int tempoLoose[] = {200,250,300,400,800};
int pitchThinking[] = {523,0,440,0};
int tempoThinking[] = {100,200,100,200};
float totalAttemps = 0.0, badAttemps = 0.0, accuracy = 0.0;


uint16_t r, g, b, c;

//we will use one perceptron with 4 inputs (Red, green, blue, clean)
perceptron colorPerceptron(5);//fifth is for bias

void setup() {
  pixels.begin();
  pixels.setBrightness(50);
  turnOff();

  myservo.attach(2);  // attaches the servo on pin 9 to the servo object
  pinMode(9, INPUT_PULLUP);

  myservo.write(defaultPos);

  randomSeed(analogRead(A0));
  colorPerceptron.randomize();  //weight initialization

  Serial.begin(115200);

  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  //Search for 
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("WiFi connected");

  //Create the menu for the Telegram Bot
  const char* row_one[] = {"Wrong"};
  const char* row_two[] = {"Accuracy"};
  const char* row_three[] = {"Bad Attemps", "Good Attemps", "Total Attemps"};
  const char* row_four[] = {"What is your name?", "Cual es su nombre?"};
  const char* row_five[] = {"What do you sort?", "Que ordenas?"};
  keyboard_one.addRow(row_one, 1);
  keyboard_one.addRow(row_two, 1);
  keyboard_one.addRow(row_three, 3);
  keyboard_one.addRow(row_four, 2);
  keyboard_one.addRow(row_five, 2);
  bot.begin();
}

void loop() {
  uint16_t rtemp, gtemp, btemp, ctemp;
  tcs.getRawData(&rtemp, &gtemp, &btemp, &ctemp);
  delay(100);
  Serial.println(ctemp);
  if (ctemp > 2500) {
    totalAttemps++;

    tcs.getRawData(&r, &g, &b, &c);
    delay(60);

    /*** store in perceptron inputs ***/
    colorPerceptron.inputs[0] = r;
    colorPerceptron.inputs[1] = g;
    colorPerceptron.inputs[2] = b;
    colorPerceptron.inputs[3] = c;

    //make a guess
    guess = colorPerceptron.feedForward();
    spinWheel(36);
    turnOff();

    if (guess == 1) {
      myservo.write(defaultPos + 45); //Turn right
      turnOn(10, 6, 1);
      delay(250);
      turnOff();
    }
    else {
      myservo.write(defaultPos - 45); //Turn Left
      turnOn(4, 6, 1);
      delay(250);
      turnOff();
    }
    delay(1000);
  }

  //Ask if there is any message send it from telegram asking for something
  telegram(guess);

  //Press button if guess incorrect
  if (digitalRead(9)) {
    turnOn(0, 12, 2);
    turnOff();
    for (int i = 0; i < sizeof(notesLoose)/sizeof(int); i++) {
      tone(7,notesLoose[i],tempoLoose[i]);
      delay(tempoLoose[i]);
      noTone(7);
    }
    //Serial.println("Boton");
    badAttemps++;
    colorPerceptron.train(-guess, guess);
    delay(1000);
  }

  myservo.write(defaultPos); //Se mueve a la posicion neutral

  Serial.print("Good attemps: "); Serial.println(totalAttemps - badAttemps);
  Serial.print("Bad attemps: "); Serial.println(badAttemps);
  Serial.print("Total attemps: "); Serial.println(totalAttemps);

  accuracy = ( (totalAttemps - badAttemps) / totalAttemps ) * 100;
  Serial.print("Accuracy: "); Serial.println(accuracy);
}

void telegram(float g) {
  message m = bot.getUpdates(); // Read new messages
  //Serial.println("Waiting message from telegram");
  String texto = m.text;
  texto.toUpperCase();
  if ( m.chat_id != 0 ) { // Checks if there are some updates
    Serial.println(texto);
    if (texto.equals("WRONG")) {
      turnOn(0, 12, 2);
      turnOff();
      for (int i = 0; i < sizeof(notesLoose)/sizeof(int); i++) {
        tone(7,notesLoose[i],tempoLoose[i]);
        delay(tempoLoose[i]);
        noTone(7);
      }
      Serial.println("Boton");
      badAttemps++;
      colorPerceptron.train(-guess, guess);
    } else if (texto.equals("ACCURACY")) {
      bot.sendMessage(m.chat_id, "Accuracy = " + String(accuracy), keyboard_one);
    } else if (texto.equals("BAD ATTEMPS")) {
      bot.sendMessage(m.chat_id, "Bad Attemps = " + String(badAttemps), keyboard_one);
    } else if (texto.equals("GOOD ATTEMPS")) {
      bot.sendMessage(m.chat_id, "Good Attemps = " + String(totalAttemps - badAttemps), keyboard_one);
    } else if (texto.equals("TOTAL ATTEMPS")) {
      bot.sendMessage(m.chat_id, "Total Attemps = " + String(totalAttemps), keyboard_one);
    } else if (texto.equals("WHAT IS YOUR NAME?") || texto.equals("CUAL ES SU NOMBRE?")) {
      bot.sendMessage(m.chat_id, "My name is SortaBot - Me llamo SortaBot", keyboard_one);
    } else if (texto.equals("QUE ORDENAS?") || texto.equals("WHAT DO YOU SORT?")) {
      bot.sendMessage(m.chat_id, "I sort Lego - Ordeno legos", keyboard_one);
    }
  }

uint16_t distanciaEucl(uint16_t r, uint16_t g, uint16_t b) {
  return sqrt(r * r + g * g + b * b);
}

void spinWheel(int vueltas) {
  for (int i = 0; i < sizeof(pitchThinking)/sizeof(int); i++) {
      tone(7,pitchThinking[i],tempoThinking[i]);
      delay(tempoThinking[i]);
      noTone(7);
    }
    
  for (int i = 0; i < vueltas; i++) { //3-4 es arriba
    int j = i % 12;
    int k = i % 4;
    pixels.setPixelColor(j, 50, 50, 50);
    pixels.setPixelColor(j + 1, 100, 100, 100);
    pixels.setPixelColor(j + 2, 250, 250, 250);
    pixels.show();
    delay(35);
    pixels.setPixelColor(j, 0, 0, 0);
    tone(7,pitchThinking[k],tempoThinking[k]);
    delay(tempoThinking[k]);
    noTone(7);
  }
  turnOff();
}

void turnOff() {
  for (int i = 0; i < NUMPIXELS; i++) { //3-4 es arriba
    pixels.setPixelColor(i, 0, 0, 0);
  }
  pixels.show();
}

void turnOn(int neoPixelStart, int neoPixelEnd, int color) {
  uint32_t c;
  switch (color) {
    case 1:
      c = pixels.Color(250, 250, 250);
      break;
    case 2:
      c = pixels.Color(250, 0, 0);
      break;
  }
  for (int i = neoPixelStart; i < neoPixelStart + neoPixelEnd; i++) { //3-4 es arriba
    int j = i % 12;
    pixels.setPixelColor(j, c);
    pixels.show();
    delay(50);
  }
}
