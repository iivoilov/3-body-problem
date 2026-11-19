#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="$project_dir/doc/assets"
for dependency in ffmpeg xwininfo; do
    command -v "$dependency" >/dev/null || { printf 'Missing recording dependency: %s\n' "$dependency" >&2; exit 1; }
done
mkdir -p "$output_dir"
record_tmp="$(mktemp -d)"
app_pid=''
cleanup() {
    if [[ -n "$app_pid" ]]; then kill "$app_pid" 2>/dev/null || true; wait "$app_pid" 2>/dev/null || true; fi
    rm -rf -- "$record_tmp"
}
trap cleanup EXIT

title="Orbit recording $$"
"$project_dir/run.sh" --title "$title" --time 1000 --speed 1 >"$record_tmp/app.log" 2>&1 &
app_pid=$!
window_id=''
for ((attempt=0; attempt<100; ++attempt)); do
    window_id="$(xwininfo -name "$title" 2>/dev/null | awk '/Window id:/ {print $4; exit}' || true)"
    [[ "$window_id" =~ ^0x[0-9a-fA-F]+$ ]] && break
    if ! kill -0 "$app_pid" 2>/dev/null; then cat "$record_tmp/app.log" >&2; exit 1; fi
    sleep 0.2
done
if [[ ! "$window_id" =~ ^0x[0-9a-fA-F]+$ ]]; then
    cat "$record_tmp/app.log" >&2
    printf '%s\n' 'Could not find the recording window. Run this from an X11/WSLg desktop session.' >&2
    exit 1
fi
video_size="$(xwininfo -id "$window_id" | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')"
printf 'Recording only app window %s (%s), for 15 seconds.\n' "$window_id" "$video_size"
ffmpeg -hide_banner -loglevel warning -y -f x11grab -framerate 20 -draw_mouse 0 \
    -window_id "$window_id" -video_size "$video_size" -i "${DISPLAY:-:0}" -t 15 -an \
    -vf 'pad=ceil(iw/2)*2:ceil(ih/2)*2' -c:v libx264 -preset fast -crf 22 -pix_fmt yuv420p \
    -movflags +faststart "$output_dir/orbit-lab.mp4"
ffmpeg -hide_banner -loglevel warning -y -i "$output_dir/orbit-lab.mp4" \
    -filter_complex 'fps=15,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=sierra2_4a:diff_mode=rectangle' \
    -loop 0 -final_delay 7 "$output_dir/orbit-lab.gif"
printf 'Saved %s and %s\n' "$output_dir/orbit-lab.mp4" "$output_dir/orbit-lab.gif"
