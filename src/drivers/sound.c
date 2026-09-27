#include "sound.h"
#include "common.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool sb16_initialized = false;
static uint8_t sb16_version_major = 0;
static uint8_t sb16_version_minor = 0;
static dma_buffer_t dma_buffer;
static uint8_t current_volume = 128; // Global volume setting

// Serial debug output functions
extern void serial_puts(const char *msg);

static void serial_printf_str(const char *msg) {
    serial_puts(msg);
}

static void serial_printf_int(const char *prefix, int value, const char *suffix) {
    char buffer[256];
    sprintf(buffer, "%s%d%s", prefix, value, suffix);
    serial_puts(buffer);
}

static void serial_printf_hex(const char *prefix, int value, const char *suffix) {
    char buffer[256];
    sprintf(buffer, "%s0x%02X%s", prefix, value, suffix);
    serial_puts(buffer);
}

// Debug function to check volume
void debug_current_volume(const char* location) {
    serial_printf_str("[SB16] Volume check at ");
    serial_printf_str(location);
    serial_printf_str(": ");
    serial_printf_int("", current_volume, "/15\n");
}

// DMA Controller functions
static void setup_dma_8bit(uint32_t addr, uint16_t count) {
    // Mask DMA channel 1
    outb(DMA1_MASK_REG, 0x05); // Mask channel 1
    
    // Clear flip-flop
    outb(DMA1_CLEAR_FF, 0x00);
    
    // Set mode (single transfer, address increment, read transfer)
    outb(DMA1_MODE_REG, 0x49); // Channel 1, single transfer, read
    
    // Set address (low byte, high byte)
    outb(DMA1_CH1_ADDR, addr & 0xFF);
    outb(DMA1_CH1_ADDR, (addr >> 8) & 0xFF);
    
    // Set page (high byte of address)
    outb(DMA1_CH1_PAGE, (addr >> 16) & 0xFF);
    
    // Set count (low byte, high byte) - count is length-1
    count--;
    outb(DMA1_CH1_COUNT, count & 0xFF);
    outb(DMA1_CH1_COUNT, (count >> 8) & 0xFF);
    
    // Unmask DMA channel 1
    outb(DMA1_MASK_REG, 0x01);
}

static void setup_dma_16bit(uint32_t addr, uint16_t count) {
    serial_printf_str("[SB16] Setting up 16-bit DMA - original addr=");
    serial_printf_hex("", addr, ", count=");
    serial_printf_int("", count, "\n");
    
    // Ensure address is word-aligned for 16-bit DMA
    if (addr & 1) {
        serial_printf_str("[SB16] WARNING: Address not word-aligned!\n");
        addr &= ~1; // Force word alignment
    }
    
    // For 16-bit DMA on ISA bus, addresses are still in bytes but count is in words
    uint32_t byte_addr = addr;
    uint16_t word_count = count / 2; // Convert byte count to word count
    
    serial_printf_str("[SB16] 16-bit DMA: byte_addr=");
    serial_printf_hex("", byte_addr, ", word_count=");
    serial_printf_int("", word_count, "\n");
    
    // Mask DMA channel 5 (16-bit channel)
    outb(DMA2_MASK_REG, 0x05); // Mask channel 5
    
    // Clear flip-flop
    outb(DMA2_CLEAR_FF, 0x00);
    
    // Set mode: Channel 1 (5-4), single transfer, address increment, read transfer
    outb(DMA2_MODE_REG, 0x45); // Channel 5, single transfer, read, address increment
    
    // Set address (low byte, high byte) - address in bytes
    outb(DMA2_CH5_ADDR, byte_addr & 0xFF);
    outb(DMA2_CH5_ADDR, (byte_addr >> 8) & 0xFF);
    
    // Set page (bits 16-23 of address)
    outb(DMA2_CH5_PAGE, (byte_addr >> 16) & 0xFF);
    
    // Set count (low byte, high byte) - count in words minus 1
    word_count--;
    outb(DMA2_CH5_COUNT, word_count & 0xFF);
    outb(DMA2_CH5_COUNT, (word_count >> 8) & 0xFF);
    
    // Unmask DMA channel 5
    outb(DMA2_MASK_REG, 0x01);
    
    serial_printf_str("[SB16] 16-bit DMA setup complete\n");
    serial_printf_int("count=", (count + 1) << 1, "\n");
}

