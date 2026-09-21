// pir, water-leak (tywe3s)
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "pir_water_leak_uart_tuya_component.h"

#ifdef USE_ESP8266
extern "C" {
  #include <ets_sys.h>
  // Явно указываем компилятору сигнатуру функции из SDK
  void uart_tx_one_char(uint8_t uart_no, uint8_t data);
}
#define UART0 0
#endif

namespace esphome {
namespace pir_water_leak_uart_tuya_component {

static const char *const TAG = "tuya";
static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;

    void  TuComponent::setup() {

#ifdef USE_ESP8266
      // 1. Отправляем 0x00 напрямую в регистр
      uart_tx_one_char(UART0, 0x00); 
      delay(20); // Используем delay() вместо delayMicroseconds

      // 2. Отправляем 0xFF напрямую в регистр
      uart_tx_one_char(UART0, 0xFF); 
      delay(10);
#endif     
      
#if defined(USE_BK72XX) || defined(LT_BK7231N)
      const uint8_t bk_tx_pin = 11; 
      
      // 1. Удерживаем линию, выжигая мусор загрузчика Beken
      pinMode(bk_tx_pin, OUTPUT);
      digitalWrite(bk_tx_pin, LOW);  
      delay(100);                    
    
      digitalWrite(bk_tx_pin, HIGH); 
      delay(20);                     
    
      // 2. ИСПРАВЛЕНО: Вместо перевода в INPUT возвращаем пин в режим аппаратного UART!
      // В ядре LibreTiny для Beken за переключение пина 11 (TX1) в режим UART отвечает макрос 
      // или прямой вызов ре-инициализации порта. 
      // Самый надежный способ вернуть Alt-функцию UART1 в Arduino-слое LibreTiny:
      
  #if defined(LT_BK7231N)
          // Принудительно стартуем Serial1, чтобы он перехватил пины 10 и 11 обратно в UART режим
          Serial1.begin(115200); 
  #endif
#endif
      
      // ЕСЛИ ЛОГИ ВКЛЮЧЕНЫ В YAML — ФЛАГ СТАНЕТ TRUE, ЕСЛИ NONE — ОСТАНЕТСЯ FALSE:
#ifdef ENABLE_DIAGNOSTICS
      this->is_diagnostics_enabled = true;
#else
      this->is_diagnostics_enabled = false;
#endif 
    
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);  
      //this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);  
     
    }     
    
    void  TuComponent::loop()  {
      while (this->available()) {
        uint8_t c;
        this->read_byte(&c);
        this->handle_char_(c);
      }
      process_command_queue_();
    }

   
    void  TuComponent::handle_char_(uint8_t c) {      
      this->rx_message_.push_back(c);             
      if (!this->validate_message_()) {
        this->rx_message_.clear();
      } else {
        last_rx_char_timestamp_ = millis();
      }     
    }     
    
      bool  TuComponent::validate_message_() {
        uint32_t at = this->rx_message_.size() - 1;
        auto *data = &this->rx_message_[0];
        uint8_t new_byte = data[at];
 
        // Byte 0: HEADER1 (always 0x55)
        if (at == 0)
          return new_byte == 0x55;
        // Byte 1: HEADER2 (always 0xAA)
        if (at == 1)
          return new_byte == 0xAA;
 
        // Byte 2: VERSION
        // no validation for the following fields:
        uint8_t version = data[2];
        if (at == 2)
          return true;
        // Byte 3: COMMAND
        uint8_t command = data[3]; 
        if (at == 3)
          return true;
 
        // Byte 4: LENGTH1
        // Byte 5: LENGTH2
        if (at <= 5) {
          // no validation for these fields
          return true;
        }
 
        uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));
 
        // wait until all data is read
        if (at - 6 < length)
          return true;
 
        // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
        uint8_t rx_checksum = new_byte;
        uint8_t calc_checksum = 0;
        for (uint32_t i = 0; i < 6 + length; i++)
          calc_checksum += data[i];
        
        if (rx_checksum != calc_checksum) {
          return false;
        }
 
        // valid message
        const uint8_t *message_data = data + 6;
        
#ifdef ENABLE_DIAGNOSTICS
        this->g_0_.g_time = millis();
        this->g_0_.g_delay = rx_message_;
        this->g_0_.g_inout = "Received"; 
        this->g_1_.push_back(this->g_0_);
