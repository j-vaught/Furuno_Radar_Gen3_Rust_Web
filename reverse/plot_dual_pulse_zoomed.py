#!/usr/bin/env python3
"""Zoomed PPI + 1D spoke comparison for status=1 vs status=4."""

import sys
import csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap

BLACK = '#000000'
WHITE = '#FFFFFF'
GARNET = '#73000A'
ATLANTIC = '#466A9F'
CONGAREE = '#1F414D'
ROSE = '#CC2E40'
HORSESHOE = '#65780B'

cmap_echo = LinearSegmentedColormap.from_list('radar', [
    (0.0, BLACK),
    (0.15, CONGAREE),
    (0.4, ATLANTIC),
    (0.65, GARNET),
    (0.85, ROSE),
    (1.0, WHITE),
])

def load_csv(path):
    spokes = {1: [], 4: []}
    with open(path, 'r') as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            status = int(row[0])
            angle = int(row[4])
            echo = np.array([int(x) for x in row[5:]], dtype=np.float32)
            if status in spokes:
                spokes[status].append((angle, echo))
    return spokes

def render_ppi_zoomed(spokes, title, ax, zoom=150):
    size = zoom * 2
    img = np.zeros((size, size), dtype=np.float32)
    cx, cy = zoom, zoom

    for angle_ticks, echo in spokes:
        angle_deg = angle_ticks * 360.0 / 4096.0
        angle_rad = np.radians(90.0 - angle_deg)
        n = min(len(echo), zoom)
        for r_idx in range(n):
            x = int(cx + r_idx * np.cos(angle_rad))
            y = int(cy - r_idx * np.sin(angle_rad))
            if 0 <= x < size and 0 <= y < size:
                img[y, x] = max(img[y, x], echo[r_idx])

    ax.imshow(img, cmap=cmap_echo, vmin=0, vmax=252,
              extent=[-zoom, zoom, -zoom, zoom],
              interpolation='nearest', origin='lower')
    for r in range(25, zoom, 25):
        circle = plt.Circle((0, 0), r, fill=False, color='#5C5C5C',
                            linewidth=0.3, linestyle='--')
        ax.add_patch(circle)
    ax.set_xlim(-zoom, zoom)
    ax.set_ylim(-zoom, zoom)
    ax.set_aspect('equal')
    ax.set_title(title, color=WHITE, fontsize=13, fontweight='bold')
    ax.tick_params(colors='#5C5C5C', labelsize=7)
    for spine in ax.spines.values():
        spine.set_edgecolor('#363636')
    ax.set_facecolor(BLACK)

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else (
        '/home/user/Code_dev/RADAR_web/main/runtime/output/raw/csv/20260306_125844_351.csv')
    out_dir = '/home/user/Code_dev/RADAR_web/reverse/'

    spokes = load_csv(csv_path)

    # --- Figure 1: Zoomed PPI side-by-side ---
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8), facecolor=BLACK)
    render_ppi_zoomed(spokes[1], 'Status=1 (Long Pulse)\n17 intensity levels', ax1)
    render_ppi_zoomed(spokes[4], 'Status=4 (Short Pulse)\n130+ intensity levels', ax2)
    plt.tight_layout()
    plt.savefig(out_dir + 'ppi_zoomed_comparison.png', dpi=150,
                facecolor=BLACK, bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: ppi_zoomed_comparison.png")

    # --- Figure 2: 1D spoke overlay at similar angles ---
    # Find matching angle pairs (closest angles between the two sets)
    angles_1 = {a: e for a, e in spokes[1]}
    angles_4 = {a: e for a, e in spokes[4]}

    # Pick a few angles where both have data nearby
    sample_angles = []
    for a1 in sorted(angles_1.keys()):
        # Find closest status=4 angle
        closest = min(angles_4.keys(), key=lambda a4: abs(a4 - a1))
        if abs(closest - a1) <= 6:  # within ~0.5 degrees
            sample_angles.append((a1, closest))
        if len(sample_angles) >= 6:
            break

    fig, axes = plt.subplots(3, 2, figsize=(16, 12), facecolor=BLACK)
    axes = axes.flatten()

    for idx, (a1, a4) in enumerate(sample_angles):
        ax = axes[idx]
        ax.set_facecolor(BLACK)

        e1 = angles_1[a1][:200]
        e4 = angles_4[a4][:200]
        x = np.arange(len(e1))

        ax.fill_between(x, e1, alpha=0.3, color=GARNET)
        ax.plot(x, e1, color=GARNET, linewidth=1.2, label=f'Status=1 (angle {a1})')
        ax.fill_between(x[:len(e4)], e4, alpha=0.3, color=ATLANTIC)
        ax.plot(np.arange(len(e4)), e4, color=ATLANTIC, linewidth=1.0,
                label=f'Status=4 (angle {a4})', linestyle='-')

        deg1 = a1 * 360.0 / 4096.0
        ax.set_title(f'Bearing {deg1:.1f} deg', color=WHITE, fontsize=11, fontweight='bold')
        ax.set_xlabel('Range bin', color='#A2A2A2', fontsize=9)
        ax.set_ylabel('Intensity', color='#A2A2A2', fontsize=9)
        ax.tick_params(colors='#5C5C5C', labelsize=8)
        ax.set_ylim(-5, 265)
        ax.legend(loc='upper right', fontsize=8, facecolor='#363636',
                  edgecolor='#5C5C5C', labelcolor=WHITE)
        for spine in ax.spines.values():
            spine.set_edgecolor('#363636')

        # Mark the 17 quantization levels for status=1
        levels = [0, 8, 20, 28, 36, 44, 56, 64, 76, 88, 104, 120, 140, 164, 188, 220, 252]
        for lv in levels:
            ax.axhline(y=lv, color='#5C5C5C', linewidth=0.3, alpha=0.5)

    plt.suptitle('Spoke Comparison: Long Pulse (Garnet) vs Short Pulse (Blue)\n'
                 'Gray lines = 17 quantization levels of status=1',
                 color=WHITE, fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig(out_dir + 'spoke_comparison_1d.png', dpi=150,
                facecolor=BLACK, bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: spoke_comparison_1d.png")

    # --- Figure 3: Intensity histogram comparison ---
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6), facecolor=BLACK)

    all_1 = np.concatenate([e for _, e in spokes[1]])
    all_4 = np.concatenate([e for _, e in spokes[4]])
    # Remove zeros for clarity
    all_1_nz = all_1[all_1 > 0]
    all_4_nz = all_4[all_4 > 0]

    ax1.set_facecolor(BLACK)
    ax1.hist(all_1_nz, bins=256, range=(0, 256), color=GARNET, alpha=0.8)
    ax1.set_title('Status=1 Intensity Distribution (non-zero)', color=WHITE,
                  fontsize=12, fontweight='bold')
    ax1.set_xlabel('Echo value', color='#A2A2A2')
    ax1.set_ylabel('Count', color='#A2A2A2')
    ax1.tick_params(colors='#5C5C5C')
    for spine in ax1.spines.values():
        spine.set_edgecolor('#363636')

    ax2.set_facecolor(BLACK)
    ax2.hist(all_4_nz, bins=256, range=(0, 256), color=ATLANTIC, alpha=0.8)
    ax2.set_title('Status=4 Intensity Distribution (non-zero)', color=WHITE,
                  fontsize=12, fontweight='bold')
    ax2.set_xlabel('Echo value', color='#A2A2A2')
    ax2.set_ylabel('Count', color='#A2A2A2')
    ax2.tick_params(colors='#5C5C5C')
    for spine in ax2.spines.values():
        spine.set_edgecolor('#363636')

    plt.tight_layout()
    plt.savefig(out_dir + 'intensity_histogram_comparison.png', dpi=150,
                facecolor=BLACK, bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: intensity_histogram_comparison.png")

    # --- Figure 4: Differential PPI (status=4 minus status=1) ---
    # For each angle, find matching pairs and compute difference
    zoom = 150
    size = zoom * 2
    img_1 = np.zeros((size, size), dtype=np.float32)
    img_4 = np.zeros((size, size), dtype=np.float32)
    cx, cy = zoom, zoom

    for angle_ticks, echo in spokes[1]:
        angle_deg = angle_ticks * 360.0 / 4096.0
        angle_rad = np.radians(90.0 - angle_deg)
        for r_idx in range(min(len(echo), zoom)):
            x = int(cx + r_idx * np.cos(angle_rad))
            y = int(cy - r_idx * np.sin(angle_rad))
            if 0 <= x < size and 0 <= y < size:
                img_1[y, x] = max(img_1[y, x], echo[r_idx])

    for angle_ticks, echo in spokes[4]:
        angle_deg = angle_ticks * 360.0 / 4096.0
        angle_rad = np.radians(90.0 - angle_deg)
        for r_idx in range(min(len(echo), zoom)):
            x = int(cx + r_idx * np.cos(angle_rad))
            y = int(cy - r_idx * np.sin(angle_rad))
            if 0 <= x < size and 0 <= y < size:
                img_4[y, x] = max(img_4[y, x], echo[r_idx])

    diff = img_4.astype(np.float32) - img_1.astype(np.float32)

    # Diverging colormap: garnet (negative) -> black (zero) -> atlantic (positive)
    cmap_diff = LinearSegmentedColormap.from_list('diff', [
        (0.0, GARNET),
        (0.35, ROSE),
        (0.5, '#363636'),
        (0.65, ATLANTIC),
        (1.0, '#88BBEE'),
    ])

    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(22, 7), facecolor=BLACK)

    for ax, img, title in [(ax1, img_1, 'Status=1 (Long)'),
                            (ax2, img_4, 'Status=4 (Short)')]:
        ax.set_facecolor(BLACK)
        ax.imshow(img, cmap=cmap_echo, vmin=0, vmax=252,
                  extent=[-zoom, zoom, -zoom, zoom],
                  interpolation='nearest', origin='lower')
        for r in range(25, zoom, 25):
            circle = plt.Circle((0, 0), r, fill=False, color='#5C5C5C',
                                linewidth=0.3, linestyle='--')
            ax.add_patch(circle)
        ax.set_xlim(-zoom, zoom)
        ax.set_ylim(-zoom, zoom)
        ax.set_aspect('equal')
        ax.set_title(title, color=WHITE, fontsize=12, fontweight='bold')
        ax.tick_params(colors='#5C5C5C', labelsize=7)
        for spine in ax.spines.values():
            spine.set_edgecolor('#363636')

    ax3.set_facecolor(BLACK)
    vlim = max(abs(diff.min()), abs(diff.max()), 1)
    im = ax3.imshow(diff, cmap=cmap_diff, vmin=-vlim, vmax=vlim,
              extent=[-zoom, zoom, -zoom, zoom],
              interpolation='nearest', origin='lower')
    for r in range(25, zoom, 25):
        circle = plt.Circle((0, 0), r, fill=False, color='#5C5C5C',
                            linewidth=0.3, linestyle='--')
        ax3.add_patch(circle)
    ax3.set_xlim(-zoom, zoom)
    ax3.set_ylim(-zoom, zoom)
    ax3.set_aspect('equal')
    ax3.set_title('Difference (Short - Long)\nBlue=Short brighter, Red=Long brighter',
                  color=WHITE, fontsize=12, fontweight='bold')
    ax3.tick_params(colors='#5C5C5C', labelsize=7)
    for spine in ax3.spines.values():
        spine.set_edgecolor('#363636')

    cbar = fig.colorbar(im, ax=ax3, shrink=0.8, pad=0.02)
    cbar.ax.tick_params(colors='#A2A2A2', labelsize=8)
    cbar.set_label('Intensity difference', color='#A2A2A2', fontsize=9)

    plt.tight_layout()
    plt.savefig(out_dir + 'ppi_differential.png', dpi=150,
                facecolor=BLACK, bbox_inches='tight', pad_inches=0.3)
    plt.close()
    print(f"Saved: ppi_differential.png")

if __name__ == '__main__':
    main()
