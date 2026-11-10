#include "network_spike.h"
#include "sdkconfig.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {
const char *tag = "heap_network";
AsyncWebServer server(80);
portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
HeapNetworkStatus status{};

class JsonArena : public ArduinoJson::Allocator {
public:
    void reset() { used = 0; }
    void *allocate(size_t size) override {
        constexpr size_t alignment = alignof(std::max_align_t);
        const size_t header = alignment;
        const size_t total = (header + size + alignment - 1) & ~(alignment - 1);
        if (total > sizeof(storage) - used) return nullptr;
        auto *block = storage + used;
        *reinterpret_cast<size_t *>(block) = size;
        used += total;
        return block + header;
    }
    void deallocate(void *) override {}
    void *reallocate(void *pointer, size_t size) override {
        if (pointer == nullptr) return allocate(size);
        auto *replacement = allocate(size);
        if (replacement != nullptr) {
            const size_t previous = *reinterpret_cast<size_t *>(
                static_cast<uint8_t *>(pointer) - alignof(std::max_align_t));
            memcpy(replacement, pointer, std::min(previous, size));
        }
        return replacement;
    }
private:
    alignas(std::max_align_t) uint8_t storage[8192];
    size_t used = 0;
};

JsonArena json_arena;
uint8_t message_buffer[4096];
WiFiClient printer;
bool fragmented = false;
bool discard_message = false;
bool binary_message = false;
size_t message_size = 0;

