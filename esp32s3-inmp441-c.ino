#include <Arduino.h>
// I2S interface for INMP441 microphone
#include <ESP_I2S.h>

// Microphone pins
#define I2S_SCK_PIN  4
#define I2S_WS_PIN   5
#define I2S_SD_PIN   6

i2s_chan_handle_t rx_handle;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("=== FINAL STABILIZATION: FORCING PLL_160M & PHILIPS ===");

    // 1. Allocate channels
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    // 2. Build the standard configuration manually to force the clock source
    i2s_std_config_t std_cfg = {
    .clk_cfg = {
        .sample_rate_hz = 16000,
        .clk_src = I2S_CLK_SRC_PLL_160M, // Force 160MHz internal PLL clock pool
        .mclk_multiple = I2S_MCLK_MULTIPLE_384 // Divisible by 3 to clear 24/32-bit math errors
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO), 
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = (gpio_num_t)I2S_SCK_PIN,
        .ws   = (gpio_num_t)I2S_WS_PIN,
        .dout = I2S_GPIO_UNUSED,
        .din  = (gpio_num_t)I2S_SD_PIN,
        .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
    }
    };

    // Ensure both slots are driven so the clock hits 64 cycles per frame
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH; 

    if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) {
    Serial.println("Failed to initialize standard mode framework!");
    while(1);
    }

    i2s_channel_enable(rx_handle);
    Serial.println("System fully stabilized.");
}

void loop() {
    // Read a stereo frame (Left 32-bit + Right 32-bit = 8 bytes)
    int32_t flash_buffer[2] = {0}; 
    size_t bytes_read = 0;

    // Perform blocking direct read from the DMA buffer
    esp_err_t err = i2s_channel_read(rx_handle, &flash_buffer, sizeof(flash_buffer), &bytes_read, 100);

    if (err == ESP_OK && bytes_read > 0) {
        // Because L/R is grounded, our audio data lives inside the Left channel slot (index 0)
        int32_t raw_sample = flash_buffer[0];

        // Sign extend 24-bit PCM data to standard 32-bit signed integers
        if (raw_sample & 0x00800000) {
            raw_sample |= 0xFF000000;
        }

        // Print output to verify wave activity
        if (raw_sample != 0) {
            Serial.println(raw_sample);
            // Map the large 32-bit values down to an easily readable 0-100 sound level
            int volumeIndicator = map(abs(raw_sample), 0, 8388607, 0, 100);

            // Clap or blow into the mic to watch this value jump
            for(int i=0; i<volumeIndicator; i++) {
            Serial.print("|");
            }
            Serial.println();
        }
    } else {
        // Debug indicator if the physical link drops or breaks
        Serial.println("DMA Buffer underflow or timeout.");
        delay(100);
    }
}
