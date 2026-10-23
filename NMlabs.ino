
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Electra.h>

// ==================== SENSORES ====================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==================== INFRAVERMELHO ====================

const uint16_t kIrLed = 12;
IRElectraAc ac(kIrLed);

// ==================== WIFI ====================

const char* ssid = "Mjrc";
const char* password = "qwerty123";

// ==================== SUPABASE ====================

const char* supabase_url =
  "https://jfttyewqluguuiqflquk.supabase.co/rest/v1/leituras_ambientais";

const char* supabase_key =
  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImpmdHR5ZXdxbHVndXVpcWZscXVrIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODk2ODQ3MjAsImV4cCI6MjEwNTI2MDcyMH0.jE6NRvJpGA17M1pe6HhIo0kf4112pDkaaqNEUdVgPY0";

// ==================== HORARIO NTP ====================

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

bool ntpConfigurado = false;

// ==================== TEMPORIZADORES ====================

unsigned long lastReadTime = 0;
const unsigned long readInterval = 5000;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;

unsigned long lastIrSendTime = 0;
const unsigned long irSendInterval = 10000;

// ==================== DADOS AMBIENTAIS ====================

float tempAtual = NAN;
float umidAtual = NAN;

String statusAtual = "NORMAL";

bool leituraValida = false;
bool filesystemOK = false;

const char* arquivoOffline = "/dados_offline.txt";

// ==================== PROTOTIPOS ====================

void lerSensores();
void controlarArCondicionado(unsigned long agora);
void registrarLeitura();
void enviarFlashParaSupabase();
void conectarWiFi(unsigned long agora);

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Inicializa o LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("NM Labs IoT");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // Inicializa o sensor
  dht.begin();

  // Inicializa o transmissor IR
  ac.begin();

  // Inicializa a memoria flash
  filesystemOK = LittleFS.begin(true);

  if (filesystemOK) {
    Serial.println("LittleFS montado com sucesso.");
  } else {
    Serial.println("Erro ao montar LittleFS.");
  }

  // Inicia Wi-Fi sem bloquear o programa
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Iniciando conexao Wi-Fi...");
  Serial.println("NM Labs - Monitoramento Ambiental");
  Serial.println("Controle IR do ar-condicionado");
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  unsigned long agora = millis();

  // 1. Gerencia a conexao Wi-Fi
  conectarWiFi(agora);

  // 2. Leitura dos sensores a cada 5 segundos
  if (agora - lastReadTime >= readInterval) {
    lastReadTime = agora;
    lerSensores();
  }

  // 3. Controle IR independente do envio ao Supabase
  controlarArCondicionado(agora);

  // 4. Registro e sincronizacao a cada 30 segundos
  if (agora - lastSendTime >= sendInterval) {
    lastSendTime = agora;

    if (leituraValida) {
      registrarLeitura();
    }

    if (WiFi.status() == WL_CONNECTED &&
        filesystemOK &&
        LittleFS.exists(arquivoOffline)) {
      enviarFlashParaSupabase();
    }
  }
}

// ==================== CONEXAO WIFI ====================

void conectarWiFi(unsigned long agora) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpConfigurado) {
      configTime(
        gmtOffset_sec,
        daylightOffset_sec,
        ntpServer
      );

      ntpConfigurado = true;
      Serial.println("Wi-Fi conectado. NTP configurado.");
    }

    return;
  }

  if (agora - lastReconnectAttempt >= reconnectInterval) {
    lastReconnectAttempt = agora;

    Serial.println("Wi-Fi desconectado. Reconectando...");
    WiFi.reconnect();
  }
}

// ==================== LEITURA DOS SENSORES ====================

void lerSensores() {
  float novaUmidade = dht.readHumidity();
  float novaTemperatura = dht.readTemperature();

  if (isnan(novaUmidade) || isnan(novaTemperatura)) {
    leituraValida = false;

    Serial.println("Erro na leitura do DHT11.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Erro no sensor");

    return;
  }

  tempAtual = novaTemperatura;
  umidAtual = novaUmidade;
  leituraValida = true;

  // O status ambiental considera temperatura e umidade
  if (tempAtual < 22.0 ||
      tempAtual > 26.0 ||
      umidAtual < 40.0 ||
      umidAtual > 60.0) {
    statusAtual = "ALERTA";
  } else {
    statusAtual = "NORMAL";
  }

  // Atualiza LCD
  lcd.clear();

  lcd.setCursor(0, 0);

  if (statusAtual == "ALERTA") {
    lcd.print("! ALERTA !");
  } else {
    lcd.print("Ambiente OK");
  }

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(tempAtual, 1);
  lcd.print(" U:");
  lcd.print(umidAtual, 1);

  Serial.print("Temperatura: ");
  Serial.print(tempAtual, 1);
  Serial.print(" C | Umidade: ");
  Serial.print(umidAtual, 1);
  Serial.print("% | Status: ");
  Serial.println(statusAtual);
}

// ==================== CONTROLE DO AR-CONDICIONADO ====================