// Allocate DMA buffer in low memory (below 16MB for ISA DMA)
static bool allocate_dma_buffer(uint32_t size) {
    // For simplicity, use a static buffer in low memory
    // In a real OS, you'd use proper DMA-capable memory allocation
    static uint8_t static_dma_buffer[32768]; // Increased to 32KB to support larger buffers
    
    if (size > sizeof(static_dma_buffer)) {
        printf("SB16: ERROR - Requested DMA buffer too large (max %d bytes)\n", (int)sizeof(static_dma_buffer));
        return false;
    }
    
    dma_buffer.virtual_addr = static_dma_buffer;
    dma_buffer.physical_addr = (void*)static_dma_buffer; // Identity mapping in kernel
    dma_buffer.size = size;
    dma_buffer.channel = 1; // Use channel 1 for 8-bit audio
    
    return true;
}

// Proper delay function using system timer with fallback
extern int32_t get_tick_count();
static void timer_delay_ms(uint32_t ms) {
    int32_t start_tick = get_tick_count();
    uint32_t target_ticks = ms / 10; // Assuming 100Hz timer (10ms per tick)
    
    uint32_t timeout_counter = 0;
    const uint32_t max_timeout = 1000000; // Fallback timeout
    
    while ((get_tick_count() - start_tick) < (int32_t)target_ticks) {
        timeout_counter++;
        
        // Fallback timeout to prevent infinite hang
        if (timeout_counter >= max_timeout) {
            // Fall back to simple I/O delay
            for (uint32_t i = 0; i < ms; i++) {
                for (uint32_t j = 0; j < 1000; j++) {
                    inb(0x80);
                }
            }
            break;
        }
    }
}

// Simple PC speaker test (independent of Sound Blaster)
void pc_speaker_beep(uint16_t frequency, uint32_t duration_ms) {
    serial_printf_str("PC_SPEAKER: Testing basic PC speaker beep\n");
    serial_printf_int("PC_SPEAKER: Frequency ", frequency, " Hz for ");
    serial_printf_int("", duration_ms, " ms\n");
    
    // Calculate PIT divisor for frequency
    uint16_t divisor = 1193180 / frequency;
    
    serial_printf_int("PC_SPEAKER: PIT divisor = ", divisor, "\n");
    
    // Set PIT channel 2 (speaker) frequency
    outb(0x43, 0xB6);  // Configure PIT channel 2
    outb(0x42, divisor & 0xFF);        // Low byte
    outb(0x42, (divisor >> 8) & 0xFF); // High byte
    
    // Enable speaker
    uint8_t speaker_port = inb(0x61);
    serial_printf_hex("PC_SPEAKER: Current speaker port = ", speaker_port, "\n");
    
    outb(0x61, speaker_port | 0x03);
    serial_printf_str("PC_SPEAKER: Speaker enabled\n");
    
    // Wait for duration using timer
    timer_delay_ms(duration_ms);
    
    // Disable speaker
    outb(0x61, speaker_port & 0xFC);
    serial_printf_str("PC_SPEAKER: Speaker disabled\n");
    serial_printf_str("PC_SPEAKER: PC speaker test complete\n");
}

// Wait for DMA transfer to complete
static bool wait_dma_complete(uint32_t timeout_ms) {
    serial_printf_str("SB16: Waiting for DMA completion...\n");
    
    // Check DMA status by reading DSP status
    for (uint32_t i = 0; i < timeout_ms; i++) {
        // Check if DSP is ready (not busy)
        if (!(inb(SB16_DSP_READ_STATUS) & 0x80)) {
            serial_printf_int("SB16: DMA completed after ", i, " ms\n");
            return true;
        }
        
        // Simple 1ms delay
        for (uint32_t j = 0; j < 1000; j++) {
            inb(0x80);
        }
    }
    
    serial_printf_str("SB16: DMA completion timeout\n");
    return false;
}

// Wait for DSP to be ready for writing
static bool dsp_wait_write(void) {
    int timeout = 10000;
    while (timeout-- > 0) {
        if (!(inb(SB16_DSP_WRITE_DATA) & 0x80)) {
            return true;
        }
    }
    return false;
}

