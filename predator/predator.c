#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include "utils/subghz/subghz_txrx.h"
#include "nfc/nfc_scanner.h"


const int NumberOfApps = 2;

// Array of app names
const char* app_names[2] = {
    "SubScanApp",
    "TestApp",
};

typedef struct {
    double current_rssi;
    double current_rfid_strength;
    char nfc_protocol_info[128];  // Stores information about detected NFC protocols
} SubScanAppContext;

typedef struct {
    int selected_app_index;
    bool should_quit;
    void* app_data; // Generic pointer for app-specific data
} PredatorState;

void update_rssi(SubGhzTxRx* txrx, SubScanAppContext* context) {
    context->current_rssi = subghz_txrx_radio_device_get_rssi(txrx); // Assuming this function exists
}

void nfc_scanner_callback(NfcScannerEvent event, void* context) {
    SubScanAppContext* app_context = (SubScanAppContext*)context;

    if(event.type == NfcScannerEventTypeDetected) {
        // Assuming you want to store a summary of detected protocols
        snprintf(app_context->nfc_protocol_info, sizeof(app_context->nfc_protocol_info),
                 "Detected NFC Protocols: %zu", event.data.protocol_num);
    }
}


static void predator_app_draw(Canvas* canvas, PredatorState* state) {
    canvas_clear(canvas);

    canvas_draw_str(canvas, 10, 10, app_names[state->selected_app_index]);


    if (state->selected_app_index == 0) { // SubScanApp
        SubScanAppContext* context = (SubScanAppContext*)state->app_data;
        char rssi_str[64];
        snprintf(rssi_str, sizeof(rssi_str), "RSSI: %.2f dBm", context->current_rssi);
        canvas_draw_str(canvas, 10, 55, context->nfc_protocol_info);
    }
}

static void predator_app_input_callback(InputEvent* input_event, void* context) {
    PredatorState* state = context;

    if(input_event->type == InputTypePress) {
        switch(input_event->key) {
            case InputKeyUp:
                if(state->selected_app_index > 0) state->selected_app_index--;
                break;
            case InputKeyDown:
                if(state->selected_app_index < NumberOfApps - 1) state->selected_app_index++;
                break;
            default:
                state->should_quit = true;
                break;
        }
    }
}


int predator_app() {
    PredatorState state = {0};
    state.selected_app_index = 0;
    state.should_quit = false;

    // Initialize app_data for SubScanApp
    SubScanAppContext* app_context = malloc(sizeof(SubScanAppContext));
    state.app_data = app_context;

    SubGhzTxRx* txrx = subghz_txrx_alloc(); // Create SubGhzTxRx instance
    // Start the receiver
    subghz_txrx_rx_start(txrx);
    
    Nfc* nfc = nfc_alloc(); // Assuming you have a way to allocate an Nfc instance
    NfcScanner* nfc_scanner = nfc_scanner_alloc(nfc);

    nfc_scanner_start(nfc_scanner, nfc_scanner_callback, app_context);



    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, (ViewPortDrawCallback)predator_app_draw, &state);
    view_port_input_callback_set(view_port, predator_app_input_callback, &state);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(!state.should_quit) {
        if (state.selected_app_index == 0) {
            // Update RSSI for SubScanApp
            update_rssi(txrx, (SubScanAppContext*)state.app_data);
        }

        view_port_update(view_port);
        furi_delay_ms(100); // Update every 100 ms, adjust as needed
    }

    // Cleanup
    free(app_context);
    subghz_txrx_free(txrx);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    nfc_scanner_stop(nfc_scanner);
    nfc_scanner_free(nfc_scanner);
    nfc_free(nfc);
    return 0;
}