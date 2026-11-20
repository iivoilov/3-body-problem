#!/usr/bin/env bash
set -euo pipefail

# Start on the GPU even if a parent shell previously forced software rendering.
unset LIBGL_ALWAYS_SOFTWARE
export MESA_D3D12_DEFAULT_ADAPTER_NAME="${MESA_D3D12_DEFAULT_ADAPTER_NAME:-NVIDIA}"

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$project_dir/build"
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" --parallel 2

# A driver can report success while producing black frames. Check actual pixels
# before opening the window, falling back to software only if the check fails.
if [[ -z "${LIBGL_ALWAYS_SOFTWARE:-}" ]]; then
    probe_log="$(mktemp)"
    trap 'rm -f -- "$probe_log"' EXIT
    if ! "$build_dir/three_body_problem" --smoke-test >"$probe_log" 2>&1; then
        cat "$probe_log" >&2
        printf '%s\n' 'Default graphics check failed; trying Mesa software rendering.' >&2
        export LIBGL_ALWAYS_SOFTWARE=1
    fi
    rm -f -- "$probe_log"
    trap - EXIT
fi
exec "$build_dir/three_body_problem" "$@"
