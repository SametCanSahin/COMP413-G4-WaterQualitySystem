/* ============================================================
   ESP32 SU KALİTE SİSTEMİ - RENKLİ VERSİYON (COLOR CHANGING)
   - Consumable Water -> YEŞİL
   - Unconsumable Water -> KIRMIZI
   ============================================================ */

// 1. BLYNK AYARLARI
#define BLYNK_TEMPLATE_ID "TMPL6NcE-av5V"
#define BLYNK_TEMPLATE_NAME "Water Quality System"
#define BLYNK_AUTH_TOKEN "9jRlw8yJNDOjcR2CPXPf1pdLUCJm8TQV"

// 2. WIFI AYARLARI
char ssid[] = "AGUN";
char pass[] = "A03q8uN1";

// 3. KÜTÜPHANELER
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DFRobot_ESP_PH.h"

// !!! KÜTÜPHANE İSMİNİ KONTROL ET !!!
#include <WaterQuality_inferencing.h> 

// 4. PIN TANIMLAMALARI
#define PIN_PH 35           
#define PIN_TDS 34          
#define PIN_TEMP 32         

// Nesneler
OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
DFRobot_ESP_PH ph;
BlynkTimer timer;

// Değişkenler
float tdsDegeri = 0;
float phDegeri = 0;
float sicaklik = 0;

static bool debug_nn = false; 

/* ============================================================
   ANA FONKSİYON
   ============================================================ */
void yapayZekaVeSensorIslemleri() {
  
  // --- A) SENSÖRLERİ OKU ---
  sensors.requestTemperatures(); 
  float okunanSicaklik = sensors.getTempCByIndex(0);
  if (okunanSicaklik > -100) sicaklik = okunanSicaklik;
  else sicaklik = 25.0;

  float voltagePH = analogRead(PIN_PH) / 4095.0 * 3300; 
  phDegeri = ph.readPH(voltagePH, sicaklik);

  float voltageTDS = analogRead(PIN_TDS) * (3.3 / 4096.0);
  float compensationCoefficient = 1.0 + 0.02 * (sicaklik - 25.0);
  float compensationVolatge = voltageTDS / compensationCoefficient;
  tdsDegeri = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge 
               - 255.86 * compensationVolatge * compensationVolatge 
               + 857.39 * compensationVolatge) * 0.5;

  // Blynk'e Sensör Verilerini Gönder
  Blynk.virtualWrite(V0, sicaklik);
  Blynk.virtualWrite(V1, phDegeri);
  Blynk.virtualWrite(V2, tdsDegeri);

  // --- B) YAPAY ZEKA HAZIRLIĞI ---
  size_t beklenenSayi = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  float features[beklenenSayi];

  for (int i = 0; i < beklenenSayi; i += 3) {
      features[i] = tdsDegeri;
      if (i+1 < beklenenSayi) features[i+1] = phDegeri;
      if (i+2 < beklenenSayi) features[i+2] = sicaklik;
  }

  // --- C) TAHMİNİ ÇALIŞTIR ---
  signal_t signal;
  numpy::signal_from_buffer(features, beklenenSayi, &signal);
  
  ei_impulse_result_t result = { 0 };
  run_classifier(&signal, &result, debug_nn);

  // --- D) SONUCU BELİRLE VE RENK AYARLA ---
  String hamSonuc = "";
  float enYuksekIhtimal = 0.0;

  // En yüksek sonucu bul
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      if (result.classification[i].value > enYuksekIhtimal) {
          enYuksekIhtimal = result.classification[i].value;
          hamSonuc = String(result.classification[i].label);
      }
  }

  // Ekrana Yazılacak Değer ve Rengi
  String ekranaYazilacakSonuc = "Analyzing...";
  String renkKodu = "#FFFFFF"; // Varsayılan Beyaz

  // 1. Durum: TEMİZ SU (YEŞİL)
  if (hamSonuc == "Kullanilabilir" || hamSonuc == "Temiz" || hamSonuc == "Icilebilir" || hamSonuc == "consumable") {
      ekranaYazilacakSonuc = "Consumable Water";
      renkKodu = "#23C48E"; // BLYNK YEŞİLİ
  }
  // 2. Durum: KİRLİ SU (KIRMIZI)
  else {
      ekranaYazilacakSonuc = "Unconsumable Water";
      renkKodu = "#D3435C"; // BLYNK KIRMIZISI
  }

  // 3. Durum: pH GÜVENLİK KONTROLÜ (KIRMIZI)
  if (phDegeri < 6.0 || phDegeri > 9.0) {
      ekranaYazilacakSonuc = "Unconsumable Water";
      renkKodu = "#D3435C"; // Tehlike Kırmızısı
  }

  // --- E) BLYNK'E GÖNDER (YAZI + RENK) ---
  
  // Önce rengi ayarla
  Blynk.setProperty(V3, "color", renkKodu);
  
  // Sonra yazıyı gönder
  Blynk.virtualWrite(V3, ekranaYazilacakSonuc);
  
  Serial.print("KARAR: "); Serial.println(ekranaYazilacakSonuc);
}

/* ============================================================
   SETUP VE LOOP
   ============================================================ */
void setup() {
  Serial.begin(115200);
  sensors.begin();
  ph.begin();

  Serial.print("WiFi: "); Serial.println(ssid);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  timer.setInterval(2000L, yapayZekaVeSensorIslemleri);
}

void loop() {
  Blynk.run();
  timer.run();
}