#pragma once

#include <stdint.h>

struct HeapNetworkStatus {
    bool wifi_connected;
    bool web_started;
    bool ws_connected;
    uint32_t messages;
    uint32_t oversized_messages;
    uint32_t reconnects;
    uint32_t json_errors;
};

void start_heap_network();
HeapNetworkStatus heap_network_status();