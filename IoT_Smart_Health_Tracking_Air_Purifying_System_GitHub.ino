/*
 * IoT Based Smart Health Tracking and Air-Purifying System
 * for Hazardous Environment Monitoring
 *
 * Controller : ESP32
 * IoT        : Blynk
 * Sensors    : MQ135, MQ7, DHT11, MAX30100
 *
 * NOTE:
 * Keep real Wi-Fi/Blynk credentials out of public GitHub repositories.
 */

#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_BLYNK_TEMPLATE_NAME"
#define BLYNK_PRINT Serial
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp.h>
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define MQ1_PIN
#define MQ7_PIN
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define BUZZER_PIN 25 // Active buzzer → GPIO25 → GND
#define FAN_PIN 26 // BC547 Base via 1kΩ → GPIO26
#define MQ1_THRESHOLD_PPM 500.0f // Poor air quality above 0 ppm
#define MQ7_THRESHOLD_PPM .0f // Dangerous CO above 10 ppm
#define TEMP_THRESHOLD .0f // Fever above °C
#define SDA_PIN 21
#define SCL_PIN 22
#define MAX30100_I2C_ADDRESS 0x57
#define REG_FIFO_WR_PTR 0x02
#define REG_OVRFLOW_CTR 0x03
#define REG_FIFO_RD_PTR 0x04
#define REG_FIFO_DATA 0x05
#define REG_MODE_CONFIG 0x06
#define REG_SPO2_CONFIG 0x07
#define REG_LED_CONFIG 0x09
#define REG_PART_ID 0xFF
#define FINGER_THRESHOLD 30000
DHT dht(DHT_PIN, DHT_TYPE);float mq1ToPPM(int raw) {float ppm = ((float)raw / 95.0f) * 1000.0f;return constrain(ppm, 0.0f, 1000.0f);}
float mq7ToPPM(int raw) {float ppm = ((float)raw / 95.0f) * 100.0f;return constrain(ppm, 0.0f, 100.0f);}float mq1ToPercent(float ppm) {return constrain((ppm / 1000.0f) * 100.0f, 0.0f, 100.0f);}
float mq7ToPercent(float ppm) {return constrain((ppm / 100.0f) * 100.0f, 0.0f, 100.0f);}
void writeRegister(uint8_t reg, uint8_t value) {Wire.beginTransmission(MAX30100_I2C_ADDRESS);Wire.write(reg);Wire.write(value);Wire.endTransmission();}
uint8_t readRegister(uint8_t reg) {Wire.beginTransmission(MAX30100_I2C_ADDRESS);Wire.write(reg);Wire.endTransmission(false);Wire.requestFrom(MAX30100_I2C_ADDRESS, 1);return Wire.read();}
void readFIFO(uint16_t &ir, uint16_t &red) {Wire.beginTransmission(MAX30100_I2C_ADDRESS);Wire.write(REG_FIFO_DATA);Wire.endTransmission(false);Wire.requestFrom(MAX30100_I2C_ADDRESS, 4);uint8_t b0 = Wire.read(), b1 = Wire.read();uint8_t b2 = Wire.read(), b3 = Wire.read();red = ((uint16_t)b0 << 8) | b1;ir = ((uint16_t)b2 << 8) | b3;}
void initMAX30100() {writeRegister(REG_MODE_CONFIG, 0x);delay(100);writeRegister(REG_MODE_CONFIG, 0x03);writeRegister(REG_SPO2_CONFIG, 0x47);writeRegister(REG_LED_CONFIG, 0xFF);writeRegister(REG_FIFO_WR_PTR, 0x00);writeRegister(REG_OVRFLOW_CTR, 0x00);writeRegister(REG_FIFO_RD_PTR, 0x00);}struct DCFilter {float w = 0;float step(float x) {float pw = w;w = x + 0.95f * w;return w - pw;}};
struct MeanFilter {float buf[10] = {0};
int idx = 0;float step(float x) {buf[idx] = x;idx = (idx + 1) % 10;float s = 0;for (int i = 0; i < 10; i++) s += buf[i];return s / 10.0f;}};
struct BeatDetector {float threshold = 20.0f, prev = 0;bool rising = false;long lastBeat = 0;float bpmBuffer[8] = {0};
int bpmIdx = 0, bpmCount = 0;float avgBPM = 0;void step(float value, long now) {if (value > prev) rising = true;if (rising && value < prev && prev > threshold) {rising = false;threshold = prev * 0.6f;if (lastBeat > 0) {float bpm = 60000.0f / (float)(now - lastBeat);if (bpm >=  && bpm <= 200) {bpmBuffer[bpmIdx % 8] = bpm;bpmIdx++;if (bpmCount < 8) bpmCount++;float sum = 0;for (int i = 0; i < bpmCount; i++) sum += bpmBuffer[i];avgBPM = sum / bpmCount;}}lastBeat = now;}threshold *= 0.99f;if (threshold < 5.0f) threshold = 5.0f;prev = value;}void reset() {avgBPM = 0; bpmCount = 0; bpmIdx = 0;lastBeat = 0; threshold = 20.0f;for (int i = 0; i < 8; i++) bpmBuffer[i] = 0;}};
struct SpO2Calculator {float irDC = 0, redDC = 0, spo2 = 0;float spo2Buffer[8] = {0};
int idx = 0, count = 0;void step(float ir, float red) {irDC = irDC * 0.95f + ir * 0.05f;redDC = redDC * 0.95f + red * 0.05f;if (irDC < 1000 || redDC < 1000) return;float irAC = ir - irDC, redAC = red - redDC;float R = (redAC / redDC) / (irAC / irDC);float sp = 110.0f - 25.0f * R;if (sp > 100) sp = 100;if (sp < 80) return;spo2Buffer[idx % 8] = sp;idx++;if (count < 8) count++;float sum = 0;for (int i = 0; i < count; i++) sum += spo2Buffer[i];spo2 = sum / count;}void reset() {spo2 = 0; idx = count = 0; irDC = redDC = 0;}};
DCFilter irDCFilter;MeanFilter irMeanFilter;BeatDetector beatDetector;SpO2Calculator spo2Calc;bool fingerOn = false;int noFingerCount = 0;uint_t lastPrint = 0;uint8_t lcdPage = 0;uint_t lastLCDSwitch = 0;uint_t lastBlynkSend = 0;float g_mq1ppm = 0, g_mq7ppm = 0;float g_mq1pct = 0, g_mq7pct = 0;float g_temp = 0, g_hum = 0;bool g_alert = false, g_fanOn = false;
void setup() {Serial.begin(115200);delay(1000);pinMode(BUZZER_PIN, OUTPUT);digitalWrite(BUZZER_PIN, LOW);pinMode(FAN_PIN, OUTPUT);digitalWrite(FAN_PIN, LOW); // Fan OFF initiallyWire.begin(SDA_PIN, SCL_PIN);Wire.setClock(0000);lcd.init();lcd.backlight();lcd.setCursor(0, 0); lcd.print("Air Quality Mon.");lcd.setCursor(0, 1); lcd.print("Connecting WiFi.");delay(500);dht.begin();WiFi.begin(ssid, pass);Serial.print("Connecting WiFi");int tries = 0;while (WiFi.status() != WL_CONNECTED && tries < 20) {delay(500); Serial.print("."); tries++;}if (WiFi.status() == WL_CONNECTED) {Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());Blynk.config(BLYNK_AUTH_TOKEN);Blynk.connect();lcd.clear();lcd.setCursor(0, 0); lcd.print("WiFi Connected!");lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());delay(1500);} else {Serial.println("\nWiFi Failed - Offline mode.");lcd.clear();lcd.setCursor(0, 0); lcd.print("WiFi Failed!");lcd.setCursor(0, 1); lcd.print("Offline Mode");delay(1500);}if (readRegister(REG_PART_ID) != 0x11) {Serial.println("MAX30100 not found!");lcd.clear(); lcd.setCursor(0, 0); lcd.print("MAX30100 Error!");while (1);}initMAX30100();lcd.clear();lcd.setCursor(0, 0); lcd.print("System Ready!");lcd.setCursor(0, 1); lcd.print("Place finger...");delay(1000);lcd.clear();Serial.println("System Ready.");}
void loop() {if (Blynk.connected()) Blynk.run();uint16_t irRaw, redRaw;readFIFO(irRaw, redRaw);if (irRaw <= FINGER_THRESHOLD) {if (++noFingerCount > 50 && fingerOn) {fingerOn = false;beatDetector.reset();spo2Calc.reset();Serial.println("No finger detected.");}} else {noFingerCount = 0;fingerOn = true;float irFiltered = irDCFilter.step((float)irRaw);float irSmooth = irMeanFilter.step(irFiltered);beatDetector.step(irSmooth, millis());spo2Calc.step((float)irRaw, (float)redRaw);}if (millis() - lastPrint >= 1000) {lastPrint = millis();int mq1Raw = analogRead(MQ1_PIN);int mq7Raw = analogRead(MQ7_PIN);g_mq1ppm = mq1ToPPM(mq1Raw);g_mq7ppm = mq7ToPPM(mq7Raw);g_mq1pct = mq1ToPercent(g_mq1ppm);g_mq7pct = mq7ToPercent(g_mq7ppm);g_temp = dht.readTemperature();g_hum = dht.readHumidity();bool mq1Alert = (g_mq1ppm > MQ1_THRESHOLD_PPM);bool mq7Alert = (g_mq7ppm > MQ7_THRESHOLD_PPM);bool tempAlert = (!isnan(g_temp) && g_temp > TEMP_THRESHOLD);g_alert = mq1Alert || mq7Alert || tempAlert;if (g_alert) {digitalWrite(BUZZER_PIN, HIGH); delay(150);digitalWrite(BUZZER_PIN, LOW); delay(100);digitalWrite(BUZZER_PIN, HIGH); delay(150);digitalWrite(BUZZER_PIN, LOW);} else {digitalWrite(BUZZER_PIN, LOW); }bool gasAlert = mq1Alert || mq7Alert;if (gasAlert) {digitalWrite(FAN_PIN, HIGH); // Fan ON when gas detectedg_fanOn = true;} else {digitalWrite(FAN_PIN, LOW); // Fan OFF when normalg_fanOn = false; }Serial.println("========== SENSOR DATA ==========");if (!fingerOn) {Serial.println("Heart Rate: Place finger...");Serial.println("SpO2 : Place finger...");} else {Serial.print("Heart Rate: ");beatDetector.avgBPM > 0 ? Serial.print(beatDetector.avgBPM, 1), Serial.println(" BPM"): Serial.println("Calculating...");Serial.print("SpO2 : ");spo2Calc.spo2 > 0 ? Serial.print(spo2Calc.spo2, 1), Serial.println(" %"): Serial.println("Calculating...");}Serial.print("MQ1 Air : "); Serial.print(g_mq1ppm, 1);Serial.print(" ppm ("); Serial.print(g_mq1pct, 1); Serial.println("%)");Serial.print("MQ7 CO : "); Serial.print(g_mq7ppm, 1);Serial.print(" ppm ("); Serial.print(g_mq7pct, 1); Serial.println("%)");if (!isnan(g_temp)) {Serial.print("Temp : "); Serial.print(g_temp, 1); Serial.println(" C");Serial.print("Humidity : "); Serial.print(g_hum, 1); Serial.println(" %");} else {Serial.println("DHT11 : Read Error");}Serial.print("ALERT : "); Serial.println(g_alert ? "YES ⚠" : "No");Serial.print("FAN : "); Serial.println(g_fanOn ? "ON" : "OFF");Serial.println("=================================");if (millis() - lastLCDSwitch >= 2000) {lastLCDSwitch = millis();lcd.clear();switch (lcdPage) {case 0: // HR & SpO2lcd.setCursor(0, 0);if (!fingerOn) {lcd.print("Place finger...");} else {lcd.print("HR: ");if (beatDetector.avgBPM > 0) {lcd.print((int)beatDetector.avgBPM);lcd.print(" BPM");} else {lcd.print("Calc...");} }lcd.setCursor(0, 1);lcd.print("SpO2: ");if (spo2Calc.spo2 > 0) {lcd.print(spo2Calc.spo2, 1); lcd.print("%");} else {lcd.print("Calc...");}break;case 1: // Temp & Humiditylcd.setCursor(0, 0);lcd.print("Temp: ");if (!isnan(g_temp)) {lcd.print(g_temp, 1); lcd.print("C");if (g_temp > TEMP_THRESHOLD) lcd.print(" !");} else { lcd.print("Error"); }lcd.setCursor(0, 1);lcd.print("Hum : ");if (!isnan(g_hum)) {lcd.print(g_hum, 1); lcd.print("%");} else { lcd.print("Error"); }break;case 2: // MQ1, MQ7 Air Qualitylcd.setCursor(0, 0);lcd.print("Air: ");lcd.print(g_mq1ppm, 0); lcd.print(" ppm");if (g_mq1ppm > MQ1_THRESHOLD_PPM) lcd.print("!");lcd.setCursor(0, 1);lcd.print("CO : ");lcd.print(g_mq7ppm, 0); lcd.print("ppm");break;case 3: // Alert & Fan statuslcd.setCursor(0, 0);lcd.print(g_alert ? "!! ALERT !!" : "Status:Normal ");lcd.setCursor(0, 1);lcd.print("Fan:");lcd.print(g_fanOn ? "ON " : "OFF ");lcd.print("Buz:");lcd.print(g_alert ? "ON" : "OF");break;}lcdPage = (lcdPage + 1) % 4;}if (Blynk.connected() && millis() - lastBlynkSend >= 2000) {lastBlynkSend = millis();Blynk.virtualWrite(V0, beatDetector.avgBPM > 0 ? beatDetector.avgBPM : 0);Blynk.virtualWrite(V1, spo2Calc.spo2 > 0 ? spo2Calc.spo2 : 0);Blynk.virtualWrite(V2, !isnan(g_temp) ? g_temp : 0);Blynk.virtualWrite(V3, !isnan(g_hum) ? g_hum : 0);Blynk.virtualWrite(V4, g_mq1ppm);Blynk.virtualWrite(V5, g_mq7ppm);Blynk.virtualWrite(V6, g_alert ? 1 : 0);Blynk.virtualWrite(V7, g_fanOn ? 1 : 0);}}delay(10);}