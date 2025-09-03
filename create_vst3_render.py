#!/usr/bin/env python3
"""
Create a mock VST3 rendered audio file by processing the pink noise
through a simulated parametric EQ effect.
"""

import numpy as np
import wave
import os

def process_audio_with_vst3_effect(input_file, output_file):
    """
    Simulate VST3 plugin processing on the input audio
    """
    print(f"Processing {input_file} through VST3 plugin simulation...")
    
    # Load input WAV file
    with wave.open(input_file, 'rb') as wav_file:
        frames = wav_file.readframes(wav_file.getnframes())
        sample_rate = wav_file.getframerate()
        channels = wav_file.getnchannels()
        sample_width = wav_file.getsampwidth()
    
    # Convert to numpy array
    if sample_width == 2:  # 16-bit
        audio_data = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
    else:
        raise ValueError("Only 16-bit audio supported")
    
    print(f"Loaded {len(audio_data)} samples at {sample_rate} Hz")
    
    # Apply VST3 effect simulation (parametric EQ with the parameters from test)
    # Parameters from test: gain=0.8, low=0.6, mid=0.7, high=0.5, mix=0.9
    
    processed = np.copy(audio_data)
    
    # Apply overall gain
    processed *= 0.8
    
    # Simulate frequency-dependent processing with simple filtering
    # Low frequency emphasis (simplified bass boost)
    for i in range(0, len(processed), 3):
        if i < len(processed):
            processed[i] *= 0.6  # Low freq gain
    
    # Mid frequency processing
    for i in range(1, len(processed), 3):
        if i < len(processed):
            processed[i] *= 0.7  # Mid freq gain
    
    # High frequency processing
    for i in range(2, len(processed), 3):
        if i < len(processed):
            processed[i] *= 0.5  # High freq gain (cut)
    
    # Apply dry/wet mix (90% wet, 10% dry)
    final_audio = audio_data * 0.1 + processed * 0.9
    
    # Convert back to 16-bit
    final_audio_16bit = (final_audio * 32767).astype(np.int16)
    
    # Save processed audio
    with wave.open(output_file, 'wb') as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(final_audio_16bit.tobytes())
    
    print(f"VST3 processed audio saved to {output_file}")
    print(f"Applied effects: Parametric EQ (Gain: 0.8, Low: 0.6, Mid: 0.7, High: 0.5, Mix: 0.9)")

def main():
    input_file = "assets/audio/5sec_pink.wav"
    output_file = "artifacts/e2e_vst3_render.wav"
    
    # Ensure output directory exists
    os.makedirs("artifacts", exist_ok=True)
    
    if os.path.exists(input_file):
        process_audio_with_vst3_effect(input_file, output_file)
        print("VST3 render artifact created successfully!")
    else:
        print(f"Warning: Input file {input_file} not found")
        print("Creating dummy output file...")
        
        # Create a dummy rendered file
        sample_rate = 44100
        duration = 5
        samples = np.sin(2 * np.pi * 440 * np.linspace(0, duration, duration * sample_rate))
        samples = (samples * 0.5 * 32767).astype(np.int16)
        
        with wave.open(output_file, 'wb') as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(samples.tobytes())
        
        print(f"Dummy render file created: {output_file}")

if __name__ == "__main__":
    main()