// Wait for DSP to have data ready for reading
static bool dsp_wait_read(void) {
    int timeout = 10000;
    while (timeout-- > 0) {
        if (inb(SB16_DSP_READ_STATUS) & 0x80) {
            return true;
        }
    }
    return false;
}

// Write a command to the DSP
static bool dsp_write(uint8_t data) {
    if (!dsp_wait_write()) {
        return false;
    }
    outb(SB16_DSP_WRITE_DATA, data);
    return true;
}

// Read data from the DSP
static uint8_t dsp_read(void) {
    if (!dsp_wait_read()) {
        return 0xFF;
    }
    uint8_t result = inb(SB16_DSP_READ_DATA);
    return result;
}

// Reset the DSP
void sb16_reset(void) {
    outb(SB16_DSP_RESET, 1);
    
    // Wait at least 3 microseconds
    for (int i = 0; i < 100; i++) {
        inb(0x80); // I/O delay
    }
    
    outb(SB16_DSP_RESET, 0);
    
    // Wait for DSP to acknowledge reset (should return 0xAA)
    uint8_t response = dsp_read();
    if (response != 0xAA) {
        printf("SB16: Reset failed, got 0x%02X instead of 0xAA\n", response);
    }
}

// Detect Sound Blaster 16
bool sb16_detect(void) {
    // Reset the DSP
    sb16_reset();
    
    // Get DSP version
    if (!dsp_write(DSP_CMD_GET_VERSION)) {
        return false;
    }
    
    sb16_version_major = dsp_read();
    sb16_version_minor = dsp_read();
    
    // SB16 should report version 4.x
    if (sb16_version_major < 4) {
        return false;
    }
    
    printf("SB16: Detected version %d.%02d\n", sb16_version_major, sb16_version_minor);
    return true;
}

// Set mixer register
static void mixer_write(uint8_t reg, uint8_t value) {
    serial_printf_str("[SB16] Mixer write: reg=0x");
    serial_printf_hex("", reg, ", value=0x");
    serial_printf_hex("", value, "\n");
    
    outb(SB16_MIXER_ADDRESS, reg);
    outb(SB16_MIXER_DATA, value);
}

// Read mixer register
static uint8_t mixer_read(uint8_t reg) {
    outb(SB16_MIXER_ADDRESS, reg);
    return inb(SB16_MIXER_DATA);
}

// Initialize Sound Blaster 16
bool sb16_init(void) {
    if (sb16_initialized) {
        return true;
    }
    
    printf("SB16: Initializing Sound Blaster 16...\n");
    
    if (!sb16_detect()) {
        printf("SB16: No Sound Blaster 16 detected\n");
        return false;
    }
    
    // Reset mixer
    mixer_write(MIXER_RESET, 0);
    
    // Set very low initial volumes for better volume control range (20%)
    mixer_write(MIXER_MASTER_VOLUME, 0x33); // Both channels 20%
    mixer_write(MIXER_PCM_VOLUME, 0x33);    // Both channels 20%
    
    // Enable voice/PCM output (some SB16 cards need this)
    mixer_write(0x0E, 0x02); // Output mixer - enable voice
    
    serial_printf_str("[SB16] Mixer volumes set to 20%\n");
    
    // Allocate DMA buffer for audio operations (32KB for high-quality audio)
    printf("SB16: Allocating 32KB DMA buffer...\n");
    if (!allocate_dma_buffer(32768)) {
        printf("SB16: Failed to allocate DMA buffer\n");
        return false;
    }
    printf("SB16: 32KB DMA buffer allocated successfully\n");
    
    // Turn on speaker (once during init)
    printf("SB16: Turning speaker on...\n");
    sb16_speaker_on();
    
    sb16_initialized = true;
    printf("SB16: Initialization complete\n");
    return true;
}

// Turn speaker on
void sb16_speaker_on(void) {
    dsp_write(DSP_CMD_SPEAKER_ON);
}

// Turn speaker off
void sb16_speaker_off(void) {
    dsp_write(DSP_CMD_SPEAKER_OFF);
}

