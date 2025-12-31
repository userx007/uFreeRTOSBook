#!/usr/bin/env bash

shopt -s nullglob

for f in *; do
    new=$(printf '%s' "$f" | sed -E 's/[ -]/_/g; s/_+/_/g')
    if [[ "$f" != "$new" ]]; then
        mv -- "$f" "$new"
    fi
done
