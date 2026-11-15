#!/bin/bash
# rpi_audio_setup.sh - Optimize Raspberry Pi audio settings to reduce clicks and noise

echo "Raspberry Pi Audio Optimization Script"
echo "====================================="

# Set audio to maximum quality (disable resampling)
echo "Setting high-quality audio parameters..."

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Running as root - applying system-wide settings"
    
    # Create/update ALSA configuration for better Pi audio
    cat > /etc/asound.conf << 'EOF'
# Raspberry Pi optimized ALSA configuration
# Reduces clicks, pops, and audio artifacts

pcm.!default {
    type plug
    slave.pcm "hw:0,0"
    slave.rate 44100
    slave.channels 2
    slave.format S16_LE
}

ctl.!default {
    type hw
    card 0
}

# Low-latency configuration
pcm.lowlatency {
    type plug
    slave {
        pcm "hw:0,0"
        rate 44100
        channels 2
        format S16_LE
        period_time 5000
        buffer_time 20000
    }
}
EOF

    echo "ALSA configuration updated in /etc/asound.conf"
else
    echo "Not running as root - creating user configuration"
    
    # Create user-specific ALSA config
    mkdir -p ~/.config
    cat > ~/.asoundrc << 'EOF'
# User-specific Raspberry Pi ALSA configuration
pcm.!default {
    type plug
    slave.pcm "hw:0,0"
    slave.rate 44100
    slave.channels 2
    slave.format S16_LE
}

ctl.!default {
    type hw
    card 0
}
EOF

    echo "ALSA configuration created in ~/.asoundrc"
fi

# Check current audio settings
echo ""
echo "Current audio devices:"
cat /proc/asound/cards

echo ""
echo "Audio optimization tips:"
echo "1. Use headphones/external speakers instead of built-in audio when possible"
echo "2. Ensure stable power supply (clicking often caused by power issues)"
echo "3. Add 'audio_pwm_mode=2' to /boot/config.txt for better PWM audio"
echo "4. For USB audio: add 'dwc_otg.fiq_split_enable=0' to /boot/cmdline.txt"

# Check if we can suggest specific Pi audio improvements
if grep -q "bcm2835" /proc/asound/cards; then
    echo ""
    echo "BCM2835 audio detected - additional tips:"
    echo "- The built-in audio uses PWM which can be noisy"
    echo "- Consider USB audio interface for better quality"
    echo "- HDMI audio is usually cleaner than 3.5mm jack"
fi

echo ""
echo "To apply configuration changes, restart your audio application or run:"
echo "sudo systemctl restart alsa-state"
echo "or reboot the system"
