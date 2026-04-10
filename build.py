#!/usr/bin/env python3
"""
InksPet Firmware Builder
Builds firmware, filesystem, copies to firmware/ dir, and updates manifest.json
for ESP Web Tools browser-based flashing.

Usage:
    python3 build.py          # Full build
    python3 build.py --skip-build  # Only copy + update manifest (skip pio compile)
"""

import os
import shutil
import subprocess
import sys
import json
from datetime import datetime

class FirmwareBuilder:
    # Partition offsets for huge_app.csv layout
    PARTITIONS = {
        'bootloader': '0x1000',
        'partitions': '0x8000',
        'firmware':   '0x10000',   # app0 in huge_app.csv
        'littlefs':   '0x310000',  # spiffs in huge_app.csv
    }

    def __init__(self):
        self.workspace_dir = os.path.dirname(os.path.abspath(__file__))
        self.build_dir = os.path.join(self.workspace_dir, '.pio', 'build', 'esp32dev')
        self.firmware_dir = os.path.join(self.workspace_dir, 'firmware')

    def run_command(self, cmd):
        """Run shell command and return success status"""
        try:
            result = subprocess.run(cmd, shell=True, check=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    encoding='utf-8')
            print(f"  $ {cmd}")
            if result.stdout.strip():
                # Print last few lines (build summary)
                lines = result.stdout.strip().split('\n')
                for line in lines[-5:]:
                    print(f"    {line}")
            return True
        except subprocess.CalledProcessError as e:
            print(f"  FAILED: {cmd}")
            print(f"  {e.stderr[-500:]}" if e.stderr else "")
            return False

    def get_firmware_version(self):
        """Read firmware version from version.h"""
        version_file = os.path.join(self.workspace_dir, 'src', 'version.h')
        try:
            with open(version_file, 'r') as f:
                for line in f:
                    if 'FIRMWARE_VERSION' in line and '"' in line:
                        return line.split('"')[1]
        except Exception as e:
            print(f"  Error reading version: {e}")
        return None

    def build_firmware(self):
        """Compile firmware with PlatformIO"""
        print("[1/4] Building firmware...")
        if not self.run_command("pio run -e esp32dev"):
            return False

        # Verify output files exist
        for f in ['firmware.bin', 'bootloader.bin', 'partitions.bin']:
            path = os.path.join(self.build_dir, f)
            if not os.path.exists(path):
                print(f"  Error: {f} not found at {path}")
                return False

        print("  Firmware build OK")
        return True

    def build_filesystem(self):
        """Build LittleFS filesystem image"""
        print("[2/4] Building filesystem (LittleFS)...")
        if not self.run_command("pio run -t buildfs -e esp32dev"):
            return False

        littlefs_path = os.path.join(self.build_dir, 'littlefs.bin')
        if not os.path.exists(littlefs_path):
            print(f"  Error: littlefs.bin not found")
            return False

        print("  Filesystem build OK")
        return True

    def copy_firmware_files(self):
        """Copy build outputs to firmware/ dir with versioned names"""
        print("[3/4] Copying firmware files...")

        os.makedirs(self.firmware_dir, exist_ok=True)

        version = self.get_firmware_version() or "unknown"
        timestamp = datetime.now().strftime("%y%m%d%H%M")

        # Source -> destination mapping
        file_map = {
            'bootloader.bin': f'bootloader-v{version}-{timestamp}.bin',
            'partitions.bin': f'partitions-v{version}-{timestamp}.bin',
            'firmware.bin':   f'firmware-v{version}-{timestamp}.bin',
            'littlefs.bin':   f'spiffs-v{version}-{timestamp}.bin',
        }

        self._file_map = {}  # Store for manifest update

        for src_name, dst_name in file_map.items():
            src_path = os.path.join(self.build_dir, src_name)
            dst_path = os.path.join(self.firmware_dir, dst_name)

            if not os.path.exists(src_path):
                print(f"  Error: source file missing: {src_path}")
                return False

            # Remove old file if exists
            if os.path.exists(dst_path):
                os.remove(dst_path)

            shutil.copy2(src_path, dst_path)
            size = os.path.getsize(dst_path)
            print(f"  {src_name} -> {dst_name} ({size:,} bytes)")

            # Map original name key to new filename
            key = src_name.replace('.bin', '').replace('littlefs', 'littlefs')
            self._file_map[src_name] = {
                'path': dst_name,
                'size': size,
            }

        print("  Files copied OK")
        return True

    def update_manifest(self):
        """Generate manifest.json and manifest-v{version}.json for ESP Web Tools"""
        print("[4/4] Updating manifest...")

        version = self.get_firmware_version()
        if not version:
            print("  Error: cannot read firmware version")
            return False

        # Build manifest structure
        manifest = {
            "name": "InksPet",
            "version": version,
            "home_assistant_domain": "",
            "funding_url": "",
            "new_install_prompt_erase": True,
            "builds": [
                {
                    "chipFamily": "ESP32",
                    "improv": False,
                    "parts": []
                }
            ]
        }

        # Part order: bootloader, partitions, firmware, littlefs
        part_keys = [
            ('bootloader.bin', 'bootloader'),
            ('partitions.bin', 'partitions'),
            ('firmware.bin',   'firmware'),
            ('littlefs.bin',   'littlefs'),
        ]

        for src_name, key in part_keys:
            if src_name not in self._file_map:
                print(f"  Error: {src_name} not in file map")
                return False

            info = self._file_map[src_name]
            manifest['builds'][0]['parts'].append({
                'path': info['path'],
                'offset': self.PARTITIONS[key],
                'size': info['size'],
            })

        # Write versioned manifest
        versioned_path = os.path.join(self.firmware_dir, f'manifest-v{version}.json')
        with open(versioned_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        print(f"  manifest-v{version}.json created")

        # Write default manifest.json (for backward compat)
        default_path = os.path.join(self.firmware_dir, 'manifest.json')
        with open(default_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        print(f"  manifest.json updated (v{version})")

        return True

    def build(self, skip_build=False):
        """Run the full build pipeline"""
        print("=" * 50)
        print(f"  InksPet Firmware Builder")
        print(f"  {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 50)

        version = self.get_firmware_version()
        print(f"  Version: {version}")
        print(f"  Partition layout: huge_app.csv")
        print(f"  App offset: {self.PARTITIONS['firmware']}")
        print(f"  LittleFS offset: {self.PARTITIONS['littlefs']}")
        print()

        if skip_build:
            steps = [
                (self.copy_firmware_files, "Copy files"),
                (self.update_manifest, "Update manifest"),
            ]
        else:
            steps = [
                (self.build_firmware, "Build firmware"),
                (self.build_filesystem, "Build filesystem"),
                (self.copy_firmware_files, "Copy files"),
                (self.update_manifest, "Update manifest"),
            ]

        for step_func, step_name in steps:
            print()
            if not step_func():
                print(f"\n  BUILD FAILED at: {step_name}")
                return False

        print()
        print("=" * 50)
        print(f"  BUILD SUCCESS")
        print(f"  Output: {self.firmware_dir}/")

        # List output files
        for f in sorted(os.listdir(self.firmware_dir)):
            if f.endswith('.bin') or f.endswith('.json'):
                size = os.path.getsize(os.path.join(self.firmware_dir, f))
                print(f"    {f:45s} {size:>10,} bytes")

        print("=" * 50)
        return True


def main():
    skip_build = '--skip-build' in sys.argv
    builder = FirmwareBuilder()
    success = builder.build(skip_build=skip_build)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