// Set audio format
bool sb16_set_format(audio_format_t* format) {
    if (!sb16_initialized) {
        return false;
    }
    
    // Set sample rate (for 16-bit audio)
    if (format->bits_per_sample == 16) {
        dsp_write(DSP_CMD_SET_OUTPUT_RATE);
        dsp_write((format->sample_rate >> 8) & 0xFF); // High byte
        dsp_write(format->sample_rate & 0xFF);        // Low byte
    } else {
        // For 8-bit audio, use time constant
        uint8_t time_constant = 256 - (1000000 / format->sample_rate);
        dsp_write(DSP_CMD_SET_TIME_CONSTANT);
        dsp_write(time_constant);
    }
    
    return true;
}

// Set volume (0-255)
void sb16_set_volume(uint8_t volume) {
    if (!sb16_initialized) {
        return;
    }
    
    serial_printf_str("[SB16] Volume command received: ");
    serial_printf_int("", volume, "/255\n");
    
    // Store the volume for consistency and actually use it
    current_volume = volume;
    
    // Use the actual volume parameter instead of hardcoded 50%
    uint8_t mixer_value = volume; // Use the provided volume directly
    
    mixer_write(MIXER_MASTER_VOLUME, mixer_value);
    mixer_write(MIXER_PCM_VOLUME, mixer_value);
    
    serial_printf_str("[SB16] Volume set to: 0x");
    serial_printf_hex("", mixer_value, "\n");
}

// Generate a simple beep using high-quality 8-bit audio with perfect zero-crossing
void sb16_beep(uint16_t frequency, uint32_t duration_ms) {
    serial_printf_str("[SB16] Playing ");
    serial_printf_int("", frequency, "Hz for ");
    serial_printf_int("", duration_ms, "ms (8-bit 22kHz zero-crossing)\n");
    
    if (!sb16_initialized) {
        serial_printf_str("[SB16] Not initialized!\n");
        return;
    }
    
    // Use pre-allocated DMA buffer from init
    uint32_t buffer_size = 4096;
    if (!dma_buffer.virtual_addr) {
        serial_printf_str("[SB16] No DMA buffer allocated!\n");
        return;
    }
    
    // High quality 8-bit audio at 22kHz
    uint8_t* audio_buffer = (uint8_t*)dma_buffer.virtual_addr;
    uint32_t sample_rate = 22050;
    
    // Calculate samples for EXACT complete cycles (guarantees zero crossings)
    uint32_t samples_per_cycle = sample_rate / frequency;
    uint32_t max_cycles = buffer_size / samples_per_cycle;
    uint32_t complete_cycles = max_cycles; // Use maximum complete cycles that fit
    uint32_t samples_for_cycles = complete_cycles * samples_per_cycle;
    
    // Ensure we don't exceed buffer bounds
    if (samples_for_cycles > buffer_size) {
        complete_cycles--;
        samples_for_cycles = complete_cycles * samples_per_cycle;
    }
    
    // Generate perfect sine wave with GUARANTEED zero crossings + minimal fade
    for (uint32_t i = 0; i < samples_for_cycles; i++) {
        // Phase from 0 to 2*PI*complete_cycles (guarantees zero at start and end)
        double phase = 2.0 * M_PI * complete_cycles * (double)i / (double)samples_for_cycles;
        double sample = sin(phase);
        
        // Apply volume control with linear scaling for predictable volume response
        double volume_scale = (double)current_volume / 255.0;
        
        // Very short fade envelope (just 2 samples at each end) to smooth transitions
        double envelope = 1.0;
        if (i == 0) {
            envelope = 0.0; // Start at zero
        } else if (i == 1) {
            envelope = 0.5; // Gentle ramp up
        } else if (i == samples_for_cycles - 2) {
            envelope = 0.5; // Gentle ramp down
        } else if (i == samples_for_cycles - 1) {
            envelope = 0.0; // End at zero
        }
        
        // Convert to unsigned 8-bit with very small amplitude for proper volume scaling
        audio_buffer[i] = (uint8_t)(128 + (sample * 2 * volume_scale * envelope));
    }
    
    // Fill remaining buffer with exact silence
    for (uint32_t i = samples_for_cycles; i < buffer_size; i++) {
        audio_buffer[i] = 128; // Exact center for unsigned 8-bit
    }
    
    // Speaker is already on from init
    
    // Set higher sample rate using time constant method
    uint8_t time_constant = 256 - (1000000 / sample_rate);
    if (!dsp_write(DSP_CMD_SET_TIME_CONSTANT) || !dsp_write(time_constant)) {
        serial_printf_str("[SB16] Failed to set sample rate!\n");
        return;
    }
    
    // Setup 8-bit DMA
    setup_dma_8bit((uint32_t)dma_buffer.physical_addr, buffer_size);
    
    // Use single-cycle 8-bit playback (more reliable than auto-init for short clips)
    if (!dsp_write(DSP_CMD_PLAY_8BIT)) {
        serial_printf_str("[SB16] Failed to start playback!\n");
        return;
    }
    
    // Send data length (low byte, high byte) - length is size-1
    uint16_t dma_length = buffer_size - 1;
    if (!dsp_write(dma_length & 0xFF) || !dsp_write((dma_length >> 8) & 0xFF)) {
        serial_printf_str("[SB16] Failed to send data length!\n");
        return;
    }
    
    // Wait for playback to complete (single cycle only)
    uint32_t cycle_duration_ms = (buffer_size * 1000) / sample_rate;
    timer_delay_ms(cycle_duration_ms);
    
    serial_printf_str("[SB16] 8-bit playback complete\n");
}

