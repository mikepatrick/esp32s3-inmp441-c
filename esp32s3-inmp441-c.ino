#include <Arduino.h>
#include <driver/i2s_std.h>

// Your verified pins from MicroPython
#define I2S_SCK_PIN 4
#define I2S_WS_PIN 5
#define I2S_SD_PIN 6

i2s_chan_handle_t rx_handle;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("--- Starting Hardware-Aligned I2S Test ---");

  // 1. Allocate the hardware I2S channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);

  // 2. Configure 64-clock timing (Stereo layout, 32-bit slot container)
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),  // 16kHz
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_SCK_PIN,
      .ws = (gpio_num_t)I2S_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = (gpio_num_t)I2S_SD_PIN,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false } }
  };

  // Force both slots active to ensure the clock hits exactly 64 pulses per frame
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;

  if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) {
    Serial.println("I2S Initialization failed!");
    while (1)
      ;
  }

  // Turn on the clock lines
  i2s_channel_enable(rx_handle);
  Serial.println("Microphone hardware clock active. Speak now!");
}

void loop() {
  int32_t stereo_sample[2] = { 0 };
  size_t bytes_read = 0;

  // Read the raw 8-byte stereo chunk
  esp_err_t err = i2s_channel_read(rx_handle, &stereo_sample, sizeof(stereo_sample), &bytes_read, 100);

  if (err == ESP_OK && bytes_read == sizeof(stereo_sample)) {
    // Grab the left channel raw container
    int32_t raw_sample = stereo_sample[0];

    // CORRECT SIGN EXTENSION: Because the driver padded zeroes at the bottom,
    // the data is already aligned to the absolute top of the 32-bit container.
    // We just shift it down by 8 bits to restore its true 24-bit signed integer value!
    int32_t true_sample = raw_sample >> 8;

    // Print the raw waveform value to the Serial Plotter
    if (true_sample > 1000000) {
      Serial.println(true_sample);
    }
  }
}