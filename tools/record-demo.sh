#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="$project_dir/doc/assets"
version="${1:-01}"
if [[ "$version" != 01 && "$version" != 02 && "$version" != 02-01 && "$version" != 02-02 && "$version" != 02-03 ]]; then
    printf '%s\n' 'Usage: record-demo.sh [01|02|02-01|02-02|02-03]' >&2
    exit 1
fi
[[ "$version" == 02 ]] && version=02-01
playback=0.08
[[ "$version" == 02-02 ]] && playback=2
[[ "$version" == 02-03 ]] && playback=0.8
# Both versions use the same output dimensions. Keep the more detailed terrain
# preview at 10 fps to limit GIF size; intermediate captures use 20 fps.
gif_fps=15
gif_colors=128
gif_delay=7
if [[ "$version" == 02-* ]]; then
    gif_fps=10
    gif_colors=96
    gif_delay=10
fi
for dependency in ffmpeg xwininfo; do
    command -v "$dependency" >/dev/null || { printf 'Missing recording dependency: %s\n' "$dependency" >&2; exit 1; }
done
mkdir -p "$output_dir"
record_tmp="$(mktemp -d)"
app_pid=''
stop_app() {
    if [[ -n "$app_pid" ]]; then
        kill "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
        app_pid=''
    fi
}
cleanup() {
    stop_app
    rm -rf -- "$record_tmp"
}
trap cleanup EXIT

record_segment() {
    local view="$1" start_time="$2" duration="$3" destination="$4"
    local title="3-body-problem recording $version $view $$"
    local log="$record_tmp/$view.log"
    "$project_dir/run.sh" --orbit "$version" --view "$view" --title "$title" \
        --time "$start_time" --speed 1 >"$log" 2>&1 &
    app_pid=$!
    local window_id=''
    local ready=false
    for ((attempt=0; attempt<100; ++attempt)); do
        window_id="$(xwininfo -name "$title" 2>/dev/null | awk '/Window id:/ {print $4; exit}' || true)"
        if [[ "$window_id" =~ ^0x[0-9a-fA-F]+$ ]] && grep -q '^First frame ready[.]$' "$log"; then
            ready=true
            break
        fi
        if ! kill -0 "$app_pid" 2>/dev/null; then cat "$log" >&2; exit 1; fi
        sleep 0.2
    done
    if [[ "$ready" != true ]]; then
        cat "$log" >&2
        printf '%s\n' 'Could not find a rendered recording window. Run this from an X11/WSLg desktop session.' >&2
        exit 1
    fi
    local video_size
    video_size="$(xwininfo -id "$window_id" | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')"
    printf 'Recording Orbit / %s %s view (%s), for %s seconds.\n' "$version" "$view" "$video_size" "$duration"
    ffmpeg -hide_banner -loglevel warning -y -f x11grab -framerate 20 -draw_mouse 0 \
        -window_id "$window_id" -video_size "$video_size" -i "${DISPLAY:-:0}" -t "$duration" -an \
        -vf 'scale=1280:800:flags=lanczos' -c:v libx264 -preset fast -crf 22 -pix_fmt yuv420p \
        "$destination"
    stop_app
}

if [[ "$version" == 02-* ]]; then
    record_segment surface 0 8 "$record_tmp/surface.mkv"
    # Continue at eight real seconds of this preset’s documented playback rate.
    record_segment orbit "$(awk -v rate="$playback" 'BEGIN {print rate*8}')" 7 "$record_tmp/orbit.mkv"
    printf '%s\n' "file 'surface.mkv'" "file 'orbit.mkv'" >"$record_tmp/segments.txt"
    ffmpeg -hide_banner -loglevel warning -y -f concat -safe 0 -i "$record_tmp/segments.txt" \
        -c copy "$record_tmp/demo.mkv"
else
    record_segment orbit 1000 15 "$record_tmp/demo.mkv"
fi
ffmpeg -hide_banner -loglevel warning -y -i "$record_tmp/demo.mkv" \
    -filter_complex "fps=$gif_fps,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=$gif_colors:stats_mode=diff[p];[b][p]paletteuse=dither=sierra2_4a:diff_mode=rectangle" \
    -loop 0 -final_delay "$gif_delay" "$output_dir/orbit-$version.gif"
printf 'Saved %s\n' "$output_dir/orbit-$version.gif"
