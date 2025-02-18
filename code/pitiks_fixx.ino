#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <RTClib.h>
#include <DHT.h>
#include <Ultrasonic.h>
#include <ESP32Servo.h>
#include <FirebaseESP32.h>

#define DHT_PIN 25
#define TRIG_PIN 33
#define ECHO_PIN 32
#define RELAY_PIN 26
#define SERVO_PIN 14

char ssid[] = "ZTE_2.4G_rh44JS";
char password[] = "1234567890";

#define FIREBASE_HOST "pitiks-c49b7-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "y0AyhGNbDb57FUFmOOB02rxJqG226H0uGhsm5SJq"

WiFiUDP ntpUDP;
DHT dht(DHT_PIN, DHT11);
RTC_DS3231 rtc;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
Ultrasonic ultrasonic(TRIG_PIN, ECHO_PIN);
Servo servo;
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

int currentYear, currentMonth, currentDay, currentHour, currentMinute, currentSecond;
char dateStr[11];
int previousMillis = 0, servoValue, previousFirebase;
float feedPercentage, temperature, humidity;
bool servoRot = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  servo.attach(SERVO_PIN);
  WiFi.begin(ssid, password);
  pinMode(RELAY_PIN, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.println("Tidak terhubung ke jaringan WiFi...");
  }
  
  Serial.println("Terhubung ke jaringan WiFi!");

  timeClient.begin();
  timeClient.setTimeOffset(25200);
  rtc.begin();

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to default!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(millis() - previousMillis >= 1000){
  timeClient.update(); // Perbarui waktu dari server NTP
  unsigned long epochTime = timeClient.getEpochTime(); // Dapatkan waktu epoch dari NTP
  DateTime now = DateTime(epochTime); // Konversi waktu epoch ke DateTime
  currentYear = now.year();
  currentMonth = now.month();
  currentDay = now.day();
  currentHour = now.hour();
  currentMinute = now.minute();
  currentSecond = now.second();

  sprintf(dateStr, "%02d-%02d-%04d", currentDay, currentMonth, currentYear);

  Serial.print("Tanggal: ");
  Serial.print(dateStr);
  Serial.print(" ");
  Serial.print("waktu saat ini: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.print(":");
  Serial.println(currentSecond);
  previousMillis = millis();
  }

  //Suhu
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.print(" °C, Kelembaban: ");
  Serial.print(humidity);
  Serial.println(" %");
  if (temperature < 30 || humidity > 60) {
    digitalWrite(RELAY_PIN, LOW); // Nyalakan relay jika suhu kurang dari 20°C atau kelembaban lebih dari 60%
    Serial.println("Lampu Menyala!"); // Tambahkan ini
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Matikan relay jika tidak memenuhi kondisi
  }

  //Pakan
  float distance = ultrasonic.read();
  
  // Hitung persentase sisa pakan
  feedPercentage = 100 - (distance / 10) * 100;
  Serial.print("Sisa Pakan : ");
  Serial.println(feedPercentage);
  
  // Cetak jarak yang terbaca ke Serial Monitor
  Serial.print("jarak pakan: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance < 10) {
    if ((currentHour == 9 && currentMinute == 40 && (currentSecond == 0 || currentSecond ==1)) || 
        (currentHour == 9 && currentMinute == 42 && (currentSecond == 0 || currentSecond ==1)) || 
        (currentHour == 9 && currentMinute == 44 && (currentSecond == 0 || currentSecond ==1)) || 
        (currentHour == 9 && currentMinute == 46 && (currentSecond == 0 || currentSecond ==1))) {
          activateServo(); // Jika jarak kurang dari 10 cm dan waktu sesuai dengan jadwal, aktifkan servo
    }
  }else{
    servo.write(0);
  }

  if(millis() - previousFirebase >= 10000){
    Serial.println("DataSend");
    // Firebase.setStringAsync(firebaseData, "/sensor/date", dateStr);
    Firebase.setFloat(firebaseData, "/sensor/feedPercentage", feedPercentage);
    Firebase.setFloatAsync(firebaseData, "/sensor/temperature", temperature);
    Firebase.setFloatAsync(firebaseData, "/sensor/humidity", humidity);
    previousFirebase = millis();
  }
  
  if (Firebase.getString(firebaseData, "/sensor/servoValue")){
    String servoValueStr = firebaseData.stringData();
    servoValue = servoValueStr.toInt();
    Serial.println(servoValue*1000);
  }
  if (Firebase.getString(firebaseData, "/sensor/Lampu")) {
      if(firebaseData.dataType() == "string"){
        String Status = firebaseData.stringData();
        Status.replace("\"", "");
        if (Status == "1") {
          Serial.println("Led Menyala");
          digitalWrite(RELAY_PIN, HIGH);
        }else if (Status == "0") {
          Serial.println("led Mati");
          digitalWrite(RELAY_PIN, LOW);
        } else {
          Serial.println("Silahkan hanya isi dengan ON atau OFF");
        }
      }
  }
}

void activateServo() {
  // Aktifkan servo
  servo.write(180); // Posisi 180 derajat membuka servo
  Serial.println("Beri pakan!"); // Tambahkan ini
  delay(servoValue*1000); // Tunggu 1 detik
  servo.write(0); // Nonaktifkan servo setelah diberi pakan
}
