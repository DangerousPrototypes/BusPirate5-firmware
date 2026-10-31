#!/usr/bin/env python3
"""
SeismicStream + SM-24 geophone vibration tool.

Captures vibration data, identifies spectral peaks, and compares
measurements between locations/conditions for source hunting.

Uses narrowband FFT (0.1 Hz resolution) to find clear tonal peaks —
like the professional report's "dominant frequency: 9 Hz" — plus
broadband comparisons across frequency bands.

Capture:
    python geophone.py COM35 300 -o kitchen_wall      # 5 min capture
    python geophone.py COM35 300 -o bedroom_wall

Analyze (shows peaks + bands):
    python geophone.py --from-file kitchen_wall_counts.npy

Consistency check — split one capture into chunks and overlay:
    python geophone.py --from-file data.npy --chunks 5 --plot
    (splits 5 min capture into 5x 60s, overlays spectra)

Compare locations (split each into chunks for self-consistency):
    python geophone.py --ab kitchen.npy bedroom.npy --chunks 3 --plot
    (each file split into 3 pieces → 6 traces overlaid)

Wiring:
    SM-24 geophone → SeismicStream Input 2 (terminal blocks)
    SeismicStream USB-C → PC

Prerequisites:
    pip install numpy scipy pyserial
    pip install matplotlib  (only if using --plot)
"""
import argparse
import sys
import time
import numpy as np
from scipy import signal as sig

# ── SeismicStream config ──
GAIN_UV = {1: 0.64, 2: 0.32, 4: 0.16, 8: 0.08, 16: 0.04}  # µV per count
GAIN_CMD = {1: b'1', 2: b'2', 4: b'4', 8: b'8', 16: b'6'}
RATE_CMD = {25: b'a', 50: b'b', 100: b'c'}

HP_CUTOFF = 3.0  # Hz, high-pass filter

# Frequency bands for broadband overview
BANDS = [
    ("8-10",   8, 10),
    ("10-12", 10, 12.5),
    ("12-16", 12.5, 16),
    ("16-20", 16, 20),
    ("20-25", 20, 25),
    ("25-32", 25, 31.5),
    ("32-40", 31.5, 40),
    ("40-49", 40, 49),
]


def capture(port, duration, gain=16, sample_rate=100):
    """Capture ASCII integer samples from SeismicStream."""
    import serial

    n_target = int(duration * sample_rate)
    ser = serial.Serial(port, 115200, timeout=2)
    ser.reset_input_buffer()
    ser.write(b'y')       # Input 2
    time.sleep(0.1)
    if gain in GAIN_CMD:
        ser.write(GAIN_CMD[gain])
        time.sleep(0.1)
    if sample_rate in RATE_CMD:
        ser.write(RATE_CMD[sample_rate])
        time.sleep(0.1)
    time.sleep(0.5)
    ser.reset_input_buffer()

    print(f"Capturing {n_target} samples ({duration}s at {sample_rate} SPS, x{gain} gain)...")
    samples = []
    t_start = time.time()
    while len(samples) < n_target:
        if time.time() - t_start > duration + 10:
            print(f"Timeout: got {len(samples)}/{n_target}")
            break
        line = ser.readline()
        if not line:
            continue
        try:
            samples.append(int(line.strip()))
        except ValueError:
            continue
        if len(samples) % (sample_rate * 10) == 0:
            print(f"  {len(samples)}/{n_target} ({time.time()-t_start:.0f}s)")
    ser.close()
    counts = np.array(samples, dtype=np.float64)
    print(f"Captured {len(counts)} samples ({len(counts)/sample_rate:.1f}s)")
    return counts


# ── Signal processing ──