#endif        
        
        this->handle_command_(command, version, message_data, length);

        // return false to reset rx buffer
        return false;
      }


      void  TuComponent::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
        TuyaCommandType command_type = (TuyaCommandType) command;
       
        if (this->expected_response_.has_value() && this->expected_response_ == command_type) {
          this->expected_response_.reset();
          this->command_queue_.erase(command_queue_.begin());
          this->init_retries_ = 0;
        }

        switch (command_type) {    
          case TuyaCommandType::HEARTBEAT:
          
              this->init_state_ = TuyaInitState::INIT_PRODUCT;
              this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);

            break;            
          case TuyaCommandType::PRODUCT_QUERY: {
            // check it is a valid string made up of printable characters
            bool valid = true;
            for (size_t i = 0; i < len; i++) {
              if (!std::isprint(buffer[i])) {
                valid = false;
                break;
              }
            }
            if (valid) {
              this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
            } else {
              this->product_ = R"({"p":"INVALID"})";
            }
            
              this->init_state_ = TuyaInitState::INIT_CONF;
              this->send_command_(TuyaCommand{.cmd = TuyaCommandType::CONF_QUERY, .payload = std::vector<uint8_t>{0x02}});

          break;
            
          }   
          case TuyaCommandType::CONF_QUERY: {
            if (this->init_state_ == TuyaInitState::INIT_WIFI) {
              this->init_state_ = TuyaInitState::INIT_DATAPOINT;
              this->send_command_(TuyaCommand{.cmd = TuyaCommandType::CONF_QUERY, .payload = std::vector<uint8_t>{0x04}});              
            }            
        
            if (this->init_state_ == TuyaInitState::INIT_CONF) {
              this->init_state_ = TuyaInitState::INIT_WIFI;
              this->send_command_(TuyaCommand{.cmd = TuyaCommandType::CONF_QUERY, .payload = std::vector<uint8_t>{0x03}});
            }             
            
            break;
          }
          case TuyaCommandType::WIFI_RESET: {
            if (this->init_state_ == TuyaInitState::INIT_WIFI_RESET) {
              this->init_state_ = TuyaInitState::INIT_QUERY;
              ESP_LOGE(TAG, "=========RESET========="); 
              get_restart_swith_factory = true;
            } 
            
#if defined(USE_BK72XX) || defined(LT_BK7231N)

              // Отвечаем в MCU, чтобы подтвердить получение
              this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_RESET, .payload = std::vector<uint8_t>{0x02}});