// Generate and play a tone (defaults to 16-bit stereo 44.1kHz)
void sb16_play_tone(uint16_t frequency, uint32_t duration_ms) {
    // Use high-quality 16-bit stereo by default
    sb16_beep_16bit(frequency, duration_ms);
}

// High-quality 16-bit stereo beep function at 44.1kHz
void sb16_beep_16bit(uint16_t frequency, uint32_t duration_ms) {
    debug_current_volume("start of sb16_beep_16bit");
    
    serial_printf_str("[SB16] Playing ");
    serial_printf_int("", frequency, "Hz for ");
    serial_printf_int("", duration_ms, "ms (16-bit stereo 44.1kHz)\n");
    
    if (!sb16_initialized) {
        serial_printf_str("[SB16] Not initialized!\n");
        return;
    }
    
    // Use the DMA buffer for audio (32KB for high-quality stereo)
    uint32_t buffer_size = 32768;
    if (!dma_buffer.virtual_addr) {
        serial_printf_str("[SB16] No DMA buffer allocated!\n");
        return;
    }
    
    // 16-bit stereo audio at 44.1kHz
    int16_t* audio_buffer = (int16_t*)dma_buffer.virtual_addr;
    uint32_t sample_rate = 44100;
    uint32_t total_samples = buffer_size / 4; // 4 bytes per stereo sample (2x 16-bit)
    
    // Calculate samples for EXACT complete cycles (guarantees zero crossings)
    uint32_t samples_per_cycle = sample_rate / frequency;
    uint32_t max_cycles = total_samples / samples_per_cycle;
    uint32_t complete_cycles = max_cycles; // Use maximum complete cycles that fit
    uint32_t samples_for_cycles = complete_cycles * samples_per_cycle;
    
    // Ensure we don't exceed buffer bounds
    if (samples_for_cycles > total_samples) {
        complete_cycles--;
        samples_for_cycles = complete_cycles * samples_per_cycle;
    }
    
    // Ensure we have at least a few cycles for proper audio
    if (complete_cycles < 3) {
        serial_printf_str("[SB16] Warning: Only ");
        serial_printf_int("", complete_cycles, " cycles in buffer - audio may be very short\n");
    }
    
    serial_printf_str("[SB16] Freq=");
    serial_printf_int("", frequency, "Hz, samples_per_cycle=");
    serial_printf_int("", samples_per_cycle, ", total_samples=");
    serial_printf_int("", total_samples, "\n");
    serial_printf_str("[SB16] Current volume: ");
    serial_printf_int("", current_volume, "/255\n");
    serial_printf_str("[SB16] Generating ");
    serial_printf_int("", complete_cycles, " complete cycles, ");
    serial_printf_int("", samples_for_cycles, " samples\n");
    
    // Generate perfect sine wave with GUARANTEED zero crossings + minimal fade
    for (uint32_t i = 0; i < samples_for_cycles; i++) {
        // Phase from 0 to 2*PI*complete_cycles (guarantees zero at start and end)
        double phase = 2.0 * M_PI * complete_cycles * (double)i / (double)samples_for_cycles;
        double sample = sin(phase);
        
        // Simple envelope to prevent clicks - just fade the first and last few samples
        double envelope = 1.0;
        uint32_t fade_samples = 4; // Very short fade to just prevent clicks
        
        if (i < fade_samples) {
            // Linear fade in - start from 0.2 instead of 0 to avoid silent samples
            envelope = 0.2 + 0.8 * (double)(i + 1) / (double)(fade_samples + 1);
        } else if (i >= samples_for_cycles - fade_samples) {
            // Linear fade out
            uint32_t fade_pos = i - (samples_for_cycles - fade_samples);
            envelope = 0.2 + 0.8 * (double)(fade_samples - fade_pos) / (double)(fade_samples + 1);
        }
        
        // Software volume control since QEMU ignores SB16 mixer
        int16_t sample_value;
        
        if (current_volume == 0) {
            // Complete silence for volume 0
            sample_value = 0;
        } else {
            // Just play at full volume - let the host OS control volume
            sample_value = (int16_t)(sample * 8000 * envelope);
        }
        
        // Debug first few samples to see actual amplitudes
        if (i < 3) {
            serial_printf_str("[SB16] Sample ");
            serial_printf_int("", i, ": ");
            serial_printf_int("", sample_value, " (vol=");
            serial_printf_int("", current_volume, "/255)\n");
        }
        
        audio_buffer[i * 2] = sample_value;     // Left channel
        audio_buffer[i * 2 + 1] = sample_value; // Right channel (same as left)
    }
    
    // Fill remaining buffer with silence (0 = center for signed 16-bit stereo)
    for (uint32_t i = samples_for_cycles * 2; i < total_samples * 2; i++) {
        audio_buffer[i] = 0; // Stereo silence
    }
    
    // Speaker is already on from init
    
    serial_printf_str("[SB16] Setting 16-bit sample rate to 44.1kHz...\n");
    // For 16-bit mode, use the proper sample rate command (0x41 for output)
    if (!dsp_write(0x41) ||                           // Set output sample rate command
        !dsp_write((sample_rate >> 8) & 0xFF) ||      // High byte first (0xAC for 44100)
        !dsp_write(sample_rate & 0xFF)) {             // Low byte second (0x44 for 44100)
        serial_printf_str("[SB16] Failed to set 16-bit sample rate!\n");
        return;
    }
    
    serial_printf_str("[SB16] Setting up 16-bit DMA...\n");
    // Setup 16-bit DMA (uses DMA channel 5)
    setup_dma_16bit((uint32_t)dma_buffer.physical_addr, buffer_size);
    
    serial_printf_str("[SB16] Starting 16-bit stereo playback...\n");
    // Use single-cycle for more reliable playback
    uint16_t word_length = (buffer_size / 2) - 1; // Convert bytes to words, -1 for SB format
    
    // Calculate how many cycles we need for the requested duration
    uint32_t buffer_duration_ms = (total_samples * 1000) / sample_rate;
    
    serial_printf_str("[SB16] Buffer duration: ");
    serial_printf_int("", buffer_duration_ms, "ms, requested: ");
    serial_printf_int("", duration_ms, "ms\n");
    
    if (duration_ms <= buffer_duration_ms) {
        // Single playback is enough - no looping needed!
        serial_printf_str("[SB16] Single cycle sufficient\n");
        
        if (!dsp_write(0xB0) ||                           // 16-bit single-cycle DMA command
            !dsp_write(0x20) ||                           // 16-bit stereo signed PCM format
            !dsp_write(word_length & 0xFF) ||             // Low byte of word length
            !dsp_write((word_length >> 8) & 0xFF)) {      // High byte of word length
            serial_printf_str("[SB16] Failed to start 16-bit stereo playback!\n");
            return;
        }
        
        // Wait for the requested duration (no more, no less)
        timer_delay_ms(duration_ms);
    } else {
        // Need multiple cycles
        uint32_t cycles_needed = (duration_ms + buffer_duration_ms - 1) / buffer_duration_ms;
        serial_printf_str("[SB16] Need ");
        serial_printf_int("", cycles_needed, " cycles\n");
        
        // Play the required number of cycles
        for (uint32_t cycle = 0; cycle < cycles_needed; cycle++) {
            if (!dsp_write(0xB0) ||                           // 16-bit single-cycle DMA command
                !dsp_write(0x20) ||                           // 16-bit stereo signed PCM format
                !dsp_write(word_length & 0xFF) ||             // Low byte of word length
                !dsp_write((word_length >> 8) & 0xFF)) {      // High byte of word length
                serial_printf_str("[SB16] Failed to start 16-bit stereo playback!\n");
                return;
            }
            
            // Wait for this cycle to complete (or partial cycle for the last one)
            uint32_t this_cycle_duration = buffer_duration_ms;
            if (cycle == cycles_needed - 1) {
                // Last cycle might be partial
                uint32_t remaining_ms = duration_ms - (cycle * buffer_duration_ms);
                if (remaining_ms < buffer_duration_ms) {
                    this_cycle_duration = remaining_ms;
                }
            }
            
            timer_delay_ms(this_cycle_duration);
        }
    }
    
    serial_printf_str("[SB16] 16-bit playback complete\n");
}

