#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

#define SERIAL_DEBUG 0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PAYLOAD_SIZE 32000  // Define a size for the payload buffer

#define MAX_PAGES_REQUEST 50

#define UPDATE_INTERVAL 60000 * 60  // 1 min * x mins
#define SCREENSAVER_TIME 20000

// Change those values to match your campus
#define CAMPUS_ID 60
#define POOL_YEAR 2022


const char* host = "https://api.intra.42.fr";
const char* tokenEndPoint = "/oauth/token";
const char* uid   = "";
const char* secret = "";

const char* user_id = "";
const char* urlApi = "https://api.intra.42.fr/v2/";

const char* ssidList[] = {""};  // List of SSIDs to try
const char* passList[] = {""};
const int numSSIDs = sizeof(ssidList) / sizeof(ssidList[0]);  // Number of SSIDs in the list

char payload[PAYLOAD_SIZE];

HTTPClient http;
String accessToken;
String formattedDate;

bool tokenObtained = false;
bool wifiInternet = false;
int wifiConnect = 0;

unsigned long lastUpdate = 0;
unsigned long lastDisplay = 0;
unsigned long lastPushTime = 0;
unsigned long lastInteraction = 0;

char logins[500][10];
char blackholes[500][26];
int nb_datas = 0;

int daysLeft;
int hoursLeft;
int minsLeft;

int firstLine = 1;

int lastStatePush1 = 0;
int lastStatePush2 = 0;
int screenSaver = 0;