void controlarArCondicionado(unsigned long agora) {
  // Nao transmite se a leitura do sensor for invalida
  if (!leituraValida) {
    return;
  }

  // Esta configuracao e de REFRIGERACAO.
  // So envia comandos quando a temperatura passa de 26 C.
  if (tempAtual <= 26.0) {
    return;
  }

  // Limita os envios a um comando a cada 10 segundos
  if (agora - lastIrSendTime < irSendInterval) {
    return;
  }

  lastIrSendTime = agora;

  Serial.println("Temperatura acima de 26 C!");
  Serial.println("Enviando comando IR para o ar-condicionado...");

  ac.on();
  ac.setTemp(20);
  ac.setMode(kElectraAcCool);
  ac.setFan(kElectraAcFanHigh);

  ac.send();

  Serial.println("Comando IR transmitido.");
  Serial.println("Proxima tentativa em 10 segundos.");
}

// ==================== REGISTRO OFFLINE ====================

void registrarLeitura() {
  if (!filesystemOK) {
    Serial.println("LittleFS indisponivel. Leitura nao salva.");
    return;
  }

  struct tm timeinfo;

  String dataStr = "";
  String horaStr = "";

  if (ntpConfigurado && getLocalTime(&timeinfo, 100)) {
    char dataRegistro[11];
    strftime(
      dataRegistro,
      sizeof(dataRegistro),
      "%Y-%m-%d",
      &timeinfo
    );

    char horaRegistro[9];
    strftime(
      horaRegistro,
      sizeof(horaRegistro),
      "%H:%M:%S",
      &timeinfo
    );

    dataStr = String(dataRegistro);
    horaStr = String(horaRegistro);
  }

  // Monta o JSON de uma leitura
  String linhaJSON = "{";

  linhaJSON += "\"temperatura\":" + String(tempAtual, 1) + ",";
  linhaJSON += "\"umidade\":" + String(umidAtual, 1) + ",";

  if (dataStr.length() > 0) {
    linhaJSON += "\"data_registro\":\"" + dataStr + "\",";
    linhaJSON += "\"hora_registro\":\"" + horaStr + "\",";
  }

  linhaJSON += "\"log_status\":\"" + statusAtual + "\"";
  linhaJSON += "}";

  File file = LittleFS.open(arquivoOffline, FILE_APPEND);

  if (file) {
    file.println(linhaJSON);
    file.close();

    Serial.println("Leitura salva na memoria flash.");
    Serial.println(linhaJSON);
  } else {
    Serial.println("Erro ao salvar leitura offline.");
  }
}

// ==================== ENVIO AO SUPABASE ====================

void enviarFlashParaSupabase() {
  if (WiFi.status() != WL_CONNECTED || !filesystemOK) {
    return;
  }

  File file = LittleFS.open(arquivoOffline, FILE_READ);

  if (!file) {
    return;
  }

  std::vector<String> linhas;

  while (file.available()) {
    String linha = file.readStringUntil('\n');
    linha.trim();

    if (linha.length() > 5) {
      linhas.push_back(linha);
    }
  }

  file.close();

  if (linhas.empty()) {
    LittleFS.remove(arquivoOffline);
    return;
  }

  const size_t TAMANHO_BLOCO = 40;

  size_t totalLinhas = linhas.size();
  size_t enviadasComSucesso = 0;

  for (size_t i = 0; i < totalLinhas; i += TAMANHO_BLOCO) {
    // Interrompe se o Wi-Fi cair durante o envio
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi caiu durante a sincronizacao.");
      break;
    }

    String jsonPayload = "[";

    size_t fimBloco = std::min(
      i + TAMANHO_BLOCO,
      totalLinhas
    );

    for (size_t j = i; j < fimBloco; j++) {
      jsonPayload += linhas[j];

      if (j < fimBloco - 1) {
        jsonPayload += ",";
      }
    }

    jsonPayload += "]";

    HTTPClient http;

    http.setTimeout(10000);

    if (!http.begin(supabase_url)) {
      Serial.println("Erro ao iniciar conexao HTTP.");
      http.end();
      break;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabase_key);
    http.addHeader(
      "Authorization",
      String("Bearer ") + supabase_key
    );
    http.addHeader("Prefer", "return=minimal");

    int httpCode = http.POST(jsonPayload);

    http.end();

    if (httpCode >= 200 && httpCode < 300) {
      enviadasComSucesso = fimBloco;

      Serial.print("Bloco enviado: ");
      Serial.print(enviadasComSucesso);
      Serial.print("/");
      Serial.println(totalLinhas);
    } else {
      Serial.print("Erro HTTP ao enviar ao Supabase: ");
      Serial.println(httpCode);
      break;
    }

    delay(200);
  }

  // Remove o arquivo quando todos os registros foram enviados
  if (enviadasComSucesso == totalLinhas) {
    LittleFS.remove(arquivoOffline);

    Serial.println(
      "Sincronizacao concluida. Arquivo offline limpo."
    );
  } else if (enviadasComSucesso > 0) {
    // Preserva os registros ainda nao enviados
    File arquivoNovo = LittleFS.open(
      arquivoOffline,
      FILE_WRITE
    );

    if (arquivoNovo) {
      for (size_t k = enviadasComSucesso;
           k < totalLinhas;
           k++) {
        arquivoNovo.println(linhas[k]);
      }

      arquivoNovo.close();

      Serial.println(
        "Registros pendentes preservados na flash."
      );
    } else {
      Serial.println(
        "Erro ao reescrever arquivo offline."
      );
    }
  }
}