// Play audio buffer (simplified version without full DMA setup)
bool sb16_play_buffer(void* buffer, uint32_t length) {
    if (!sb16_initialized) {
        return false;
    }
    
    printf("SB16: Playing %d byte buffer\n", length);
    
    // This is a simplified implementation
    // A full implementation would need DMA setup
    // For now, just simulate playback duration
    
    uint32_t duration_ms = (length * 1000) / (22050 * 2); // Rough estimate
    timer_delay_ms(duration_ms);
    
    return true;
}

// Test functions
void sb16_test_beep(void) {
    printf("SB16: Testing 16-bit stereo beep sounds (44.1kHz)...\n");
    
    sb16_beep_16bit(440, 500);  // A4 note for 500ms
    timer_delay_ms(100);
    sb16_beep_16bit(523, 500);  // C5 note for 500ms
    timer_delay_ms(100);
    sb16_beep_16bit(659, 500);  // E5 note for 500ms
    
    printf("SB16: 16-bit stereo beep test complete\n");
}

void sb16_test_tones(void) {
    printf("SB16: Testing 16-bit stereo tone generation (44.1kHz)...\n");
    
    uint16_t frequencies[] = {261, 294, 330, 349, 392, 440, 494, 523}; // C major scale
    
    for (int i = 0; i < 8; i++) {
        printf("Playing note %d Hz\n", frequencies[i]);
        sb16_beep_16bit(frequencies[i], 300);  // Use 16-bit stereo by default
        timer_delay_ms(100);
    }
    
    printf("SB16: 16-bit stereo tone test complete\n");
}

