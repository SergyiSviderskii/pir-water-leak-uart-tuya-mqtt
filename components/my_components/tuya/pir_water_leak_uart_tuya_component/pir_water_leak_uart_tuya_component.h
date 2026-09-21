// pir, water-leak (tywe3s, cbu)
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
//#include "esphome/components/wifi/wifi_component.h"
#include <vector>
#include <string>
// ЖИЗНЕННО НЕОБХОДИМЫЙ ВКЛЮЧАТЕЛЬ ДЛЯ КЛАССА BinarySensor:
#include "esphome/components/binary_sensor/binary_sensor.h"

#if defined(ESPHOME_LOG_LEVEL) && (ESPHOME_LOG_LEVEL >= 1)
#define ENABLE_DIAGNOSTICS 
#endif

namespace esphome {
namespace pir_water_leak_uart_tuya_component {

enum class TuyaDatapointType : uint8_t {
  INTEGER = 0x02,  // 4 byte  
  ENUM = 0x04,     // 1 byte
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  size_t len;
  union {
    uint32_t value_uint;
    uint8_t value_enum;
  };
};  

enum class TuyaCommandType : uint8_t {
  HEARTBEAT = 0x00,
  PRODUCT_QUERY = 0x01,
  CONF_QUERY = 0x02,
  WIFI_RESET = 0X03,
  WIFI_OTA = 0x04,
  DATAPOINT_REPORT = 0x05,
  DATAPOINT_QUERY = 0x07,
  WIFI_STREAM_STATUS = 0x10
};  

enum class TuyaInitState : uint8_t {
  INIT_HEARTBEAT = 0x00,
  INIT_PRODUCT,
  INIT_CONF,
  INIT_WIFI,
  INIT_WIFI_OTA,
  INIT_DATAPOINT,
  INIT_WIFI_RESET,
  INIT_QUERY,
  INIT_DONE,
  INIT_WIFI_STREAM_STATUS  
};

struct TuyaCommand {
  TuyaCommandType cmd;
  std::vector<uint8_t> payload;
}; 

struct UartFrame {
  uint32_t g_time = 0;
  std::string g_inout;
  std::vector<uint8_t> g_delay;
};

class TuComponent : public Component, public uart::UARTDevice {
  public:
    void setup() override;
    void loop() override;
   
    uint32_t last_off_time = 0;  
    uint32_t get_battery_pir_leak_scale = 0;
    optional<uint8_t> get_battery_pir_leak {};
    bool get_restart_swith_factory = false;
    bool is_diagnostics_enabled = false; // Флаг для YAML: включен ли макрос логов
    
    // Создаем метод, через который YAML передаст ссылку на датчик    
    void set_pir_leak_binary_sensor(binary_sensor::BinarySensor *sensor) { 
      this->pir_leak_sensor_ptr_ = sensor; 
    }   
    
    // 1. ФУНКЦИЯ-ГЕТТЕР: возвращает ссылку на ваш приватный вектор, чтобы YAML мог его прочитать
    const std::vector<UartFrame>& get_diagnostic_vector() const { 
      return this->g_1_; 
    }
 
    // 2. ФУНКЦИЯ-ОЧИСТКА: позволяет YAML дать команду на очистку приватного вектора
    void clear_diagnostic_vector() { 
      this->g_1_.clear(); 
    }    
    
   // void trigger_network_step(uint8_t step) {
//        if (step == 0x00) { this->send_command_(TuyaCommand{ .cmd = TuyaCommandType::DATAPOINT_REPORT, .payload = std::vector<uint8_t>{step}});}
//    }    
  
  protected: 
  
    // В секцию protected добавляем сам указатель
    binary_sensor::BinarySensor *pir_leak_sensor_ptr_{nullptr};  
    
    // Переменные вектора создаются только если рубильник включен
#ifdef ENABLE_DIAGNOSTICS
    UartFrame g_0_{};
    std::vector<UartFrame> g_1_{};
#else
    // Если рубильник выключен, создаем пустой «заглушечный» вектор, 
    // чтобы YAML-лямбда не выдавала ошибку при компиляции
    std::vector<UartFrame> g_1_{}; 
#endif 

    std::vector<uint8_t> rx_message_;
    std::vector<TuyaCommand> command_queue_;
    std::string product_ = "";
    optional<TuyaCommandType> expected_response_{};
    uint32_t last_rx_char_timestamp_ = 0;
    uint32_t last_command_timestamp_ = 0;
    int init_retries_{0};
    bool init_failed_{false};
    TuyaInitState init_state_ = TuyaInitState::INIT_HEARTBEAT;  

   void handle_char_(uint8_t c);
   void handle_datapoints_(const uint8_t *buffer, size_t len);  
   bool validate_message_();
   
   void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
   void send_raw_command_(TuyaCommand command);
   void process_command_queue_();
   void send_command_(const TuyaCommand &command);
   void send_empty_command_(TuyaCommandType command);   
};

}  // namespace pir_water_leak_uart_tuya_component
}  // namespace esphome
