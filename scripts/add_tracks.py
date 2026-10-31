#!/usr/bin/env python3
"""
Adds MP3 tracks to Jukebox SD card following the exact sd-manager.html specification:
- Computes safeNextNum (max(card files, tracklist entries) + 1).
- 5-digit zero-padded names (e.g. 00015.mp3).
- Maintains and sorts tracklist.txt alphabetically by key.
- Automatically cleans macOS AppleDouble (._*) resource fork junk.
Usage:
  scripts/add_tracks.py [--folder MUSIC|OTHER] [--sd /Volumes/SDCARD] file1.mp3 [file2.mp3 ...]
"""
import argparse, glob, os, shutil, sys

def main():
    parser = argparse.ArgumentParser(description="Add MP3 tracks to Jukebox SD card")
    parser.add_argument("files", nargs="+", help="MP3 files or globs to add")
    parser.add_argument("--folder", choices=["MUSIC", "OTHER"], default="MUSIC", help="Target folder (default: MUSIC)")
    parser.add_argument("--sd", default="/Volumes/SDCARD", help="SD card mount path (default: /Volumes/SDCARD)")
    args = parser.parse_args()

    sd_root = os.path.abspath(args.sd)
    target_dir = os.path.join(sd_root, args.folder)
    tracklist_file = os.path.join(sd_root, "tracklist.txt")

    if not os.path.isdir(target_dir):
        sys.exit(f"Error: Target directory does not exist: {target_dir}. Is the SD card mounted?")

    # 1. Read existing tracklist
    tracklist = {}
    if os.path.exists(tracklist_file):
        with open(tracklist_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    tracklist[k.strip()] = v.strip()

    # 2. Compute safeNextNum(folder) exactly as in sd-manager.html
    files = [f for f in os.listdir(target_dir) if f.lower().endswith(".mp3") and not f.startswith("._")]
    max_f = 0
    for f in files:
        try:
            n = int(os.path.splitext(f)[0])
            if n > max_f: max_f = n
        except ValueError:
            pass

    prefix = f"{args.folder}/"
    max_tl = 0
    for k in tracklist:
        if k.startswith(prefix):
            fname = k[len(prefix):]
            try:
                n = int(os.path.splitext(fname)[0])
                if n > max_tl: max_tl = n
            except ValueError:
                pass

    next_num = max(max_f, max_tl) + 1
    print(f"[{args.folder}] safeNextNum: current max file={max_f}, tracklist={max_tl} -> next start: {next_num:05d}")

    # 3. Process input files
    resolved_files = []
    for pattern in args.files:
        matches = glob.glob(pattern)
        if matches:
            resolved_files.extend(matches)
        elif os.path.exists(pattern):
            resolved_files.append(pattern)
        else:
            print(f"Warning: File not found: {pattern}", file=sys.stderr)

    if not resolved_files:
        sys.exit("No valid files to add.")

    for src_path in resolved_files:
        base_name = os.path.basename(src_path)
        dest_filename = f"{next_num:05d}.mp3"
        dest_path = os.path.join(target_dir, dest_filename)
        tl_key = f"{args.folder}/{dest_filename}"

        print(f"Adding: {base_name} -> {tl_key} ({os.path.getsize(src_path)} bytes)...")
        with open(src_path, "rb") as f_src, open(dest_path, "wb") as f_dst:
            shutil.copyfileobj(f_src, f_dst)

        tracklist[tl_key] = base_name
        next_num += 1

    # 4. Save sorted tracklist
    sorted_entries = sorted(tracklist.items(), key=lambda kv: kv[0])
    with open(tracklist_file, "w", encoding="utf-8") as f:
        for k, v in sorted_entries:
            f.write(f"{k} = {v}\n")

    # 5. Clean macOS junk
    junk_count = 0
    for root, dirs, fnames in os.walk(sd_root):
        for fn in fnames:
            if fn.startswith("._"):
                try:
                    os.remove(os.path.join(root, fn))
                    junk_count += 1
                except Exception:
                    pass

    print(f"\nDone! Added {len(resolved_files)} track(s). Cleaned {junk_count} AppleDouble (._*) files.")

if __name__ == "__main__":
    main()
