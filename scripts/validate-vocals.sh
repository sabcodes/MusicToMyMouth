#!/bin/zsh
set -eu
if (( $# < 1 || $# > 2 )); then
    print -u2 'Usage: ./scripts/validate-vocals.sh /absolute/path/to/vocals.mp3 [--full]'
    exit 2
fi
project_dir="${0:A:h:h}"
input_path="${1:A}"
ffmpeg_path="$(command -v ffmpeg || true)"
if [[ -z "$ffmpeg_path" && -x /opt/homebrew/bin/ffmpeg ]]; then
    ffmpeg_path=/opt/homebrew/bin/ffmpeg
fi
if [[ -z "$ffmpeg_path" || ! -f "$input_path" ]]; then
    print -u2 'A readable input vocal file and ffmpeg are required.'
    exit 2
fi
runner="$project_dir/build/MusicToMyMouthValidation"
if [[ ! -x "$runner" ]]; then
    print -u2 'Configure with -DMOUTH_BUILD_VALIDATION=ON and build MusicToMyMouthValidation first.'
    exit 2
fi
run_dir="$(mktemp -d "$project_dir/build/vocal-validation-XXXXXX")"
"$ffmpeg_path" -hide_banner -loglevel error -i "$input_path" -c:a pcm_f32le "$run_dir/input.wav"
/usr/bin/auval -v aufx Mtmm Sbcd > "$run_dir/auval.log" 2>&1
"$runner" "$run_dir/input.wav" "$run_dir/results" ${2:+"$2"}
print "Validation results and listening clips: $run_dir/results"
