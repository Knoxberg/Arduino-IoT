// Konfigurasi Blynk
#define BLYNK_TEMPLATE_ID "TMPL6A2Jl1DOW"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "[Blynk Auth Token taruh Disini]"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h>

// Pin fisik pada NodeMCU
const int pin_LED = 2;
const int pin_Relay_Power = 25;
const int pin_Server_Power_LED = 26;

// Variabel global untuk menyimpan status dan timer
int status_Server_Terakhir = -1;
int nilai_V3 = 1; // Set default auto-monitor ke on (1)
unsigned long hardShutdownStart = 0;
bool hardShutdownActivated = false;
unsigned long waktuTerakhirMati = 0; // Catat kapan server mulai terdeteksi mati
int waktuServerMati = 0;
unsigned long waktuTerakhirLED = 0;
unsigned long waktuFlashTerakhir = 0;
int hitungFlash = 0;
bool ledNyala = false;
int totalFlash = 0;
int jedaFlash = 0;

// Kredensial WiFi
char ssid[] = "[SSID Taruh Disini]";
char pass[] = "[Password WiFi Taruh Disini]";

// IP statis
IPAddress static_IP(192,168,1,250);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress primary_DNS(8,8,8,8);
IPAddress secondary_DNS(8,8,4,4);

BlynkTimer timer;

// Indikator LED biru di NodeMCU
void ledFlash(int jumlah, int jeda) {
  unsigned long waktuSekarang = millis();

  // Jika jumlah <= 0, artinya flash tak terbatas
  bool infiniteFlash = (jumlah <= 0);

  if (hitungFlash == 0) { // Inisialisasi parameter flash jika fungsi dipanggil untuk pertama kali
    totalFlash = jumlah;
    jedaFlash = jeda;
    hitungFlash++;
    waktuFlashTerakhir = waktuSekarang;
    digitalWrite(pin_LED, HIGH); // Nyalakan LED
    ledNyala = true;
    return;
  }

  if (infiniteFlash || hitungFlash <= 2 * totalFlash) {
    if (waktuSekarang - waktuFlashTerakhir >= jedaFlash) { 
      waktuFlashTerakhir = waktuSekarang;

      if (ledNyala) {
        digitalWrite(pin_LED, LOW); // Matikan LED
      } else {
        digitalWrite(pin_LED, HIGH); // Nyalakan LED
      }
      ledNyala = !ledNyala;
      hitungFlash++;
    }
  } else { // Reset variabel jika flash selesai dan tidak tak terbatas
    hitungFlash = 0;
    ledNyala = false;
  }
}

// Dipanggil ketika perangkat terhubung ke Blynk
BLYNK_CONNECTED() {
  Blynk.syncAll();
}

// V1: Tombol untuk menyalakan server
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    nyalakan_Server();
  }
}

// V2: Tombol untuk matikan server secara paksa
BLYNK_WRITE(V2) {
  if (param.asInt() == 1 && !hardShutdownActivated) {
    hardShutdownActivated = true;
    hardShutdownStart = millis(); // Catat waktu mulai menekan tombol
    Blynk.virtualWrite(V5, "[DANGER] Persiapan untuk Hard Shutdown");
  } else if (param.asInt() == 0) {
    hardShutdownActivated = false;
    Blynk.virtualWrite(V5, "Batal Hard Shutdown");
  }
}

// V3: Tombol untuk mengaktifkan mode Auto Monitor
BLYNK_WRITE(V3) {
  nilai_V3 = param.asInt();
}

// V4: LED Indikator status server
// Otomatis terupdate oleh fungsi cek_Status_Server()

// V5: Label yang menunjukkan apa yang sedang dilakukan oleh IoT
// Otomatis terupdate oleh berbagai fungsi

void nyalakan_Server() {
  ledFlash(2, 100);
  Serial.println("[INFO] Memulai prosedur menyalakan server...");
  digitalWrite(pin_Relay_Power, LOW);
  Serial.println("[OK] Relay power dinyalakan.");
  timer.setTimeout(1200L, []() {
    digitalWrite(pin_Relay_Power, HIGH);
    Serial.println("[OK] Relay power dimatikan.");
    Blynk.virtualWrite(V5, "Melepas Tombol Power");
  });
  Blynk.virtualWrite(V5, "Menekan Tombol Power");
  Serial.println("[INFO] Menekan tombol power.");
}

