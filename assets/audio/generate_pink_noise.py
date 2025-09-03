#!/usr/bin/env python3
"""
Generate a 5-second pink noise audio file for VST3 testing
"""

import numpy as np
import wave
import struct

def generate_pink_noise(duration_seconds=5, sample_rate=44100):
    """Generate pink noise using the Voss-McCartney algorithm"""
    num_samples = int(duration_seconds * sample_rate)
    
    # Number of random sources
    num_sources = 16
    
    # Initialize random sources
    sources = np.zeros(num_sources)
    
    # Output buffer
    output = np.zeros(num_samples)
    
    # Generate pink noise
    for i in range(num_samples):
        # Update sources based on binary counter
        update_mask = i & (~i + 1)  # Get rightmost set bit
        
        for j in range(num_sources):
            if update_mask & (1 << j):
                sources[j] = np.random.uniform(-1, 1)
        
        # Sum all sources
        output[i] = np.sum(sources) / num_sources
    
    # Normalize to prevent clipping
    max_val = np.max(np.abs(output))
    if max_val > 0:
        output = output / max_val * 0.8  # Leave some headroom
    
    return output

def save_wav(filename, audio_data, sample_rate=44100):
    """Save audio data to WAV file"""
    # Convert to 16-bit PCM
    audio_16bit = (audio_data * 32767).astype(np.int16)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_16bit.tobytes())

if __name__ == "__main__":
    print("Generating 5-second pink noise...")
    
    # Generate pink noise
    pink_noise = generate_pink_noise(duration_seconds=5, sample_rate=44100)
    
    # Save to file
    save_wav("5sec_pink.wav", pink_noise)
    
    print("Pink noise saved to 5sec_pink.wav")
    print(f"Duration: 5 seconds")
    print(f"Sample rate: 44100 Hz")
    print(f"Channels: 1 (mono)")
    print(f"Bit depth: 16-bit")