void setup() {
  Serial.begin(115200);
  btStop();

  pinMode(19, INPUT_PULLUP);
  pinMode(23, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  display.setTextSize(2); 
  display.setCursor(0, 10); 
  display.write("Blackhole");
  display.setCursor(0, 35); 
  display.write("Analyser");
  display.display();
  delay(3000);

  wifi_start();
}

void loop() {
  if (millis() - lastUpdate > UPDATE_INTERVAL || lastUpdate == 0){
    update_launcher();
    lastInteraction = millis();
  }
  checkButtons();
  if (millis() - lastInteraction > SCREENSAVER_TIME){
    draw_solar_system();
  } else {
    if (millis() - lastDisplay > 60000)
      displayDatas();
  }
}

void update_launcher(){
  wifiConnect = 1;
  display.clearDisplay();
  dispSetTextBlack(" ** Updating datas **", 0, 0, 1, 1);
  while (!wifiInternet){delay(500);}
  if (!tokenObtained)
    get_token();
  if (tokenObtained){
    refresh_datas();
    displayDatas();
    lastUpdate = millis();
  }
  wifiConnect = 0;
}

void checkButtons(){
  if (!digitalRead(23)){
    lastInteraction = millis();
    if (lastStatePush1 == 0)
      lastPushTime = millis();
    if (lastStatePush1 == 0 || millis() - lastPushTime > 500){
      firstLine--;
      if (firstLine < 1) firstLine = 1;
      displayDatas();
    }
    lastStatePush1 = 1;
  } else {
    lastStatePush1 = 0;
  }
  if (!digitalRead(19)){
    lastInteraction = millis();
    if (lastStatePush2 == 0)
      lastPushTime = millis();
    if (lastStatePush2 == 0 || millis() - lastPushTime > 500){
      firstLine++;
      if (firstLine > nb_datas - 7) firstLine = nb_datas - 7;
      displayDatas();
    }
    lastStatePush2 = 1;
  } else {
    lastStatePush2 = 0;
  }
}

void dispSetText(const char *text, int x, int line, int clearLine, int disp){
  if (clearLine)
    display.fillRect(0, line * 9, display.width(), 9, BLACK); 
  display.setTextSize(1);
  display.setCursor(x, line * 9);
  display.write(text);
  if (disp) display.display();
}

void dispSetTextBlack(const char *text, int x, int line, int clearLine, int disp){
  if (clearLine)
    display.fillRect(0, line * 9, display.width(), 10, WHITE); 
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(x, line * 9 + 1);
  display.write(text);
  display.setTextColor(WHITE);
  if (disp) display.display();
}

void displayTextRightJustified(const char *text, int x, int line, int clearLine) {
  int16_t x1, y1;
  uint16_t w, h;

  if (clearLine)
    display.fillRect(0, line * 9, display.width(), 9, BLACK); 
  display.setTextSize(1);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  x = x - w;
  display.setCursor(x, line * 9);
  display.write(text);
}

void refresh_datas(){
  get_all_users_in_campus();
  display.clearDisplay();
  dispSetTextBlack(" ** Updating datas **", 0, 0, 1, 1);
  get_all_blackhole();
  sortDates();
  if (SERIAL_DEBUG) {
    Serial.println("\n\nLOGIN LIST > BLACKHOLES\n");
    for (int i = 0; i < nb_datas; i++){
      Serial.print(logins[i]);
      Serial.print(" > ");
      Serial.println(blackholes[i]);
    }
  }
}

void displayDatas(){
  char cstr[10];

  display.clearDisplay();
  for (int i = 0; i <= 6; i++){
    dispSetText(logins[firstLine + i], 0, i, 1, 0);
    refTimeRemaining(blackholes[firstLine + i]);
    displayTextRightJustified(itoa(daysLeft, cstr, 10), 79, i, 0); 
    dispSetText("j", 80, i, 0, 0);
    displayTextRightJustified(itoa(hoursLeft, cstr, 10), 100, i, 0); 
    dispSetText("h", 101, i, 0, 0);
    displayTextRightJustified(itoa(minsLeft, cstr, 10), 121, i, 0); 
    dispSetText("m", 122, i, 0, 0);
  }
  display.display();
  lastDisplay = millis();
}

void removeOneArrayLine(int lineNb){
  int c;

  int i = lineNb;
  while (i < nb_datas){
    c = 0;
    while (logins[i + 1][c]){
      logins[i][c] = logins[i + 1][c];
      c++;
    }
    logins[i][c] = '\0';
    i++;
  }
  nb_datas--;
}

void refTimeRemaining(const char* timestampStr) {
  struct tm futureTime;
  strptime(timestampStr, "%Y-%m-%dT%H:%M:%S.000Z", &futureTime); // Parse the timestamp
  time_t future = mktime(&futureTime); // Convert the tm structure to time_t

  time_t now;
  time(&now); // Get the current time as time_t

  double secondsRemaining = difftime(future, now); // Get the difference in seconds

  int days = secondsRemaining / (60 * 60 * 24);
  secondsRemaining -= days * (60 * 60 * 24);
  int hours = secondsRemaining / (60 * 60);
  secondsRemaining -= hours * (60 * 60);
  int minutes = secondsRemaining / 60;
  secondsRemaining -= minutes * 60;

  daysLeft = days;
  hoursLeft = hours;
  minsLeft = minutes;
}

void sortDates() {
  for (int i = 0; i < nb_datas - 1; i++) {
    for (int j = 0; j < nb_datas - i - 1; j++) {
      if (strcmp(blackholes[j], blackholes[j + 1]) > 0) {
        char tempLogin[16];
        char tempDate[26];
        
        strcpy(tempLogin, logins[j]);
        strcpy(tempDate, blackholes[j]);
        
        strcpy(logins[j], logins[j + 1]);
        strcpy(blackholes[j], blackholes[j + 1]);
        
        strcpy(logins[j + 1], tempLogin);
        strcpy(blackholes[j + 1], tempDate);
      }
    }
  }
}

void get_all_blackhole(){
  int i;
  int user = 0;
  char *pos;
  char cstr[10];
  unsigned long userGetSumTime = 0;
  unsigned long beginTime;
  int nbUserGetRequests = 0;
  int averageTime;

  if (SERIAL_DEBUG) Serial.println("\nGETTIN BLACKHOLES\n");
  dispSetText("Getting blackholes", 0, 2, 1, 1);
  while (user < nb_datas){
    beginTime = millis();
    if (SERIAL_DEBUG) Serial.printf("%d / %d : %s\n", user + 1, nb_datas, logins[user]);
    displayTextRightJustified(itoa(user + 1, cstr, 10), 20, 3, 1);
    dispSetText("/", 21, 3, 0, 0);
    dispSetText(itoa(nb_datas, cstr, 10), 27, 3, 0, 0);
    dispSetText(logins[user], 55, 3, 0, 0);
    // Progress bar
    display.fillRect(0, 37, 128, 10, WHITE);
    display.fillRect(1, 38, (126 * user) / nb_datas, 8, BLACK);
    // Time left
    if (nbUserGetRequests){
      displayTextRightJustified(itoa((userGetSumTime / nbUserGetRequests) * (nb_datas - user) / 1000, cstr, 10), 78, 6, 1);
      dispSetText("s left", 80, 6, 0, 0);
    }
    display.display();

    payload[0] = '\0';
    int retry = 0;
    while (payload[0] == '\0'){
      if (wifiInternet){
        if (retry < 5){
          get_payload((String(urlApi) + "users/" + logins[user]).c_str());
        } else {
          delay(10000);
        }
        if (payload[0] == '\0'){
          retry++;
          delay(500);
        }
      } else {
        delay(500);
      }
    }

    pos = strstr(payload, "blackholed_at");
    while (pos){
      if (pos[15] != 'n')
        break;
      pos += 20;
      pos = strstr(pos, "blackholed_at");
    }
    // dispSetText(logins[user], 0, 5, 1, 0);
    // dispSetText("", 0, 6, 1, 0);
    if (pos){
      pos += 16;
      for (i = 0; pos[i] != '\"'; i++) {  // Copy until the next quote
        blackholes[user][i] = pos[i];
      }
      blackholes[user][i] = '\0';
      if (SERIAL_DEBUG) Serial.print("    > ");
      if (SERIAL_DEBUG) Serial.println(blackholes[user]);
      // dispSetText(blackholes[user], 55, 5, 0, 1);
    } else {
      removeOneArrayLine(user);
      user--;
      if (SERIAL_DEBUG) Serial.println("    > No blackhole");
      // dispSetText("no data..", 55, 5, 0, 1);
    }
    userGetSumTime += millis() - beginTime;
    nbUserGetRequests++;
    user++;
  }
}

void get_all_users_in_campus(){
  char userLogin[15];
  char cstr[10];
  int i;
  int page = 1;

  dispSetText("", 0, 6, 1, 0);
  dispSetText("Getting users..", 0, 2, 1, 1);
  if (SERIAL_DEBUG) Serial.println();

  nb_datas = 0;
  while (page <= MAX_PAGES_REQUEST && strlen(payload) > 10 || page == 1){
    dispSetText(" Page", 0, 3, 1, 0);
    dispSetText(itoa(page, cstr, 10), 44, 3, 0, 1);

    payload[0] = '\0';
    while (payload[0] == '\0')
      get_payload((String(urlApi) + "campus/" + String(CAMPUS_ID) + "/users?filter[pool_year]=" + String(POOL_YEAR) + "&page[size]=20&page[number]=" + String(page)).c_str());
    
    char *start = payload;
    char *pos;
    char *admis;
    while ((pos = strstr(start, "login")) != NULL){
      pos += 8;
      for (i = 0; pos[i] != '\"'; i++){
        userLogin[i] = pos[i];
      }
      userLogin[i] = '\0';

      if (SERIAL_DEBUG) Serial.print(nb_datas);
      if (SERIAL_DEBUG) Serial.print(" ");
      if (SERIAL_DEBUG) Serial.print(userLogin);

      admis = strstr(pos, "active?");
      if (admis[9] == 't'){
        if (SERIAL_DEBUG) Serial.println("    active? TRUE");
        for (i = 0; userLogin[i]; i++){
          logins[nb_datas][i] = userLogin[i];
        }
        logins[nb_datas][i] = '\0';
        nb_datas++;
      } else {
        if (SERIAL_DEBUG) Serial.println("    active? FALSE");
      }
      start = pos;
    }
    if (SERIAL_DEBUG) Serial.print("Nb users: ");
    if (SERIAL_DEBUG) Serial.println(nb_datas);

    dispSetText("Total users: ", 0, 5, 1, 0);
    dispSetText(itoa(nb_datas, cstr, 10), 90, 5, 0, 1);

    delay(10);
    page++;
  }
}

void get_payload(const char* data_url){
  if (SERIAL_DEBUG) Serial.print("URL api request: ");
  if (SERIAL_DEBUG) Serial.println(data_url);
  http.begin(data_url);
  http.addHeader("Authorization", String("Bearer ") + accessToken);
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200){
    String response = http.getString();
    response.toCharArray(payload, PAYLOAD_SIZE);
  } else {
    if (SERIAL_DEBUG) Serial.printf("Http error: %d\n", httpResponseCode);
    payload[0] = '\0';
  }
  http.end();
}

