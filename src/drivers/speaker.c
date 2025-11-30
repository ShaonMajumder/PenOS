#include <drivers/speaker.h>
#include <arch/x86/io.h>
#include <arch/x86/timer.h>

#define PIT_CHANNEL_2   0x42
#define PIT_COMMAND     0x43
#define SPEAKER_PORT    0x61

// Initialize PC speaker
void speaker_init(void) {
    speaker_stop();
}

// Play a tone at the given frequency
void speaker_play(uint32_t frequency) {
    if (frequency == 0) {
        speaker_stop();
        return;
    }
    
    // Calculate PIT divisor (PIT frequency is 1193180 Hz)
    uint32_t divisor = 1193180 / frequency;
    
    // Set PIT channel 2 to square wave mode
    outb(PIT_COMMAND, 0xB6);
    
    // Send frequency divisor
    outb(PIT_CHANNEL_2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL_2, (uint8_t)((divisor >> 8) & 0xFF));
    
    // Enable speaker (bits 0 and 1)
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp | 0x03);
}

// Stop the speaker
void speaker_stop(void) {
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

// Play a beep for a specific duration
void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_play(frequency);
    
    // Delay (approximate - not precise timing)
    for (volatile uint32_t i = 0; i < duration_ms * 10000; i++) {
        __asm__ volatile("nop");
    }
    
    speaker_stop();
}

// Play a startup sound (ascending tones)
void speaker_startup_sound(void) {
    // Play a pleasant startup melody
    speaker_beep(523, 150);  // C5 - longer
    speaker_beep(0, 30);     // Pause
    speaker_beep(659, 150);  // E5 - longer
    speaker_beep(0, 30);     // Pause
    speaker_beep(784, 200);  // G5 - even longer
    speaker_stop();
    
    // Note: In WSL/QEMU without audio hardware, you won't hear this,
    // but the PC speaker driver is executing correctly!
}
