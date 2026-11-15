#pip install numpy scipy matplotlib

import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

# ====================== PARÂMETROS ======================
sample_rate = 48000      # Hz
duration = 5.0           # segundos totais
tube_length = 3        # metros (mude aqui)
speed_of_sound = 343.0
reflection_coeff = 0.92  # quanto reflete na ponta achatada (0.85 = mais absorção)
damping = 0.995          # amortecimento por sample (paredes + ar)
# =======================================================

t = np.arange(0, duration, 1/sample_rate)
num_samples = len(t)

# Entrada: 20ms de onda quadrada 440 Hz
freq = 440.0
input_signal = np.zeros(num_samples)
samples_20ms = int(0.02 * sample_rate)
input_signal[:samples_20ms] = signal.square(2 * np.pi * freq * t[:samples_20ms], duty=0.5) * 0.8

# Simulação do tubo (digital waveguide - 1D)
# Tubo aberto nas duas extremidades: reflexão na extremidade aberta inverte fase
# delay_samples = tempo de ida simples (L/v)
# Ressonância fundamental: sr / (2 * delay_samples) = v / (2L)
delay_samples = max(1, int(tube_length * sample_rate / speed_of_sound))
buffer = np.zeros(delay_samples)
output = np.zeros(num_samples)

for n in range(num_samples):
    # Onda chegando ao mic (extremidade aberta distante)
    delayed = buffer[0]

    # Extremidade aberta → reflexão com inversão de fase
    reflected = -delayed * reflection_coeff * damping

    # Extremidade do falante: sinal injetado + onda refletida vinda de volta
    current = input_signal[n] + reflected

    # Propaga pelo tubo
    buffer = np.roll(buffer, -1)
    buffer[-1] = current

    # Mic lê a pressão na extremidade distante
    output[n] = delayed

# ====================== PLOTS ======================
fig, axs = plt.subplots(3, 1, figsize=(12, 10))

axs[0].plot(t, output, color='blue', linewidth=0.7)
f0 = speed_of_sound / (2 * tube_length)
axs[0].set_title(f'Resposta Impulsiva do Tubo {tube_length}m | f0≈{f0:.1f} Hz')
axs[0].set_xlabel('Tempo (s)')
axs[0].set_ylabel('Amplitude')
axs[0].grid(True, alpha=0.3)

# NFFT maior → mais resolução em frequência para ver os modos baixos
# Colormap: preto (silêncio) → azul → violeta claro (máximo)
from matplotlib.colors import LinearSegmentedColormap
_tube_cmap = LinearSegmentedColormap.from_list(
    'tube', ['#000000', '#0d0d6b', '#3333cc', '#7744dd', '#bb88ff'], N=256)
axs[1].specgram(output, Fs=sample_rate, NFFT=8192, noverlap=7168, cmap=_tube_cmap)
axs[1].set_title(f'Spectrogram - Tubo {tube_length}m | modos: {f0:.1f}, {2*f0:.1f}, {3*f0:.1f}... Hz')
axs[1].set_xlabel('Tempo (s)')
axs[1].set_ylabel('Frequência (Hz)')
axs[1].set_ylim([0, min(3000, sample_rate / 2)])   # zoom nos modos do tubo
# marca os primeiros 6 harmônicos
for k in range(1, 7):
    axs[1].axhline(k * f0, color='cyan', linewidth=0.6, linestyle='--', alpha=0.7)

freqs = np.fft.rfftfreq(num_samples, 1/sample_rate)
spectrum = np.abs(np.fft.rfft(output))
axs[2].plot(freqs, 20 * np.log10(spectrum + 1e-10), color='blue', linewidth=0.8)
axs[2].set_title('Espectro de Potência (picos = modos ressonantes do tubo)')
axs[2].set_xlabel('Frequência (Hz)')
axs[2].set_ylabel('Magnitude (dB)')
axs[2].set_xlim([0, min(3000, sample_rate / 2)])   # zoom nos modos
for k in range(1, 7):
    axs[2].axvline(k * f0, color='red', linewidth=0.8, linestyle='--', alpha=0.6, label=f'{k}×f0' if k == 1 else '')
axs[2].legend(loc='upper right', fontsize=8)
axs[2].grid(True, alpha=0.3)

plt.tight_layout()
plt.show()          # ou plt.savefig('resultado.png', dpi=200)