void get_token(){
  dispSetText("Getting token..", 0, 6, 1, 1);
  // Create Http Client
  String url = String(host) + String(tokenEndPoint);
  if (SERIAL_DEBUG) Serial.println(url);
  String body = "grant_type=client_credentials&client_id=" + String(uid) + "&client_secret=" + String(secret);
  if (SERIAL_DEBUG) Serial.println(body);
  int httpResponseCode;
  // Connect to server
  http.begin(url);
  // Send token request
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  httpResponseCode = http.POST(body);
  if (httpResponseCode == HTTP_CODE_OK){
    DynamicJsonDocument json(1024);
    deserializeJson(json, http.getString());
    accessToken = json["access_token"].as<String>();
    if (SERIAL_DEBUG) Serial.println(accessToken);
    json.clear();
    tokenObtained = true;
    dispSetText("Token OK !", 0, 6, 1, 1);
  }
  if (SERIAL_DEBUG) Serial.println(httpResponseCode);
  http.end();
}

bool connectToWirelessAccessPoint() {
  WiFi.mode(WIFI_STA);
  dispSetText("Connect: ", 0, 6, 0, 0);
  displayTextRightJustified(ssidList[0], 127, 6, 0);
  display.display();
  if (SERIAL_DEBUG) Serial.println("> Wifi / Connect to ssid");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();
    WiFi.begin(ssidList[i], passList[i]);  // Use the appropriate password for each SSID
    delay(1000);
    int j = 0;
    while (WiFi.status() != WL_CONNECTED && j++ < 20)
      delay(100);
    if (SERIAL_DEBUG) Serial.println("Retry..");
  }
  if (SERIAL_DEBUG) Serial.print("Connected to ");
  if (SERIAL_DEBUG) Serial.println(ssidList[i]);
  dispSetText("Connected !", 0, 6, 1, 1);

  delay(100);
  const char *tz = "CET-1CEST,M3.5.0,M10.5.0/3";
  configTime(0, 0, "pool.ntp.org"); 
  // configTimeWithTz(tz, "pool.ntp.org");
  setenv("TZ", tz, 1); // Set environment variable with your time zone
  tzset();

  delay(100);

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  if (SERIAL_DEBUG) Serial.println(&timeinfo, "%b %d %Y %H:%M:%S");
  delay(100);

  return true;
}

