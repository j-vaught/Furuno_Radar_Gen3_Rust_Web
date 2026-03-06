#!/usr/bin/env python3
"""Plot status=1 and status=4 echo spokes as separate PPI radar images."""

import sys
import csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap

# Brand colors
GARNET = '#73000A'
BLACK = '#000000'
WHITE = '#FFFFFF'
ATLANTIC = '#466A9F'

# Custom colormap: black -> garnet -> white
cmap_echo = LinearSegmentedColormap.from_list('radar', [
    (0.0, BLACK),
    (0.3, '#1F414D'),   # Congaree
    (0.5, ATLANTIC),
    (0.7, GARNET),
    (1.0, WHITE),
])

def load_csv(path):
    """Load echo CSV, return dict of status -> list of (angle, echo_array)."""
    spokes = {1: [], 4: []}
    with open(path, 'r') as f:
        reader = csv.reader(f)
        next(reader)  # skip header
        for row in reader:
            status = int(row[0])
            angle = int(row[4])
            echo = np.array([int(x) for x in row[5:]], dtype=np.float32)
            if status in spokes:
                spokes[status].append((angle, echo))
    return spokes

def render_ppi(spokes, title, output_path, max_radius=868):
    """Render a PPI plot from a list of (angle_ticks, echo_array) tuples."""
    size = max_radius * 2
    img = np.zeros((size, size), dtype=np.float32)
    cx, cy = max_radius, max_radius

    for angle_ticks, echo in spokes:
        angle_deg = angle_ticks * 360.0 / 4096.0
        angle_rad = np.radians(90.0 - angle_deg)  # north-up

        n = len(echo)
        if n == 0:
            continue

        for r_idx in range(n):
            r = r_idx
            x = int(cx + r * np.cos(angle_rad))
            y = int(cy - r * np.sin(angle_rad))
            if 0 <= x < size and 0 <= y < size:
                img[y, x] = max(img[y, x], echo[r_idx])

    fig, ax = plt.subplots(figsize=(10, 10), facecolor=BLACK)
    ax.set_facecolor(BLACK)

    vmax = 252.0 if img.max() > 0 else 1.0
    ax.imshow(img, cmap=cmap_echo, vmin=0, vmax=vmax,
              extent=[-max_radius, max_radius, -max_radius, max_radius],
              interpolation='nearest', origin='lower')

    # Range rings
    for r in range(200, max_radius, 200):
        circle = plt.Circle((0, 0), r, fill=False, color='#5C5C5C',
                            linewidth=0.5, linestyle='--')
        ax.add_patch(circle)

    ax.set_xlim(-max_radius, max_radius)
    ax.set_ylim(-max_radius, max_radius)
    ax.set_aspect('equal')
    ax.set_title(title, color=WHITE, fontsize=16, fontweight='bold', pad=15)
    ax.tick_params(colors='#5C5C5C', labelsize=8)
    for spine in ax.spines.values():
        spine.set_edgecolor('#363636')

    n_spokes = len(spokes)
    vals = [e.max() for _, e in spokes if len(e) > 0]
    avg_max = np.mean(vals) if vals else 0
    info = f"{n_spokes} spokes | avg peak: {avg_max:.0f}"
    ax.text(0.02, 0.02, info, transform=ax.transAxes, color='#A2A2A2',
            fontsize=9, verticalalignment='bottom')

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, facecolor=BLACK, edgecolor='none',
                bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: {output_path} ({n_spokes} spokes)")

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else (
        '/home/user/Code_dev/RADAR_web/main/runtime/output/raw/csv/20260306_125844_351.csv')
    out_dir = '/home/user/Code_dev/RADAR_web/reverse/'

    spokes = load_csv(csv_path)

    render_ppi(spokes[1], 'Status=1 (Long Pulse / Coarse)',
               out_dir + 'ppi_status1_long_pulse.png')
    render_ppi(spokes[4], 'Status=4 (Short Pulse / Fine)',
               out_dir + 'ppi_status4_short_pulse.png')

    # Combined side-by-side
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(20, 10), facecolor=BLACK)

    for ax, status, title in [(ax1, 1, 'Status=1 (Long Pulse)'),
                               (ax2, 4, 'Status=4 (Short Pulse)')]:
        size = 868 * 2
        img = np.zeros((size, size), dtype=np.float32)
        cx, cy = 868, 868
        for angle_ticks, echo in spokes[status]:
            angle_deg = angle_ticks * 360.0 / 4096.0
            angle_rad = np.radians(90.0 - angle_deg)
            for r_idx in range(len(echo)):
                x = int(cx + r_idx * np.cos(angle_rad))
                y = int(cy - r_idx * np.sin(angle_rad))
                if 0 <= x < size and 0 <= y < size:
                    img[y, x] = max(img[y, x], echo[r_idx])

        ax.set_facecolor(BLACK)
        ax.imshow(img, cmap=cmap_echo, vmin=0, vmax=252,
                  extent=[-868, 868, -868, 868],
                  interpolation='nearest', origin='lower')
        for r in range(200, 868, 200):
            circle = plt.Circle((0, 0), r, fill=False, color='#5C5C5C',
                                linewidth=0.5, linestyle='--')
            ax.add_patch(circle)
        ax.set_xlim(-868, 868)
        ax.set_ylim(-868, 868)
        ax.set_aspect('equal')
        ax.set_title(title, color=WHITE, fontsize=14, fontweight='bold')
        ax.tick_params(colors='#5C5C5C', labelsize=7)
        for spine in ax.spines.values():
            spine.set_edgecolor('#363636')
        ax.text(0.02, 0.02, f"{len(spokes[status])} spokes",
                transform=ax.transAxes, color='#A2A2A2', fontsize=9,
                verticalalignment='bottom')

    plt.tight_layout()
    plt.savefig(out_dir + 'ppi_dual_pulse_comparison.png', dpi=150,
                facecolor=BLACK, edgecolor='none',
                bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: {out_dir}ppi_dual_pulse_comparison.png")

if __name__ == '__main__':
    main()
