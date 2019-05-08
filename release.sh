#!/bin/bash

RELEASE=release/VRC-Knuckles-Emulator-1
mkdir -p $RELEASE
cp VRC-Knuckles-Emulator/{action_manifest.json,knuckles_bindings.json} $RELEASE
cp x64/Release/{VRC-Knuckles-Emulator.exe,vrc_knuckles_emulator.dll,openvr_api.dll} $RELEASE