bool read_exact(uint8_t *buffer, size_t size) {
    const uint32_t started = millis();
    size_t received = 0;
    while (received < size) {
        const int available = printer.available();
        if (available > 0) {
            const size_t count = std::min(size - received, static_cast<size_t>(available));
            const int actual = printer.read(buffer + received, count);
            if (actual > 0) received += actual;
        } else if (!printer.connected() || millis() - started >= 2000) {
            return false;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    return true;
}

bool send_frame(uint8_t opcode, const uint8_t *payload, size_t length) {
    if (length > 125) return false;
    uint8_t header[6] = {static_cast<uint8_t>(0x80 | opcode),
                         static_cast<uint8_t>(0x80 | length)};
    esp_fill_random(header + 2, 4);
    uint8_t masked[125];
    for (size_t index = 0; index < length; ++index) {
        masked[index] = payload[index] ^ header[2 + index % 4];
    }
    return printer.write(header, sizeof(header)) == sizeof(header)
           && printer.write(masked, length) == length;
}

bool send_probe() {
    constexpr char probe[] = "{\"method\":\"get\",\"params\":{\"ReqPrinterPara\":1}}";
    return send_frame(1, reinterpret_cast<const uint8_t *>(probe), sizeof(probe) - 1);
}

bool read_http_line(char *line, size_t capacity) {
    size_t length = 0;
    uint8_t byte;
    while (length + 1 < capacity) {
        if (!read_exact(&byte, 1)) return false;
        if (byte == '\n') {
            if (length > 0 && line[length - 1] == '\r') --length;
            line[length] = '\0';
            return true;
        }
        line[length++] = byte;
    }
    return false;
}

bool connect_printer() {
    printer.stop();
    if (!printer.connect(CONFIG_KNOMI_SPIKE_PRINTER_HOST, CONFIG_KNOMI_SPIKE_PRINTER_PORT, 5000)) return false;
    printer.setNoDelay(true);
    uint8_t nonce[16];
    esp_fill_random(nonce, sizeof(nonce));
    unsigned char key[25]{};
    size_t encoded = 0;
    if (mbedtls_base64_encode(key, sizeof(key), &encoded, nonce, sizeof(nonce)) != 0) return false;
    char joined[61];
    snprintf(joined, sizeof(joined), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    unsigned char digest[20];
    unsigned char expected[29]{};
    if (mbedtls_sha1(reinterpret_cast<const unsigned char *>(joined), strlen(joined), digest) != 0
        || mbedtls_base64_encode(expected, sizeof(expected), &encoded, digest, sizeof(digest)) != 0) return false;
    printer.printf("GET / HTTP/1.1\r\nHost: %s:%u\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
                   CONFIG_KNOMI_SPIKE_PRINTER_HOST, CONFIG_KNOMI_SPIKE_PRINTER_PORT, key);
    char line[256];
    if (!read_http_line(line, sizeof(line))
        || (strncmp(line, "HTTP/1.1 101 ", 13) != 0 && strncmp(line, "HTTP/1.0 101 ", 13) != 0)) return false;
    bool accept = false;
    bool upgrade = false;
    const uint32_t started = millis();
    for (unsigned count = 0; count < 32 && millis() - started < 5000; ++count) {
        if (!read_http_line(line, sizeof(line))) return false;
        if (line[0] == '\0') {
            fragmented = discard_message = binary_message = false;
            message_size = 0;
            return accept && upgrade;
        }
        char *value = strchr(line, ':');
        if (value == nullptr) continue;
        *value++ = '\0';
        while (*value == ' ' || *value == '\t') ++value;
        if (strcasecmp(line, "Sec-WebSocket-Accept") == 0) {
            accept = strcmp(value, reinterpret_cast<const char *>(expected)) == 0;
        }
        if (strcasecmp(line, "Upgrade") == 0) upgrade = strcasecmp(value, "websocket") == 0;
    }
    return false;
}

bool process_message() {
    portENTER_CRITICAL(&status_lock);
    ++status.messages;
    portEXIT_CRITICAL(&status_lock);
    if (message_size == 2 && memcmp(message_buffer, "ok", 2) == 0) return true;
    json_arena.reset();
    JsonDocument filter(&json_arena);
    const char *fields[] = {"ModeCode", "state", "deviceState", "withSelfTest", "printProgress",
                           "printLeftTime", "printJobTime", "printStartTime", "printFileName",
                           "nozzleTemp", "targetNozzleTemp", "bedTemp0", "targetBedTemp0",
                           "boxTemp", "maxBoxTemp", "targetBoxTemp", "connect", "layer", "TotalLayer",
                           "model", "hostname", "modelVersion", "modelFanPct", "curFeedratePct",
                           "printTimeLeft", "connectionCount", "autoLevelResult"};
    for (const char *field : fields) filter[field] = true;
    filter["err"]["errcode"] = true;
    filter["err"]["key"] = true;
    filter["err"]["value"] = true;
    filter["linuxOtaInfo"]["version"] = true;
    JsonDocument document(&json_arena);
    const auto error = deserializeJson(document, static_cast<const uint8_t *>(message_buffer),
                                       message_size, DeserializationOption::Filter(filter));
    if (error) {
        portENTER_CRITICAL(&status_lock);
        ++status.json_errors;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGW(tag, "Filtered JSON failed: %s", error.c_str());
        return true;
    }
    if (strcmp(document["ModeCode"] | "", "heart_beat") == 0) {
        constexpr uint8_t reply[] = {'o', 'k'};
        return send_frame(1, reply, sizeof(reply));
    }
    return true;
}

bool receive_frame() {
    uint8_t header[2];
    if (!read_exact(header, sizeof(header))) return false;
    const bool final = (header[0] & 0x80) != 0;
    const uint8_t opcode = header[0] & 0x0F;
    if ((header[0] & 0x70) != 0 || (header[1] & 0x80) != 0) return false;
    uint64_t length = header[1] & 0x7F;
    if (length >= 126) {
        const size_t count = length == 126 ? 2 : 8;
        uint8_t extended[8];
        if (!read_exact(extended, count) || (count == 8 && (extended[0] & 0x80))) return false;
        length = 0;
        for (size_t index = 0; index < count; ++index) length = (length << 8) | extended[index];
    }
    if (opcode >= 8) {
        if (!final || length > 125 || (opcode != 8 && opcode != 9 && opcode != 10)) return false;
        uint8_t control[125];
        if (!read_exact(control, static_cast<size_t>(length))) return false;
        if (opcode == 9) return send_frame(10, control, length);
        if (opcode == 8) {
            send_frame(8, control, length);
            return false;
        }
        return true;
    }
    if (opcode == 1 || opcode == 2) {
        if (fragmented) return false;
        message_size = 0;
        discard_message = false;
        binary_message = opcode == 2;
    } else if (opcode != 0 || !fragmented) {
        return false;
    }
    if (length > sizeof(message_buffer) - message_size) discard_message = true;
    uint8_t chunk[512];
    while (length != 0) {
        const size_t count = std::min<uint64_t>(sizeof(chunk), length);
        if (!read_exact(chunk, count)) return false;
        if (!discard_message && !binary_message) {
            memcpy(message_buffer + message_size, chunk, count);
            message_size += count;
        }
        length -= count;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    fragmented = !final;
    if (final) {
        if (discard_message && !binary_message) {
            portENTER_CRITICAL(&status_lock);
            ++status.oversized_messages;
            portEXIT_CRITICAL(&status_lock);
            ESP_LOGI(tag, "Oversized text message discarded; connection kept");
        } else if (!binary_message) {
            return process_message();
        }
    }
    return true;
}

void status_response(AsyncWebServerRequest *request) {
    const auto snapshot = heap_network_status();
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    JsonDocument document;
    document["wifi"] = snapshot.wifi_connected;
    document["web"] = snapshot.web_started;
    document["ws"] = snapshot.ws_connected;
    document["messages"] = snapshot.messages;
    document["oversized"] = snapshot.oversized_messages;
    document["reconnects"] = snapshot.reconnects;
    document["json_errors"] = snapshot.json_errors;
    document["heap"]["free"] = heap_caps_get_free_size(caps);
    document["heap"]["largest"] = heap_caps_get_largest_free_block(caps);
    document["heap"]["minimum"] = heap_caps_get_minimum_free_size(caps);
    document["heap"]["total"] = heap_caps_get_total_size(caps);
    const uint32_t psram_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    document["psram"]["total"] = heap_caps_get_total_size(psram_caps);
    document["psram"]["free"] = heap_caps_get_free_size(psram_caps);
    document["psram"]["largest"] = heap_caps_get_largest_free_block(psram_caps);
    document["psram"]["minimum"] = heap_caps_get_minimum_free_size(psram_caps);
    const uint32_t dma_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    document["dma"]["free"] = heap_caps_get_free_size(dma_caps);
    document["dma"]["largest"] = heap_caps_get_largest_free_block(dma_caps);
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(document, *response);
    request->send(response);
}

void network_task(void *) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(CONFIG_KNOMI_SPIKE_WIFI_SSID, CONFIG_KNOMI_SPIKE_WIFI_PASSWORD);
    server.on("/api/status", HTTP_GET, status_response);
    uint32_t last_retry = millis();
    uint32_t last_ws_attempt = last_retry - 1000;
    uint32_t last_rx = last_retry;
    bool probe_sent = false;
    bool ws_connected = false;
    bool connected_before = false;
    while (true) {
        const bool connected = WiFi.status() == WL_CONNECTED;
        if (connected && !status.web_started) {
            server.begin();
            portENTER_CRITICAL(&status_lock);
            status.web_started = true;
            portEXIT_CRITICAL(&status_lock);
        }
        portENTER_CRITICAL(&status_lock);
        status.wifi_connected = connected;
        portEXIT_CRITICAL(&status_lock);
        if (connected && !connected_before) {
            ESP_LOGI(tag, "STA connected; diagnostic URL http://%s/api/status",
                     WiFi.localIP().toString().c_str());
        }
        if (!connected && millis() - last_retry >= 30000) {
            ESP_LOGW(tag, "STA not connected; retrying (credentials are never logged)");
            WiFi.reconnect();
            last_retry = millis();
        }
        if (!connected || !printer.connected()) ws_connected = false;
        if (connected && !ws_connected && millis() - last_ws_attempt >= 1000) {
            last_ws_attempt = millis();
            ws_connected = connect_printer();
            portENTER_CRITICAL(&status_lock);
            ++status.reconnects;
            portEXIT_CRITICAL(&status_lock);
            if (ws_connected) {
                ESP_LOGI(tag, "Read-only printer WebSocket connected");
                last_rx = millis();
                probe_sent = false;
            } else {
                printer.stop();
                ESP_LOGW(tag, "Printer WebSocket connect/upgrade failed");
            }
        }
        if (ws_connected && printer.available() > 0) {
            ws_connected = receive_frame();
            last_rx = millis();
            probe_sent = false;
        }
        if (ws_connected && millis() - last_rx >= 10000 && !probe_sent) {
            ws_connected = send_probe();
            probe_sent = true;
        }
        if (ws_connected && millis() - last_rx >= 30000) ws_connected = false;
        if (!ws_connected) printer.stop();
        portENTER_CRITICAL(&status_lock);
        status.ws_connected = ws_connected;
        portEXIT_CRITICAL(&status_lock);
        connected_before = connected;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}

HeapNetworkStatus heap_network_status() {
    portENTER_CRITICAL(&status_lock);
    const auto snapshot = status;
    portEXIT_CRITICAL(&status_lock);
    return snapshot;
}

void start_heap_network() {
    if (CONFIG_KNOMI_SPIKE_WIFI_SSID[0] == '\0') {
        ESP_LOGE(tag, "Configure Wi-Fi in menuconfig under KNOMI Phase 0.3 heap spike; Phase 0.3 is not active");
        return;
    }
    if (xTaskCreatePinnedToCore(network_task, "heap_net", 6144, nullptr, 5, nullptr, 0) != pdPASS) {
        ESP_LOGE(tag, "Network task allocation failed");
    }
}