// Simple test with longer duration and simpler audio
void sb16_test_simple_beep(void) {
    printf("SB16: Testing simple long beep (raw square wave)...\n");
    
    if (!sb16_detect()) {
        printf("SB16: No Sound Blaster detected\n");
        return;
    }
    
    // Allocate larger buffer for longer audio
    if (!allocate_dma_buffer(4096)) {
        printf("SB16: Failed to allocate DMA buffer\n");
        return;
    }
    
    // Generate simple square wave - very obvious sound
    uint8_t* audio_buffer = (uint8_t*)dma_buffer.virtual_addr;
    uint32_t buffer_size = 4096;
    
    // Create 440Hz square wave at 8kHz sample rate
    // 8000 / 440 = ~18 samples per cycle
    uint32_t samples_per_cycle = 18;
    
    printf("SB16: Generating simple square wave (18 samples per cycle)\n");
    for (uint32_t i = 0; i < buffer_size; i++) {
        if ((i % samples_per_cycle) < (samples_per_cycle / 2)) {
            audio_buffer[i] = 200;  // High volume
        } else {
            audio_buffer[i] = 56;   // Low volume (avoid silence which might be filtered)
        }
    }
    
    // Print first few samples for debugging
    printf("SB16: First 16 samples: ");
    for (int i = 0; i < 16; i++) {
        printf("0x%02X ", audio_buffer[i]);
    }
    printf("\n");
    
    // Speaker is already on from init - no need to turn on/off
    
    // Use very simple 8kHz sample rate
    uint8_t time_constant = 256 - (1000000 / 8000);
    printf("SB16: Setting 8kHz sample rate (time constant = %d)\n", time_constant);
    
    if (!dsp_write(DSP_CMD_SET_TIME_CONSTANT) || !dsp_write(time_constant)) {
        printf("SB16: ERROR - Failed to set sample rate\n");
        return;
    }
    
    // Setup DMA
    setup_dma_8bit((uint32_t)dma_buffer.physical_addr, buffer_size);
    
    // Use single-cycle DMA (more reliable than auto-init)
    printf("SB16: Starting single-cycle DMA playback (4096 bytes = 512ms at 8kHz)\n");
    
    if (!dsp_write(DSP_CMD_PLAY_8BIT)) {
        printf("SB16: ERROR - Failed to send play command\n");
        return;
    }
    
    // Send data length (low byte, high byte) - length is size-1
    uint16_t dma_length = buffer_size - 1;
    if (!dsp_write(dma_length & 0xFF) || !dsp_write((dma_length >> 8) & 0xFF)) {
        printf("SB16: ERROR - Failed to send data length\n");
        return;
    }
    
    printf("SB16: DMA transfer started - should hear 440Hz square wave for ~512ms\n");
    
    // Wait for playback (buffer size in ms at 8kHz)
    uint32_t duration_ms = (buffer_size * 1000) / 8000;
    printf("SB16: Waiting %d ms for playback to complete...\n", duration_ms);
    timer_delay_ms(duration_ms + 200); // Add extra time
    
    // Speaker stays on - no need to turn off
    
    printf("SB16: Simple beep test complete\n");
}

