#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>

// Sound Blaster 16 I/O ports (base 0x220)
#define SB16_BASE           0x220
#define SB16_DSP_RESET      (SB16_BASE + 0x06)
#define SB16_DSP_READ_DATA  (SB16_BASE + 0x0A)
#define SB16_DSP_WRITE_DATA (SB16_BASE + 0x0C)
#define SB16_DSP_READ_STATUS (SB16_BASE + 0x0E)
#define SB16_MIXER_ADDRESS  (SB16_BASE + 0x04)
#define SB16_MIXER_DATA     (SB16_BASE + 0x05)

// DMA Controller ports
#define DMA1_MASK_REG       0x0A
#define DMA1_MODE_REG       0x0B
#define DMA1_CLEAR_FF       0x0C
#define DMA1_CH1_ADDR       0x02
#define DMA1_CH1_COUNT      0x03
#define DMA1_CH1_PAGE       0x83

#define DMA2_MASK_REG       0xD4
#define DMA2_MODE_REG       0xD6
#define DMA2_CLEAR_FF       0xD8
#define DMA2_CH5_ADDR       0xC4
#define DMA2_CH5_COUNT      0xC6
#define DMA2_CH5_PAGE       0x8B

// DSP Commands
#define DSP_CMD_SET_TIME_CONSTANT    0x40
#define DSP_CMD_SET_OUTPUT_RATE      0x41
#define DSP_CMD_SET_INPUT_RATE       0x42
#define DSP_CMD_SPEAKER_ON           0xD1
#define DSP_CMD_SPEAKER_OFF          0xD3
#define DSP_CMD_PAUSE_DMA            0xD0
#define DSP_CMD_RESUME_DMA           0xD4
#define DSP_CMD_GET_VERSION          0xE1
#define DSP_CMD_PLAY_8BIT            0x14
#define DSP_CMD_PLAY_16BIT           0xB0
#define DSP_CMD_AUTO_INIT_8BIT       0x1C
#define DSP_CMD_AUTO_INIT_16BIT      0xB6

// Mixer registers
#define MIXER_MASTER_VOLUME     0x22
#define MIXER_PCM_VOLUME        0x04
#define MIXER_RESET             0x00

// Audio format structure
typedef struct {
    uint16_t sample_rate;    // 8000-44100 Hz
    uint8_t bits_per_sample; // 8 or 16
    uint8_t channels;        // 1 (mono) or 2 (stereo)
} audio_format_t;

// DMA buffer structure
typedef struct {
    void* physical_addr;
    void* virtual_addr;
    uint32_t size;
    uint8_t channel;
} dma_buffer_t;

// Function declarations
bool sb16_init(void);
bool sb16_detect(void);
void sb16_reset(void);
bool sb16_set_format(audio_format_t* format);
bool sb16_play_buffer(void* buffer, uint32_t length);
void sb16_set_volume(uint8_t volume);
void sb16_speaker_on(void);
void sb16_speaker_off(void);
void debug_current_volume(const char* location);

// Simple sound generation
void sb16_beep(uint16_t frequency, uint32_t duration_ms);
void sb16_beep_16bit(uint16_t frequency, uint32_t duration_ms);
void sb16_play_tone(uint16_t frequency, uint32_t duration_ms);

// Test functions
void sb16_test_beep(void);
void sb16_test_tones(void);
void sb16_test_simple_dma(void);
void sb16_test_simple_beep(void);

// PC Speaker functions (independent test)
void pc_speaker_beep(uint16_t frequency, uint32_t duration_ms);

#endif // SOUND_H