void wifi_start() {
  xTaskCreatePinnedToCore(wifi_task, "wifi_task", 3 * 1024, NULL, 10, NULL, 0);
}

void wifi_task(void *arg) {
  while (1){
    if (WiFi.status() != WL_CONNECTED && wifiConnect){
      delay(500);
      wifiInternet = false;
      connectToWirelessAccessPoint();
      if (WiFi.status() == WL_CONNECTED)
        wifiInternet = true;
    }
    if (WiFi.status() == WL_CONNECTED && !wifiConnect){
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      wifiInternet = false;
    }
    delay(1000);
  }
  vTaskDelete(NULL);
}

void draw_solar_system(){
  static float angle = 132.0;
  // unsigned long start = millis();
  // while (millis() - start < 5000){
    display.clearDisplay();

    // Draw orbits
    drawScaledCircle(20, 0.5); // Orbit for Mercury
    drawScaledCircle(30, 0.5); // Orbit for Venus
    drawScaledCircle(40, 0.5); // Orbit for Earth
    drawScaledCircle(50, 0.5); // Orbit for Mars
    // Add more orbits for other planets...

    // Draw planets on orbits
    drawPlanetOnOrbit(20, 10, angle, 2); // Mercury
    drawPlanetOnOrbit(20, 10, angle + 160, 2); // Mercury

    drawPlanetOnOrbit(30, 15, angle * 0.7, 3); // Venus
    drawPlanetOnOrbit(30, 15, angle * 0.7 + 100, 2);
    drawPlanetOnOrbit(30, 15, angle * 0.7 + 220, 3);

    drawPlanetOnOrbit(40, 20, angle * 0.4, 3); // Earth
    drawPlanetOnOrbit(40, 20, angle * 0.4 + 160, 2); // Earth

    drawPlanetOnOrbit(50, 25, angle * 0.2, 2); // Mars
    drawPlanetOnOrbit(50, 25, angle * 0.2 + 110, 3); // Mars
    drawPlanetOnOrbit(50, 25, angle * 0.2 + 220, 2); // Mars
    // Add more planets...

    angle += 2.5; // Increment the angle for the next frame

    display.display();
    //delay(50); // Delay to control frame rate
  // }
}

void drawScaledCircle(int radius, float scaleY) {
  int steps = 50; // Increase for smoother circles
  float theta = 0.0;
  for (int i = 0; i < steps; i++) {
    float x1 = radius * cos(theta);
    float y1 = radius * sin(theta) * scaleY;
    float x2 = radius * cos(theta + TWO_PI / steps);
    float y2 = radius * sin(theta + TWO_PI / steps) * scaleY;
    display.drawLine(SCREEN_WIDTH / 2 + x1, SCREEN_HEIGHT / 2 + y1, SCREEN_WIDTH / 2 + x2, SCREEN_HEIGHT / 2 + y2, SSD1306_WHITE);
    theta += TWO_PI / steps;
  }
}

void drawPlanetOnOrbit(int semiMajorAxis, int semiMinorAxis, float angle, int planetSize) {
  float radAngle = radians(angle);
  int planetX = SCREEN_WIDTH / 2 + semiMajorAxis * cos(radAngle);
  int planetY = SCREEN_HEIGHT / 2 + semiMinorAxis * sin(radAngle);
  display.fillCircle(planetX, planetY, planetSize, SSD1306_WHITE);
}