def _split_counts(counts, n_chunks, sample_rate):
    """Split raw counts into n_chunks equal pieces. Returns list of arrays."""
    chunk_len = len(counts) // n_chunks
    # Ensure each chunk is at least 20s for decent frequency resolution
    if chunk_len < 20 * sample_rate:
        actual = max(1, len(counts) // (20 * sample_rate))
        print(f"  Warning: {n_chunks} chunks too short, using {actual} instead")
        n_chunks = actual
        chunk_len = len(counts) // n_chunks
    pieces = []
    for i in range(n_chunks):
        pieces.append(counts[i * chunk_len:(i + 1) * chunk_len])
    return pieces


def _hp_filter(voltage, sample_rate):
    """Apply high-pass filter and remove DC."""
    voltage = voltage - np.mean(voltage)
    sos = sig.butter(4, HP_CUTOFF, btype='high', fs=sample_rate, output='sos')
    return sig.sosfiltfilt(sos, voltage)


def _band_rms(voltage, sample_rate, f_lo, f_hi):
    """RMS voltage in a frequency band via bandpass filter."""
    sos = sig.butter(4, [f_lo, f_hi], btype='band', fs=sample_rate, output='sos')
    return np.sqrt(np.mean(sig.sosfiltfilt(sos, voltage)**2))


def _welch_psd(voltage, sample_rate):
    """Welch PSD with 0.1 Hz resolution. Returns (freqs, psd) in V²/Hz."""
    nperseg = 10 * sample_rate  # 10s segments → 0.1 Hz bins
    if len(voltage) < 2 * nperseg:
        nperseg = len(voltage) // 2
    return sig.welch(voltage, fs=sample_rate, nperseg=nperseg,
                     window='hann', scaling='density', detrend='linear')


def _find_peaks(freqs, asd_uv, f_min=5.0, f_max=49.0, min_prominence_db=3.0):
    """Find spectral peaks in amplitude spectral density.

    Returns list of (freq_hz, asd_uv_per_rthz, prominence_db)
    sorted by prominence (strongest first).
    """
    mask = (freqs >= f_min) & (freqs <= f_max)
    f = freqs[mask]
    a = asd_uv[mask]
    if len(a) < 5:
        return []

    a_db = 20 * np.log10(a + 1e-30)
    df = f[1] - f[0]
    min_dist = max(1, int(1.0 / df))  # at least 1 Hz between peaks

    indices, props = sig.find_peaks(a_db, distance=min_dist,
                                    prominence=min_prominence_db)
    peaks = []
    for idx, prom in zip(indices, props['prominences']):
        peaks.append((f[idx], a[idx], float(prom)))
    peaks.sort(key=lambda x: -x[2])
    return peaks


def _db(val, ref):
    """20*log10(val/ref), safe."""
    return 20 * np.log10(val / ref + 1e-30)


def _process(counts, gain, sample_rate):
    """Full analysis: HP-filter → bands + PSD + peak detection."""
    uv_per_count = GAIN_UV[gain]
    voltage = counts * uv_per_count * 1e-6
    voltage = _hp_filter(voltage, sample_rate)
    total_rms = np.sqrt(np.mean(voltage**2))

    # Band RMS
    band_rms = {}
    for name, f_lo, f_hi in BANDS:
        band_rms[name] = _band_rms(voltage, sample_rate, f_lo, f_hi)

    # Chunk consistency — 10s chunks
    chunk_len = 10 * sample_rate
    n_chunks = max(1, len(voltage) // chunk_len)
    chunk_band_rms = {name: [] for name, _, _ in BANDS}
    for c in range(n_chunks):
        chunk = voltage[c * chunk_len:(c + 1) * chunk_len]
        for name, f_lo, f_hi in BANDS:
            chunk_band_rms[name].append(_band_rms(chunk, sample_rate, f_lo, f_hi))

    # PSD + peak detection
    freqs, psd = _welch_psd(voltage, sample_rate)
    asd_uv = np.sqrt(psd) * 1e6  # µV/√Hz
    peaks = _find_peaks(freqs, asd_uv)

    return {
        'voltage': voltage,
        'total_rms': total_rms,
        'band_rms': band_rms,
        'chunk_band_rms': chunk_band_rms,
        'n_chunks': n_chunks,
        'freqs': freqs,
        'asd_uv': asd_uv,
        'peaks': peaks,
    }


def _asd_at(result, freq):
    """Look up ASD level (µV/√Hz) at a specific frequency."""
    idx = np.argmin(np.abs(result['freqs'] - freq))
    return result['asd_uv'][idx]


# ── Analysis ──

def analyze(counts, sample_rate=100, gain=16, prefix="vibration", plot=False):
    """Single capture: show detected peaks + band overview."""
    N = len(counts)
    duration = N / sample_rate
    r = _process(counts, gain, sample_rate)

    print(f"\n{'='*60}")
    print(f"  {prefix}: {duration:.0f}s, x{gain} gain, {sample_rate} SPS")
    print(f"  Total RMS (8-49 Hz): {r['total_rms']*1e6:.1f} µV")
    print(f"{'='*60}")

    # ── Peaks — the main output ──
    print(f"\n  DETECTED PEAKS (0.1 Hz resolution):")
    if r['peaks']:
        max_level = max(p[1] for p in r['peaks'][:10])
        print(f"  {'Freq':>8} {'Level':>12} {'Prominence':>12}  Spectrum")
        print(f"  {'-'*58}")
        for i, (freq, level, prom) in enumerate(r['peaks'][:10]):
            bar_len = max(1, int(level / max_level * 30))
            bar = "█" * bar_len
            tag = " ← DOMINANT" if i == 0 else ""
            print(f"  {freq:7.1f}Hz {level:10.4f}µV/√Hz {prom:+10.1f}dB   {bar}{tag}")
    else:
        print(f"  (no clear peaks — flat/noisy spectrum)")

    # ── Band overview ──
    print(f"\n  BAND OVERVIEW:")
    max_rms = max(r['band_rms'].values())
    print(f"  {'Band':<8} {'µV RMS':>8} {'Rel':>8} {'Spread':>8}")
    print(f"  {'-'*38}")
    for name, f_lo, f_hi in BANDS:
        rms = r['band_rms'][name]
        rel = _db(rms, max_rms)
        chunks = r['chunk_band_rms'][name]
        if len(chunks) > 1:
            spread = _db(max(chunks), min(chunks) + 1e-30)
            spread_str = f"±{spread/2:.1f}dB"
        else:
            spread_str = "  ---"
        bar = "█" * max(0, int((rel + 30) / 1.5))
        print(f"  {name:<8} {rms*1e6:8.2f} {rel:+7.1f}dB {spread_str:>8}  {bar}")

    # ── Consistency ──
    if r['n_chunks'] > 1:
        spreads = [_db(max(r['chunk_band_rms'][n]), min(r['chunk_band_rms'][n]) + 1e-30)
                   for n, _, _ in BANDS]
        avg = np.mean(spreads)
        if avg > 6:
            print(f"\n  ⚠ HIGH VARIATION ({avg:.1f}dB) — unsteady signal or coupling issue")
        elif avg > 3:
            print(f"\n  Moderate variation ({avg:.1f}dB spread)")
        else:
            print(f"\n  Stable ({avg:.1f}dB spread, {r['n_chunks']} chunks)")

    if plot:
        _plot_spectrum(r, prefix)
    print()


def compare_ab(file_list, labels, sample_rate=100, gain=16, plot=False):
    """A/B comparison with peak tracking + band comparison.

    The first file is the reference. Shows how each peak changed
    between captures — the key output for confirming vibrations stopped.
    """
    results = []
    for path, label in zip(file_list, labels):
        counts = np.load(path)
        r = _process(counts, gain, sample_rate)
        r['label'] = label
        r['n'] = len(counts)
        results.append(r)

    ref = results[0]

    print(f"\n{'='*70}")
    print(f"  A/B COMPARISON — reference: {ref['label']}")
    print(f"{'='*70}")

    # ── Overall ──
    print(f"\n  OVERALL (8-49 Hz):")
    for r in results:
        dur = r['n'] / sample_rate
        if r is ref:
            print(f"    {r['label']:>15}: {r['total_rms']*1e6:7.1f} µV  "
                  f"(ref, {dur:.0f}s)")
        else:
            d = _db(r['total_rms'], ref['total_rms'])
            tag = "LOUDER" if d > 1.5 else ("QUIETER" if d < -1.5 else "~SAME")
            print(f"    {r['label']:>15}: {r['total_rms']*1e6:7.1f} µV  "
                  f"{d:+5.1f}dB {tag} ({dur:.0f}s)")

    # ── Peak tracking — the money shot ──
    # Collect all peak frequencies from all captures, merge nearby (±0.5 Hz)
    all_peak_freqs = []
    for r in results:
        for freq, level, prom in r['peaks']:
            all_peak_freqs.append(freq)
    # Cluster nearby peaks
    merged = []
    for f in sorted(all_peak_freqs):
        if not merged or f - merged[-1] > 0.8:
            merged.append(f)
        else:
            merged[-1] = (merged[-1] + f) / 2  # average nearby

    if merged:
        print(f"\n  PEAK TRACKING (narrowband, 0.1 Hz resolution):")
        hdr = f"  {'Freq':>8}  {ref['label']:>14}"
        for r in results[1:]:
            hdr += f"  {r['label']:>14}  {'Change':>8}"
        print(hdr)
        print(f"  {'-'*70}")

        for pf in merged:
            ref_level = _asd_at(ref, pf)
            line = f"  {pf:7.1f}Hz  {ref_level:12.4f}µV"
            for r in results[1:]:
                level = _asd_at(r, pf)
                delta = _db(level, ref_level)
                if delta < -6:
                    tag = "▼▼▼"
                elif delta < -3:
                    tag = " ▼▼"
                elif delta > 6:
                    tag = "▲▲▲"
                elif delta > 3:
                    tag = " ▲▲"
                elif abs(delta) < 1.5:
                    tag = "  ="
                elif delta > 0:
                    tag = "  ▲"
                else:
                    tag = "  ▼"
                line += f"  {level:12.4f}µV  {delta:+6.1f}dB {tag}"
            print(line)

        # Summary: did peaks go away?
        for r in results[1:]:
            gone = []
            reduced = []
            grew = []
            for pf in merged:
                delta = _db(_asd_at(r, pf), _asd_at(ref, pf))
                if delta < -10:
                    gone.append((pf, delta))
                elif delta < -3:
                    reduced.append((pf, delta))
                elif delta > 3:
                    grew.append((pf, delta))

            print(f"\n  {r['label']} vs {ref['label']} — peaks:")
            if gone:
                print(f"    GONE:    {', '.join(f'{f:.1f}Hz ({d:+.0f}dB)' for f,d in gone)}")
            if reduced:
                print(f"    REDUCED: {', '.join(f'{f:.1f}Hz ({d:+.1f}dB)' for f,d in reduced)}")
            if grew:
                print(f"    GREW:    {', '.join(f'{f:.1f}Hz ({d:+.1f}dB)' for f,d in grew)}")
            if not gone and not reduced and not grew:
                print(f"    All peaks unchanged (within ±3 dB)")

    # ── Band comparison ──
    col_w = max(10, max(len(r['label']) for r in results) + 2)
    print(f"\n  BAND COMPARISON:")
    hdr = f"  {'Band':<8}"
    for r in results:
        hdr += f" {r['label']:>{col_w}}"
    for r in results[1:]:
        hdr += f" {'delta':>{col_w}}"
    print(hdr)
    print(f"  {'-'*(8 + (len(results)*2) * (col_w+1))}")

    for name, _, _ in BANDS:
        line = f"  {name:<8}"
        for r in results:
            line += f" {r['band_rms'][name]*1e6:{col_w}.2f}"
        for r in results[1:]:
            d = _db(r['band_rms'][name], ref['band_rms'][name])
            if d > 3:
                m = "▲▲"
            elif d > 1.5:
                m = "▲ "
            elif d < -3:
                m = "▼▼"
            elif d < -1.5:
                m = "▼ "
            else:
                m = "= "
            line += f" {d:+{col_w-3}.1f}dB {m}"
        print(line)

    # ── Consistency ──
    for r in results:
        if r['n_chunks'] > 1:
            spreads = [_db(max(r['chunk_band_rms'][n]),
                           min(r['chunk_band_rms'][n]) + 1e-30)
                       for n, _, _ in BANDS]
            avg = np.mean(spreads)
            if avg > 6:
                print(f"\n  ⚠ {r['label']}: high internal variation ({avg:.1f}dB)")

    if plot:
        _plot_compare(results)
    print()


def _compare_ab_from_counts(count_arrays, labels, sample_rate=100, gain=16, plot=False):
    """Same as compare_ab but takes pre-loaded count arrays instead of file paths."""
    results = []
    for counts, label in zip(count_arrays, labels):
        r = _process(counts, gain, sample_rate)
        r['label'] = label
        r['n'] = len(counts)
        results.append(r)

    ref = results[0]

    print(f"\n{'='*70}")
    print(f"  COMPARISON — reference: {ref['label']} ({len(results)} traces)")
    print(f"{'='*70}")

    # ── Overall ──
    print(f"\n  OVERALL (8-49 Hz):")
    for r in results:
        dur = r['n'] / sample_rate
        if r is ref:
            print(f"    {r['label']:>15}: {r['total_rms']*1e6:7.1f} µV  "
                  f"(ref, {dur:.0f}s)")
        else:
            d = _db(r['total_rms'], ref['total_rms'])
            tag = "LOUDER" if d > 1.5 else ("QUIETER" if d < -1.5 else "~SAME")
            print(f"    {r['label']:>15}: {r['total_rms']*1e6:7.1f} µV  "
                  f"{d:+5.1f}dB {tag} ({dur:.0f}s)")

    # ── Peak tracking ──
    all_peak_freqs = []
    for r in results:
        for freq, level, prom in r['peaks']:
            all_peak_freqs.append(freq)
    merged = []
    for f in sorted(all_peak_freqs):
        if not merged or f - merged[-1] > 0.8:
            merged.append(f)
        else:
            merged[-1] = (merged[-1] + f) / 2

    if merged:
        col_w = max(10, max(len(r['label']) for r in results) + 2)
        print(f"\n  PEAK TRACKING:")
        hdr = f"  {'Freq':>8}"
        for r in results:
            hdr += f"  {r['label']:>{col_w}}"
        print(hdr)
        print(f"  {'-'*(8 + len(results) * (col_w + 2))}")
        for pf in merged:
            line = f"  {pf:7.1f}Hz"
            levels = [_asd_at(r, pf) for r in results]
            for lv in levels:
                line += f"  {lv:{col_w}.4f}"
            # Show spread across all traces
            spread = _db(max(levels), min(levels) + 1e-30)
            if spread > 6:
                line += f"  spread:{spread:.0f}dB ⚠"
            elif spread > 3:
                line += f"  spread:{spread:.1f}dB"
            else:
                line += f"  ±{spread/2:.1f}dB ✓"
            print(line)

    if plot:
        _plot_compare(results)
    print()


# ── Optional plotting (only imported with --plot) ──

def _plot_spectrum(r, prefix):
    """Single capture spectrum plot with peak annotations."""
    import matplotlib.pyplot as plt

    mask = (r['freqs'] >= 5) & (r['freqs'] <= 49)
    f = r['freqs'][mask]
    a = r['asd_uv'][mask]
    a_db = 20 * np.log10(a + 1e-30)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8))

    # Linear scale
    ax1.plot(f, a, 'b-', linewidth=1)
    for freq, level, prom in r['peaks'][:8]:
        ax1.annotate(f'{freq:.1f} Hz', (freq, level),
                     textcoords="offset points", xytext=(5, 10), fontsize=9,
                     color='red', arrowprops=dict(arrowstyle='->', color='red'))
    ax1.set_ylabel('µV/√Hz')
    ax1.set_title(f'Amplitude Spectrum — {prefix}')
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(5, 49)

    # dB scale — easier to see smaller peaks
    ax2.plot(f, a_db, 'b-', linewidth=1)
    for freq, level, prom in r['peaks'][:8]:
        db = 20 * np.log10(level + 1e-30)
        ax2.annotate(f'{freq:.1f} Hz\n({prom:.0f}dB prom.)', (freq, db),
                     textcoords="offset points", xytext=(5, 10), fontsize=8,
                     color='red', arrowprops=dict(arrowstyle='->', color='red'))
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('dB re 1 µV/√Hz')
    ax2.set_title(f'Amplitude Spectrum (dB) — {prefix}')
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(5, 49)

    plt.tight_layout()
    plt.savefig(f'{prefix}_spectrum.png', dpi=150)
    print(f"  Saved {prefix}_spectrum.png")
    plt.show()


