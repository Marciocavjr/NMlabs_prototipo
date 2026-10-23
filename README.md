# NM Labs — Monitoramento Climático IoT

**Resumo do Projeto**
O NM Labs é um sistema IoT desenvolvido para monitorar temperatura e umidade em laboratórios de reparo, prevenindo falhas por descargas eletrostáticas (ESD). O dispositivo atua de forma autônoma acionando o ar-condicionado via infravermelho quando o clima sai da faixa ideal, além de salvar os dados localmente e sincronizá-los na nuvem.

### 💻 Linguagens e Tecnologias
* **C++ (Arduino IDE):** Desenvolvimento do firmware do ESP32.
* **SQL / REST API:** Banco de dados e comunicação com o **Supabase** (PostgreSQL).
* **LittleFS:** Sistema de arquivos para armazenamento offline na própria memória da placa.

### 🛠️ Componentes Utilizados
* Microcontrolador **ESP32 DevKit V1**
* Sensor de Temperatura e Umidade **DHT11**
* Display **LCD 16x2 I2C**
* **LED Emissor IR 5mm** (com resistor de 220Ω)

### 🔌 Esquema de Conexões (Pinagem)
A montagem física no ESP32 foi feita utilizando a seguinte distribuição:

* **Alimentação Geral:** O LCD é alimentado no pino **5V (VIN)**, e o sensor DHT11 no pino **3.3V**. Todos os componentes compartilham o mesmo **GND**.
* **Sensor DHT11:** O pino de dados está conectado ao **GPIO 4**.
* **Display LCD I2C:** A comunicação I2C utiliza o pino SDA no **GPIO 21** e o SCL no **GPIO 22**.
* **LED Emissor IR:** O terminal positivo (Anodo) está ligado ao **GPIO 12** (passando pelo resistor para proteção), e o negativo (Catodo) ao GND.