#endif
            break;
          }
          case TuyaCommandType::WIFI_OTA: {
            if (this->init_state_ == TuyaInitState::INIT_WIFI_OTA) {
              this->init_state_ = TuyaInitState::INIT_WIFI_RESET;
            } else {
              this->init_state_ = TuyaInitState::INIT_WIFI_OTA;
              ESP_LOGE(TAG, "=========WIFI OTA========="); 
              ESP_LOGE(TAG, "[Received] <-----:FRAME=[%s] %u  ", format_hex_pretty(this->rx_message_).c_str(), millis());              
            }
            
            this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT, .payload = std::vector<uint8_t>{0x00}});
            break;
          }
          case TuyaCommandType::DATAPOINT_REPORT: {
              
            this->handle_datapoints_(buffer, len);
            
            if (this->init_state_ != TuyaInitState::INIT_DONE) {
              this->init_state_ = TuyaInitState::INIT_DONE;
              this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT, .payload = std::vector<uint8_t>{0x00}});
            } else {
            //   this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_RESET, .payload = std::vector<uint8_t>{0x02}});    
            }
            
            break;
          }   
          case TuyaCommandType::DATAPOINT_QUERY: {
            break;
          } 
          case TuyaCommandType::WIFI_STREAM_STATUS: { // Получили 0x10 от MCU --> BK7231
            // Отправляем пустой ответ с тем же типом команды (ACK)
            
            this->send_empty_command_(TuyaCommandType::WIFI_STREAM_STATUS);
            
            break;  
          }    
          default:
            break;
        }  
      }
  
     
      void  TuComponent::process_command_queue_() {
        uint32_t now = millis();
        uint32_t delay = now - this->last_command_timestamp_;    

        if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {               
          this->rx_message_.clear();
        }
        

        // Проверяем: таймер взведен (не 0) И прошло 300 мс
        if (this->last_off_time != 0 && this->init_state_ == TuyaInitState::INIT_DONE && (now - this->last_off_time > RECEIVE_TIMEOUT)) {       
            
           this->last_off_time = 0; // Сбрасываем в 0, выполняя роль флага! Повторов не будет.            
            
           this->command_queue_.push_back(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT, .payload = std::vector<uint8_t>{0x00}});
    
        }
        
        if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
          this->expected_response_.reset();
          if (this->init_state_ != TuyaInitState::INIT_DONE && this->init_state_ != TuyaInitState::INIT_WIFI_OTA) {
            if (++this->init_retries_ >= MAX_RETRIES) {
              ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
              this->command_queue_.erase(command_queue_.begin());
              this->init_retries_ = 0;
            }
            if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT || this->init_state_ == TuyaInitState::INIT_PRODUCT) {  
              this->command_queue_.clear();
              if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT){      
              this->init_state_ = TuyaInitState::INIT_PRODUCT;
              this->command_queue_.push_back(TuyaCommand{.cmd = TuyaCommandType::PRODUCT_QUERY, .payload = std::vector<uint8_t>{}});    
              } else {   
                this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
                this->command_queue_.push_back(TuyaCommand{.cmd = TuyaCommandType::HEARTBEAT, .payload = std::vector<uint8_t>{}});
              }
            }             
          } else {
            this->command_queue_.erase(command_queue_.begin());
          }
        }
        
        // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
        if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty() &&
          !this->expected_response_.has_value()) {
          this->send_raw_command_(command_queue_.front());
          if (!this->expected_response_.has_value())
            this->command_queue_.erase(command_queue_.begin());
        }
      } 
        
          
      void  TuComponent::send_command_(const TuyaCommand &command) {
        command_queue_.push_back(command);
        process_command_queue_();
      }
 
 
      void  TuComponent::send_empty_command_(TuyaCommandType command) {
        send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
      } 
      
      
      void  TuComponent::send_raw_command_(TuyaCommand command) {
        uint8_t len_hi = (uint8_t)(command.payload.size() >> 8);
        uint8_t len_lo = (uint8_t)(command.payload.size() & 0xFF);
        uint8_t version = 0;
 
        this->last_command_timestamp_ = millis();
        switch (command.cmd) {
          case TuyaCommandType::HEARTBEAT:
            this->expected_response_ = TuyaCommandType::HEARTBEAT; 
            break;             
          case TuyaCommandType::PRODUCT_QUERY:
            this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
            break;
          case TuyaCommandType::CONF_QUERY:
            this->expected_response_ = TuyaCommandType::CONF_QUERY;
            break;
          case TuyaCommandType::DATAPOINT_REPORT:
            this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT;
            break; 
          case TuyaCommandType::WIFI_STREAM_STATUS:
            this->expected_response_ = TuyaCommandType::WIFI_STREAM_STATUS;  
            break;            
          case TuyaCommandType::WIFI_RESET:
            this->expected_response_ = TuyaCommandType::WIFI_RESET;
            break;            
          default:
            break;            
        }
 
 
        // 1. Считаем контрольную сумму заранее
        uint8_t checksum = 0x55 + 0xAA + (uint8_t) command.cmd + len_hi + len_lo;
        for (auto &data : command.payload)
          checksum += data;
    
        // 2. Создаем один монолитный вектор для отправки в физический UART
        std::vector<uint8_t> tx_buffer;
        tx_buffer.reserve(7 + command.payload.size()); // Выделяем память сразу под весь фрейм
    
        // Собираем фрейм в памяти в единый кусок кремния
        tx_buffer.insert(tx_buffer.end(), {0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
        if (!command.payload.empty()) {
            tx_buffer.insert(tx_buffer.end(), command.payload.begin(), command.payload.end());
        }
        tx_buffer.push_back(checksum);
    
        ESP_LOGE(TAG, "   [Sending] -----> [%s] Время: %u мс", format_hex_pretty(tx_buffer).c_str(), millis());        
 
        // Даем микро-фоновое время сетевой карте начать отправку лога по Wi-Fi
        // пока контроллер физически пишет байты в UART шину
        if (command.cmd == TuyaCommandType::DATAPOINT_REPORT) {
            delay(20); // Задержка СТРОГО ДО write_array, чтобы лог улетел в сеть, а MCU еще не знал о команде «смерти»
        }
    
        // 3. Физический выстрел в UART
        this->write_array(tx_buffer.data(), tx_buffer.size());
        this->flush();         
        
#ifdef ENABLE_DIAGNOSTICS
        this->g_0_.g_time = millis();
        this->g_0_.g_delay = tx_buffer; // Просто копируем уже готовый чистый вектор фрейма
        this->g_0_.g_inout = "Sending";  
        this->g_1_.push_back(this->g_0_); 
#endif   
      }    
    

      void TuComponent::handle_datapoints_(const uint8_t *buffer, size_t len) {
        size_t index = 0;
      
        // Безопасный цикл буферизации: последовательно разбираем пачки датапоинтов
        while (index + 4 <= len) {
          TuyaDatapoint datapoint{};
          datapoint.id = buffer[index];
          datapoint.type = (TuyaDatapointType) buffer[index + 1];
          
          // Считаем точный размер полезных данных текущего датапоинта
          size_t data_size = (buffer[index + 2] << 8) + buffer[index + 3];
          const uint8_t *data = buffer + index + 4;
          
          // Защита от выхода за границы общего буфера (битый пакет)
          if (index + 4 + data_size > len) {
            return; 
          }
      
          datapoint.len = data_size;
      
          switch (datapoint.type) {
            
            // ==========================================
            // ТИП ДАННЫХ: ENUM (Перечисление)
            // ==========================================
            case TuyaDatapointType::ENUM: {
                if (data_size == 1) {
                  datapoint.value_enum = data[0];
      
#ifdef USE_ESP8266              
                  // --- ОБРАБОТКА ДЛЯ ESP8266 (модуль TYWE3S) ---
                  if (datapoint.id == 1) {
                    bool is_active = (datapoint.value_enum == 0x01 || datapoint.value_enum == 0x02); 
                    if (this->pir_leak_sensor_ptr_ != nullptr) 
                      this->pir_leak_sensor_ptr_->publish_state(is_active);          
                  }
                  if (datapoint.id == 3) {
                    get_battery_pir_leak = datapoint.value_enum;  
                  }          
#endif   
      
#if defined(USE_BK72XX) || defined(LT_BK7231N)
                  // --- ОБРАБОТКА ДЛЯ BK7231 (Ваш модуль CBU) ---
                  if (datapoint.id == 1 || datapoint.id == 9) {
                    bool is_active = false;
                    if (datapoint.id == 9) {
                      // Ловим единицу сработки движения (09 04 00 01 01 на 952-й миллисекунде)
                      is_active = (datapoint.value_enum == 0x01 || datapoint.value_enum == 0x02); 
                    } else {
                      // Ловим единицу сработки со старого ID 1 (01 04 00 01 01 на 626-й миллисекунде)
                      is_active = (datapoint.value_enum == 0x01 || datapoint.value_enum == true);
                    }
      
                    if (this->pir_leak_sensor_ptr_ != nullptr) 
                      this->pir_leak_sensor_ptr_->publish_state(is_active);          
                  }
                  
                  // Защитный парсинг ID 10 (Оставляем, но приоритет теперь у VALUE ID 4)
                  if (datapoint.id == 10) {
                    // Если в YAML батарея еще не обработана (.has_value() == false), 
                    // разрешаем записать старый формат, если он вдруг прилетит
                    if (!get_battery_pir_leak.has_value()) {
                      uint8_t mcu_bat = datapoint.value_enum;
                      if (mcu_bat == 0x00) { get_battery_pir_leak = 2; } 
                      else if (mcu_bat == 0x01) { get_battery_pir_leak = 1; } 
                      else if (mcu_bat == 0x02 || mcu_bat == 0x03) { get_battery_pir_leak = 0; }
                    }
                  }
#endif
                }
                break;
              }
      
            // ==========================================
            // ТИП ДАННЫХ: VALUE (Числовые проценты / Сплошная шкала)
            // ==========================================
            case TuyaDatapointType::INTEGER: {
#if defined(USE_BK72XX) || defined(LT_BK7231N)
                // --- ПЕРЕХВАТ ДЛЯ НОВОГО ДАТЧИКА BK7231 ---
                // Лог: [55 AA 00 05 00 0D 01 04 00 01 01 04 02 00 04 00 00 00 64 86] на 626 мс
                if (datapoint.id == 4 && data_size == 4) {
                    
                  datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);    
                  
                  // ЗАЩИТНЫЙ ФИЛЬТР ПОВТОРОВ:
                  // Если в get_battery_pir_leak уже лежит двойка, которую ждет YAML сценарий,
                  // повторные пакеты на 2262мс и 3656мс просто игнорируем, чтобы не спамить
                  if (!get_battery_pir_leak.has_value()) {
                    // Принудительно выставляем двойку для вашего виртуального АЦП
                    get_battery_pir_leak = 3; 
                    get_battery_pir_leak_scale = datapoint.value_uint; 
                  }
                }
#endif
                break;
              }
      
            // По умолчанию игнорируем неподдерживаемые типы (например, строки или байт-массивы)
            default:
              break; 
          }
      
          // Сдвигаем индекс буфера на размер обработанной точки (4 байта заголовка DP + длина данных)
          index += 4 + data_size;
        }
      }


}  // namespace pir_water_leak_uart_tuya_component
}  // namespace esphome