// Simple single-cycle DMA test (more reliable than auto-init)
void sb16_test_simple_dma(void) {
    printf("SB16: Testing simple single-cycle DMA...\n");
    
    if (!sb16_detect()) {
        printf("SB16: No Sound Blaster detected for DMA test\n");
        return;
    }
    
    // Allocate a small buffer
    if (!allocate_dma_buffer(1024)) {
        printf("SB16: Failed to allocate DMA buffer\n");
        return;
    }
    
    // Generate a simple 440Hz sine wave
    uint8_t* audio_buffer = (uint8_t*)dma_buffer.virtual_addr;
    uint32_t sample_rate = 8000;
    uint32_t buffer_size = 1024;
    
    for (uint32_t i = 0; i < buffer_size; i++) {
        double t = (double)i / sample_rate;
        double sample = sin(2.0 * 3.14159 * 440.0 * t);
        audio_buffer[i] = (uint8_t)((sample + 1.0) * 127.5);  // Convert to unsigned 8-bit
    }
    
    // Speaker is already on from init - no need to turn on/off
    
    // Set sample rate using time constant
    uint8_t time_constant = 256 - (1000000 / sample_rate);
    if (!dsp_write(DSP_CMD_SET_TIME_CONSTANT) || !dsp_write(time_constant)) {
        printf("SB16: ERROR - Failed to set sample rate\n");
        return;
    }
    
    // Setup DMA
    setup_dma_8bit((uint32_t)dma_buffer.physical_addr, buffer_size);
    
    // Use single-cycle DMA (simpler and more reliable)
    printf("SB16: Starting single-cycle DMA playback\n");
    
    if (!dsp_write(DSP_CMD_PLAY_8BIT)) {
        printf("SB16: ERROR - Failed to send single-cycle play command\n");
        return;
    }
    
    // Send data length (low byte, high byte) - length is size-1
    uint16_t dma_length = buffer_size - 1;
    if (!dsp_write(dma_length & 0xFF) || !dsp_write((dma_length >> 8) & 0xFF)) {
        printf("SB16: ERROR - Failed to send data length\n");
        return;
    }
    
    printf("SB16: Single-cycle DMA started, playing 440Hz tone\n");
    
    // Wait for playback to complete (buffer duration)
    uint32_t duration_ms = (buffer_size * 1000) / sample_rate;
    timer_delay_ms(duration_ms + 100); // Add a bit extra
    
    // Speaker stays on - no need to turn off
    
    printf("SB16: Simple DMA test complete\n");
}