def _plot_compare(results):
    """Comparison: linear overlay, dB overlay, and delta plot."""
    import matplotlib.pyplot as plt
    colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red',
              'tab:purple', 'tab:brown']
    ref = results[0]

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(14, 11))

    # ── Linear overlay (µV/√Hz) — shows dominant peaks clearly ──
    for i, r in enumerate(results):
        mask = (r['freqs'] >= 5) & (r['freqs'] <= 49)
        ax1.plot(r['freqs'][mask], r['asd_uv'][mask],
                 color=colors[i % len(colors)],
                 linewidth=1.5, alpha=0.8, label=r['label'])
    # Annotate shared peaks from reference
    for freq, level, prom in ref['peaks'][:6]:
        if 5 <= freq <= 49:
            ax1.annotate(f'{freq:.1f}', (freq, level),
                         textcoords="offset points", xytext=(4, 8),
                         fontsize=8, color='dimgray',
                         arrowprops=dict(arrowstyle='->', color='gray',
                                         lw=0.8))
    ax1.set_ylabel('µV/√Hz')
    ax1.set_title('Spectrum Overlay (linear)')
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(5, 49)

    # ── dB overlay — shows smaller peaks ──
    for i, r in enumerate(results):
        mask = (r['freqs'] >= 5) & (r['freqs'] <= 49)
        a_db = 20 * np.log10(r['asd_uv'][mask] + 1e-30)
        ax2.plot(r['freqs'][mask], a_db, color=colors[i % len(colors)],
                 linewidth=1.5, alpha=0.8, label=r['label'])
    ax2.set_ylabel('dB re 1 µV/√Hz')
    ax2.set_title('Spectrum Overlay (dB)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(5, 49)

    # ── Delta plot ──
    ref_mask = (ref['freqs'] >= 5) & (ref['freqs'] <= 49)
    ref_db = 20 * np.log10(ref['asd_uv'][ref_mask] + 1e-30)
    for i, r in enumerate(results[1:], 1):
        mask = (r['freqs'] >= 5) & (r['freqs'] <= 49)
        test_db = 20 * np.log10(r['asd_uv'][mask] + 1e-30)
        if len(test_db) != len(ref_db):
            test_db = np.interp(ref['freqs'][ref_mask], r['freqs'][mask], test_db)
        delta = test_db - ref_db
        ax3.plot(ref['freqs'][ref_mask], delta, color=colors[i % len(colors)],
                 linewidth=1.5, label=f'{r["label"]} − {ref["label"]}')
    ax3.axhline(0, color='gray', linewidth=1)
    ax3.axhspan(-3, 3, color='gray', alpha=0.1, label='±3 dB')
    ax3.set_xlabel('Frequency (Hz)')
    ax3.set_ylabel('Δ dB')
    ax3.set_title(f'Change vs {ref["label"]}')
    ax3.legend(loc='upper right')
    ax3.grid(True, alpha=0.3)
    ax3.set_xlim(5, 49)

    plt.tight_layout()
    plt.savefig('compare_spectrum.png', dpi=150)
    print(f"  Saved compare_spectrum.png")
    plt.show()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='SeismicStream + SM-24 vibration tool '
                    '— detect peaks, compare captures, hunt vibration sources')
    parser.add_argument('port', nargs='?', help='Serial port (e.g., COM35)')
    parser.add_argument('duration', nargs='?', type=int, default=60,
                        help='Capture seconds (default: 60)')
    parser.add_argument('-g', '--gain', type=int, default=16,
                        choices=sorted(GAIN_UV.keys()))
    parser.add_argument('-r', '--rate', type=int, default=100,
                        choices=[25, 50, 100])
    parser.add_argument('-o', '--output', default='vibration',
                        help='Output file prefix')
    parser.add_argument('--from-file', metavar='NPY',
                        help='Re-analyze saved .npy')
    parser.add_argument('--ab', nargs='+', metavar='NPY',
                        help='A/B compare: first=reference')
    parser.add_argument('-n', '--names', nargs='+', metavar='NAME',
                        help='Labels for --ab files')
    parser.add_argument('--plot', action='store_true',
                        help='Show spectrum plot (requires matplotlib)')
    parser.add_argument('--chunks', type=int, default=0, metavar='N',
                        help='Split each capture into N pieces for consistency '
                             'check / overlay (e.g., --chunks 5 for 5 min capture)')
    args = parser.parse_args()

    if args.ab:
        if args.names and len(args.names) == len(args.ab):
            labels = args.names
        else:
            labels = []
            for f in args.ab:
                name = f.replace('_counts.npy', '').replace('.npy', '')
                name = name.split('/')[-1].split('\\')[-1]
                labels.append(name)
        if args.chunks >= 2:
            # Split each file into chunks, then compare all chunks together
            all_files = []
            all_labels = []
            for path, label in zip(args.ab, labels):
                counts = np.load(path)
                pieces = _split_counts(counts, args.chunks, args.rate)
                dur_each = len(pieces[0]) / args.rate
                print(f"  {label}: {len(counts)/args.rate:.0f}s → "
                      f"{len(pieces)} x {dur_each:.0f}s chunks")
                for i, piece in enumerate(pieces):
                    chunk_name = f'{label}_{i+1}'
                    all_files.append(piece)
                    all_labels.append(chunk_name)
            # Use pre-loaded counts directly instead of file paths
            _compare_ab_from_counts(all_files, all_labels,
                                    args.rate, args.gain, args.plot)
        else:
            compare_ab(args.ab, labels, args.rate, args.gain, args.plot)
    elif args.from_file:
        counts = np.load(args.from_file)
        name = args.from_file.replace('_counts.npy', '').replace('.npy', '')
        name = name.split('/')[-1].split('\\')[-1]
        if args.chunks >= 2:
            pieces = _split_counts(counts, args.chunks, args.rate)
            dur_each = len(pieces[0]) / args.rate
            print(f"  {name}: {len(counts)/args.rate:.0f}s → "
                  f"{len(pieces)} x {dur_each:.0f}s chunks")
            all_labels = [f'{name}_{i+1}' for i in range(len(pieces))]
            _compare_ab_from_counts(pieces, all_labels,
                                    args.rate, args.gain, args.plot)
        else:
            analyze(counts, args.rate, args.gain, name, args.plot)
    elif args.port:
        counts = capture(args.port, args.duration, args.gain, args.rate)
        import os
        out = f'{args.output}_counts.npy'
        if os.path.exists(out):
            base = args.output
            i = 1
            while os.path.exists(f'{base}_{i}_counts.npy'):
                i += 1
            out = f'{base}_{i}_counts.npy'
            print(f"  {args.output}_counts.npy exists, saving as {out}")
        np.save(out, counts)
        print(f"Saved {out}")
        if len(counts) > args.rate * 4:
            analyze(counts, args.rate, args.gain, args.output, args.plot)
    else:
        parser.print_help()
        sys.exit(1)
