"""Make a scientific motion preview from the saved C++ trajectory.

Requires NumPy, Matplotlib and Pillow. No generated or prescribed stellar paths.
"""
import argparse
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
from matplotlib.animation import FuncAnimation, PillowWriter
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', type=Path, default=ROOT/'doc/experiments/dance/preview.csv')
    parser.add_argument('--output', type=Path, default=ROOT/'doc/experiments/dance')
    args = parser.parse_args()
    data = np.loadtxt(args.data, delimiter=',', skiprows=1)
    time = data[:, 0]
    positions = data[:, 1:13].reshape(-1, 4, 3)
    star, planet = positions[:, :3], positions[:, 3]
    forward = -planet/np.linalg.norm(planet, axis=1)[:, None]
    axis = np.array([0., -np.cos(np.deg2rad(20)), np.sin(np.deg2rad(20))])
    right = np.cross(forward, axis)
    right /= np.linalg.norm(right, axis=1)[:, None]
    up = np.cross(right, forward)
    direction = star-planet[:, None, :]
    x = np.einsum('nsi,ni->ns', direction, right)
    y = np.einsum('nsi,ni->ns', direction, up)
    z = np.einsum('nsi,ni->ns', direction, forward)
    longitude = np.rad2deg(np.arctan2(x, z))
    latitude = np.rad2deg(np.arctan2(y, np.hypot(x, z)))
    plt.rcParams.update({'font.size': 9, 'text.color': '#dce1e7', 'axes.labelcolor': '#c6cbd3',
                         'xtick.color': '#9aa4b0', 'ytick.color': '#9aa4b0', 'axes.edgecolor': '#647080',
                         'savefig.facecolor': '#0c1119', 'svg.fonttype': 'none'})
    fig = plt.figure(figsize=(10, 5), dpi=96, facecolor='#0c1119')
    left = fig.add_subplot(1, 2, 1, projection='3d', facecolor='#0c1119')
    right_ax = fig.add_subplot(1, 2, 2, facecolor='#0c1119')
    fig.subplots_adjust(left=.04, right=.98, bottom=.17, top=.75, wspace=.24)
    fig.suptitle('A THREE-SUN DANCE  /  research preview', fontsize=15, y=.96)
    fig.text(.5, .875, 'Three equal stellar masses • published spatial orbit • positions integrated by the C++ solver', ha='center', fontsize=9)
    fig.text(.5, .035, 'Sky view tracks the triple’s centre; planetary daily rotation and terrain are omitted. Disks enlarged.', ha='center', fontsize=8)
    left.set(title='Stellar motion in space', xlim=(-5, 5), ylim=(-5, 5), zlim=(-5, 5),
             xlabel='X (AU)', ylabel='Z (AU)', zlabel='Y (AU)')
    left.set_box_aspect((1, 1, 1))
    left.view_init(elev=25, azim=-55)
    for a in (left.xaxis, left.yaxis, left.zaxis):
        a.pane.fill = False
        a._axinfo['grid']['color'] = '#27313d'
    limit = max(np.max(np.abs(longitude)), np.max(np.abs(latitude)))+2
    right_ax.set(title='Apparent motion from the planet', xlim=(-limit, limit), ylim=(-limit, limit),
                 xlabel='Horizontal offset from triple centre (degrees)', ylabel='Vertical offset (degrees)')
    right_ax.set_aspect('equal')
    right_ax.grid(alpha=.12)
    right_ax.axhline(0, color='#526070', alpha=.4, lw=.6)
    right_ax.axvline(0, color='#526070', alpha=.4, lw=.6)
    colors = ('#ffe2a1', '#ff995c', '#c9e4ff')
    trails, points, sky_trails, sky_points = [], [], [], []
    for i, color in enumerate(colors):
        first_period = time <= 8.698291331281512
        left.plot(star[first_period, i, 0], star[first_period, i, 2], star[first_period, i, 1], color=color, alpha=.12, lw=.7)
        trails.append(left.plot([], [], [], color=color, lw=1.5)[0])
        points.append(left.plot([], [], [], 'o', color=color, ms=6)[0])
        sky_trails.append(right_ax.plot([], [], color=color, lw=1.6)[0])
        sky_points.append(right_ax.plot([], [], 'o', color=color, ms=8, label='Sun '+chr(65+i))[0])
    right_ax.legend(loc='upper right', facecolor='#0c1119', edgecolor='#27313d', fontsize=8)
    stamp = fig.text(.5, .825, '', ha='center', color='#ffe2a1')

    def draw(frame):
        j = int(frame)
        start = max(0, j-64)  # Four years of actual motion.
        for i in range(3):
            trail = star[start:j+1, i]
            trails[i].set_data_3d(trail[:, 0], trail[:, 2], trail[:, 1])
            point = star[j:j+1, i]
            points[i].set_data_3d(point[:, 0], point[:, 2], point[:, 1])
            sky_trails[i].set_data(longitude[start:j+1, i], latitude[start:j+1, i])
            sky_points[i].set_data(longitude[j:j+1, i], latitude[j:j+1, i])
        stamp.set_text(f'YEAR {time[j]:05.2f}  /  planet orbits the entire triple, initially at 20 AU')
        return [*trails, *points, *sky_trails, *sky_points, stamp]

    args.output.mkdir(parents=True, exist_ok=True)
    draw(np.searchsorted(time, 12))
    fig.savefig(args.output/'sky-preview.png')
    frames = np.linspace(0, np.searchsorted(time, 32), 192).astype(int)
    animation = FuncAnimation(fig, draw, frames=frames, interval=1000/12, blit=False)
    animation.save(args.output/'sky-preview.gif', writer=PillowWriter(fps=12))
    plt.close(fig)


if __name__ == '__main__':
    main()