void matikan_Server_Paksa() {
  ledFlash(3, 100);
  Serial.println("[DANGER] Memulai prosedur HARD SHUTDOWN...");
  digitalWrite(pin_Relay_Power, LOW);
  Serial.println("[DANGER] Menahan tombol power...");
  timer.setTimeout(6000L, []() {
    digitalWrite(pin_Relay_Power, HIGH);
    Serial.println("[OK] Tombol power dilepaskan.");
  });
  Blynk.virtualWrite(V5, "[DANGER] Menahan Tombol Power");
}

void cek_Server_Down() {
  int status_Server_Sekarang = digitalRead(pin_Server_Power_LED);

  // Tambahkan counter mati jika server off dan mode Auto Monitor aktif
  if (nilai_V3 == 1 && status_Server_Sekarang == 0) {
    waktuTerakhirMati++;
  } else {
    waktuTerakhirMati = 0; // Reset counter mati jika server menyala atau mode Auto Monitor tidak aktif
  }
}

void cek_Status_Server() {
  Serial.println("[INFO] Mengecek status server...");
  int status_Server_Sekarang = digitalRead(pin_Server_Power_LED);
  Serial.print("[INFO] Status server saat ini: ");
  Serial.println(status_Server_Sekarang ? "ON" : "OFF");

  if (status_Server_Sekarang != status_Server_Terakhir) {
    Blynk.virtualWrite(V4, status_Server_Sekarang);
    Serial.println("[INFO] Mengupdate LED indikator di Blynk.");
    status_Server_Terakhir = status_Server_Sekarang;
  }

 // Gunakan waktuTerakhirMati untuk memeriksa apakah server telah mati selama 10 detik
  if (waktuTerakhirMati >= 20) { // 0.5 detik * 20 = 10 detik
    Serial.println("[INFO] Mode Auto Monitor aktif. Menyalakan server.");
    nyalakan_Server();
    waktuTerakhirMati = 0; // Reset counter
  }

  // Cek apakah tombol hard shutdown telah ditekan lebih dari 10 detik
  Serial.println("[INFO] Mengecek kondisi untuk hard shutdown...");
  if (hardShutdownActivated && (millis() - hardShutdownStart > 10000)) {
    matikan_Server_Paksa();
    hardShutdownActivated = false; // Reset variabel
    Blynk.virtualWrite(V2, 0); // Reset switch di panel web
    Serial.println("[DANGER] Melakukan hard shutdown.");
  }
  else {
    Serial.println("[INFO] Tidak ada kondisi untuk hard shutdown.");
  }
}

void setup_OTA() {
  ArduinoOTA.onStart([]() {
    Serial.println("Mulai menerima Update...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("Selesai menerima Update...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Gagal autentikasi OTA");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Gagal memulai update OTA");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Gagal tersambung via OTA");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Gagal menerima update OTA");
    else if (error == OTA_END_ERROR) Serial.println("Gagal mengakhiri koneksi OTA");
  });
  ArduinoOTA.begin();
}

void setup() {
  pinMode(pin_LED, OUTPUT);

  // Delay 10 Detik setelah perangkat aktif
  delay(10000);
  pinMode(pin_Relay_Power, OUTPUT);
  digitalWrite(pin_Relay_Power, HIGH);
  pinMode(pin_Server_Power_LED, INPUT);

  Serial.begin(115200);

  WiFi.config(static_IP, gateway, subnet, primary_DNS, secondary_DNS);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  setup_OTA();

  status_Server_Terakhir = digitalRead(pin_Server_Power_LED);

  timer.setInterval(500L, cek_Server_Down); // Setiap 0.5 detik, cek server yang mati
  timer.setInterval(1000L, cek_Status_Server); // Cek setiap 1 detik
  Serial.println("Setup selesai. Menunggu perintah...");
}

void loop() {
  Blynk.run();
  timer.run();
  ArduinoOTA.handle();

  if (hardShutdownActivated) {
    Serial.printf("Waktu hingga Hard Shutdown: %lu", (millis() - hardShutdownStart) / 1000);
  }

  if (nilai_V3 == 1) {
    ledFlash(0, 2000); // Flash LED tak terbatas dengan jeda 2 detik
  } else {
    hitungFlash = 0; // Reset hitungFlash untuk menghentikan flashing
    digitalWrite(pin_LED, LOW); // Pastikan LED dimatikan
  }
}