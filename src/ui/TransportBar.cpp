#include "TransportBar.h"

namespace te = tracktion_engine;

TransportBar::TransportBar(te::Edit& edit)
    : edit(edit)
{
    // Setup Play button
    playButton.setButtonText("Play");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    playButton.onClick = [this] { playClicked(); };
    addAndMakeVisible(playButton);
    
    // Setup Stop button  
    stopButton.setButtonText("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.darker());
    stopButton.onClick = [this] { stopClicked(); };
    addAndMakeVisible(stopButton);
    
    // Setup Loop button
    loopButton.setButtonText("Loop");
    loopButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
    loopButton.onClick = [this] { loopToggled(); };
    addAndMakeVisible(loopButton);
    
    // Setup Record button
    recordButton.setButtonText("Rec");
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    recordButton.onClick = [this] { recordClicked(); };
    addAndMakeVisible(recordButton);
    
    // Start timer for regular updates
    startTimerHz(20); // 20Hz updates for smooth UI
    
    updateTransportState();
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey.darker());
    
    // Draw border
    g.setColour(juce::Colours::lightgrey);
    g.drawRect(getLocalBounds(), 1);
    
    // Draw transport position indicator
    auto& transport = edit.getTransport();
    if (transport.isPlaying())
    {
        g.setColour(juce::Colours::lightgreen);
        g.fillEllipse(10, 10, 8, 8);
    }
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    int buttonWidth = 60;
    int spacing = 5;
    
    // Position buttons horizontally
    playButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(spacing);
    
    stopButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(spacing);
    
    recordButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(spacing * 2);
    
    loopButton.setBounds(area.removeFromLeft(buttonWidth));
}

void TransportBar::timerCallback()
{
    updateTransportState();
}

void TransportBar::updateTransportState()
{
    auto& transport = edit.getTransport();
    bool playing = transport.isPlaying();
    bool recording = transport.isRecording();
    
    if (isPlaying != playing)
    {
        isPlaying = playing;
        playButton.setButtonText(isPlaying ? "Pause" : "Play");
        playButton.setColour(juce::TextButton::buttonColourId, 
                           isPlaying ? juce::Colours::orange.darker() : juce::Colours::green.darker());
        repaint();
    }
    
    // Update record button state
    recordButton.setColour(juce::TextButton::buttonColourId,
                          recording ? juce::Colours::red.brighter() : juce::Colours::red);
    
    // Update loop button state
    loopButton.setToggleState(transport.looping, juce::dontSendNotification);
}

void TransportBar::togglePlayPause()
{
    playClicked();
}

void TransportBar::playClicked()
{
    auto& transport = edit.getTransport();
    
    if (transport.isPlaying())
    {
        transport.stop(false, false);
    }
    else
    {
        transport.play(false);
    }
}

void TransportBar::stopClicked()
{
    auto& transport = edit.getTransport();
    transport.stop(false, false);
    transport.setCurrentPosition(0.0);
}

void TransportBar::loopToggled()
{
    auto& transport = edit.getTransport();
    transport.looping = loopButton.getToggleState();
    
    // Set default loop points if none exist
    if (transport.looping && transport.getLoopRange().isEmpty())
    {
        transport.setLoopRange(te::EditTimeRange(0.0, 8.0)); // 8 second default loop
    }
}

void TransportBar::recordClicked()
{
    auto& transport = edit.getTransport();
    
    if (transport.isRecording())
    {
        transport.stop(false, false);
    }
    else
    {
        // Start recording (requires audio input setup)
        transport.record(false);
    }
}