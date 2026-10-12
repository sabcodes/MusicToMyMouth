#!/bin/zsh
set -eu
project_dir="${0:A:h:h}"
component="${project_dir}/build/MusicToMyMouth_artefacts/Release/AU/MusicToMyMouth.component"
if [[ ! -d "$component" ]]; then
    print -u2 'Build the Release AU first. See README.md.'
    exit 1
fi
installation_dir="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$installation_dir"
/usr/bin/ditto "$component" "$installation_dir/MusicToMyMouth.component"
/usr/bin/codesign --force --sign - --timestamp=none "$installation_dir/MusicToMyMouth.component"
print "Installed: $installation_dir/MusicToMyMouth.component"
print 'Reopen Logic, then select Audio FX > Audio Units > Sabarno Islam > MusicToMyMouth.'
