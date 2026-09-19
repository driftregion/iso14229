#!/usr/bin/env sh
set -eu

make_version() {
    major="$(awk -F ':=' '/^MAJOR_VER[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }' vars.mk)"
    minor="$(awk -F ':=' '/^MINOR_VER[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }' vars.mk)"
    revision="$(awk -F ':=' '/^REVISION[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }' vars.mk)"

    if [ -z "$major" ] || [ -z "$minor" ] || [ -z "$revision" ]; then
        echo "Unable to read MAJOR_VER, MINOR_VER, and REVISION from vars.mk" >&2
        exit 1
    fi

    printf '%s.%s.%s\n' "$major" "$minor" "$revision"
}

cmake_version() {
    version="$(sed -n 's/^project(.* VERSION \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' CMakeLists.txt)"

    if [ -z "$version" ]; then
        echo "Unable to read project VERSION from CMakeLists.txt" >&2
        exit 1
    fi

    printf '%s\n' "$version"
}

library_version() {
    version="$(sed -n 's/^[[:space:]]*"version"[[:space:]]*:[[:space:]]*"v\{0,1\}\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)"[[:space:]]*,\{0,1\}[[:space:]]*$/\1/p' library.json)"

    if [ -z "$version" ]; then
        echo "Unable to read version from library.json" >&2
        exit 1
    fi

    printf '%s\n' "$version"
}

version_at_ref() {
    ref="$1"
    file="$2"

    if ! git cat-file -e "$ref:$file" 2>/dev/null; then
        printf '\n'
        return
    fi

    case "$file" in
        vars.mk)
            content="$(git show "$ref:$file")"
            major="$(printf '%s\n' "$content" | awk -F ':=' '/^MAJOR_VER[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }')"
            minor="$(printf '%s\n' "$content" | awk -F ':=' '/^MINOR_VER[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }')"
            revision="$(printf '%s\n' "$content" | awk -F ':=' '/^REVISION[[:space:]]*:=/ { gsub(/[[:space:]\"]/, "", $2); print $2 }')"
            printf '%s.%s.%s\n' "$major" "$minor" "$revision"
            ;;
        CMakeLists.txt)
            git show "$ref:$file" | sed -n 's/^project(.* VERSION \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p'
            ;;
        library.json)
            git show "$ref:$file" | sed -n 's/^[[:space:]]*"version"[[:space:]]*:[[:space:]]*"v\{0,1\}\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)"[[:space:]]*,\{0,1\}[[:space:]]*$/\1/p'
            ;;
    esac
}

current_make_version="$(make_version)"
current_cmake_version="$(cmake_version)"
current_library_version="$(library_version)"

if [ "$current_make_version" != "$current_cmake_version" ] ||
    [ "$current_make_version" != "$current_library_version" ]; then
    echo "Version mismatch: vars.mk has $current_make_version, CMakeLists.txt has $current_cmake_version, and library.json has $current_library_version" >&2
    exit 1
fi

base_ref="${VERSION_GATE_BASE_REF:-}"
if [ -z "$base_ref" ] && [ -n "${GITHUB_BASE_REF:-}" ]; then
    base_ref="origin/${GITHUB_BASE_REF}"
fi

if [ -z "$base_ref" ]; then
    base_ref="origin/master"
fi

if ! git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
    echo "No comparable base ref found for version bump check; validated version consistency only."
    exit 0
fi

changed_files="$(
    {
        git diff --name-only "$base_ref"...HEAD
        git diff --name-only
    } | sort -u
)"
requires_version_bump="$(
    printf '%s\n' "$changed_files" |
        grep -E '^(isotp[^/]*\.(c|h)|CMakeLists\.txt|Makefile|vars\.mk|library\.json)$' |
        grep -Ev '^(CMakeLists\.txt|vars\.mk)$' || true
)"

if [ -z "$requires_version_bump" ]; then
    echo "No versioned source/build files changed; validated version consistency only."
    exit 0
fi

base_make_version="$(version_at_ref "$base_ref" vars.mk)"
base_cmake_version="$(version_at_ref "$base_ref" CMakeLists.txt)"
base_library_version="$(version_at_ref "$base_ref" library.json)"

if [ "$base_make_version" = "$current_make_version" ] ||
    [ "$base_cmake_version" = "$current_cmake_version" ] ||
    [ "$base_library_version" = "$current_library_version" ]; then
    echo "Version bump required when versioned source/build files change." >&2
    echo "Update vars.mk, CMakeLists.txt, and library.json from $base_make_version/$base_cmake_version/$base_library_version to the same new version." >&2
    echo "Changed files requiring a bump:" >&2
    printf '%s\n' "$requires_version_bump" >&2
    exit 1
fi

echo "Version gate passed: $current_make_version"
