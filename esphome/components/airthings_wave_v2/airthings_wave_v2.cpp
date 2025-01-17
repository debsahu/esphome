#include "airthings_wave_v2.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_wave_v2 {

static const char *const TAG = "airthings_wave_v2";

void AirthingsWaveV2::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle_ = 0;
      auto chr = this->parent()->get_characteristic(service_uuid_, sensors_data_characteristic_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
                 sensors_data_characteristic_uuid_.to_string().c_str());
        break;
      }
      this->handle_ = chr->handle;
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;

      request_read_values_();
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent()->conn_id)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->handle_) {
        read_sensors_(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void AirthingsWaveV2::read_sensors_(uint8_t *raw_value, uint16_t value_len) {
  auto value = (WaveReadings *) raw_value;

  if (sizeof(WaveReadings) <= value_len) {
    ESP_LOGD(TAG, "version = %d", value->version);

    if (value->version == 1) {

      this->humidity_sensor_->publish_state(value->humidity / 2.0f);
      if (is_valid_radon_value_(value->radon)) {
        this->radon_sensor_->publish_state(value->radon);
      }
      if (is_valid_radon_value_(value->radon_lt)) {
        this->radon_long_term_sensor_->publish_state(value->radon_lt);
      }
      this->temperature_sensor_->publish_state(value->temperature / 100.0f);
      }

      // This instance must not stay connected
      // so other clients can connect to it (e.g. the
      // mobile app).
      parent()->set_enabled(false);
    } else {
      ESP_LOGE(TAG, "Invalid payload version (%d != 1, newer version or not a Wave Plus?)", value->version);
    }
  }
}

bool AirthingsWaveV2::is_valid_radon_value_(uint16_t radon) { return 0 <= radon && radon <= 16383; }

void AirthingsWaveV2::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (!parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      parent()->set_enabled(true);
      parent()->connect();
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

void AirthingsWaveV2::request_read_values_() {
  auto status =
      esp_ble_gattc_read_char(this->parent()->gattc_if, this->parent()->conn_id, this->handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
  }
}

void AirthingsWaveV2::dump_config() {
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

AirthingsWaveV2::AirthingsWaveV2()
    : PollingComponent(10000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      sensors_data_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID)) {}

}  // namespace airthings_wave
}  // namespace esphome

#endif  // USE_ESP32