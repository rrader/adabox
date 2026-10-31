#!/usr/bin/env python3
"""
Jukebox MP3 Downloader & Audio Processor

Downloads tracks from YouTube (or direct URLs), converts to high quality MP3,
and places them into projects/jukebox/mp3 ready for Jukebox SD card preparation.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

DEFAULT_OUT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "mp3"))

def find_node_path():
    for p in ["/opt/homebrew/bin/node", "/usr/local/bin/node", shutil.which("node")]:
        if p and os.path.exists(p) and os.access(p, os.X_OK):
            return p
    return None

def clean_title(title: str) -> str:
    # Remove emojis
    emoji_pattern = re.compile(
        "["
        "\U0001F1E0-\U0001F1FF"  # flags
        "\U0001F300-\U0001F5FF"  # symbols & pictographs
        "\U0001F600-\U0001F64F"  # emoticons
        "\U0001F680-\U0001F6FF"  # transport & map
        "\U0001F700-\U0001F77F"  # alchemical symbols
        "\U0001F780-\U0001F7FF"  # Geometric Shapes Extended
        "\U0001F800-\U0001F8FF"  # Supplemental Arrows-C
        "\U0001F900-\U0001F9FF"  # Supplemental Symbols and Pictographs
        "\U0001FA00-\U0001FA6F"  # Chess Symbols
        "\U0001FA70-\U0001FAFF"  # Symbols and Pictographs Extended-A
        "\U00002702-\U000027B0"  # Dingbats
        "\U000024C2-\U0001F251"
        "]+",
        flags=re.UNICODE,
    )
    title = emoji_pattern.sub("", title)
    
    # Remove common video suffixes / noise
    patterns = [
        r"(?i)\s*[\(\[](?:official\s*(?:music\s*)?video|video|audio|lyrics|official|hd|4k|hq|clip|кліп|прем'єра)[\)\]]",
        r"(?i)\s*[-|–—]\s*(?:дитячі\s*пісні|пісенька\s*для\s*малюків|з\s*любов['ʼ]ю\s*до\s*дітей|мультики\s*українською).*",
        r"\s*\|\s*.*$",
    ]
    for p in patterns:
        title = re.sub(p, "", title)
        
    title = re.sub(r"\s+", " ", title).strip()
    return title

def download_track(query_or_url: str, output_name: str = None, out_dir: str = DEFAULT_OUT_DIR):
    os.makedirs(out_dir, exist_ok=True)
    node_path = find_node_path()

    base_cmd = ["uvx", "yt-dlp", "--remote-components", "ejs:github"]
    if node_path:
        base_cmd.extend(["--js-runtimes", f"node:{node_path}"])

    is_url = query_or_url.startswith("http://") or query_or_url.startswith("https://")
    target = query_or_url if is_url else f"ytsearch1:{query_or_url}"

    # Get title first if output_name not specified
    if not output_name:
        info_cmd = base_cmd + [target, "--print", "%(title)s"]
        res = subprocess.run(info_cmd, capture_output=True, text=True)
        raw_title = res.stdout.strip()
        if not raw_title:
            sys.stderr.write(f"Error fetching info for: {query_or_url}\n{res.stderr}\n")
            return None
        output_name = clean_title(raw_title)

    if not output_name.lower().endswith(".mp3"):
        filename = f"{output_name}.mp3"
    else:
        filename = output_name

    out_template = os.path.join(out_dir, f"{os.path.splitext(filename)[0]}.%(ext)s")
    target_mp3 = os.path.join(out_dir, filename)

    print(f"-> Downloading: {query_or_url}")
    print(f"   Saving as: {filename}")

    dl_cmd = base_cmd + [
        target,
        "-x",
        "--audio-format", "mp3",
        "--audio-quality", "0",
        "-o", out_template,
        "--no-playlist",
    ]

    res = subprocess.run(dl_cmd)
    if res.returncode != 0:
        sys.stderr.write(f"Download failed with code {res.returncode}\n")
        return None

    if os.path.exists(target_mp3):
        print(f"✓ Successfully saved to: {target_mp3}\n")
        return target_mp3
    return None

def main():
    parser = argparse.ArgumentParser(description="Download and convert tracks to MP3 for Jukebox")
    parser.add_argument("tracks", nargs="+", help="Search query or YouTube URL (format: 'query' or 'name=url')")
    parser.add_argument("--out-dir", "-o", default=DEFAULT_OUT_DIR, help="Destination directory (default: projects/jukebox/mp3)")
    
    args = parser.parse_args()

    for item in args.tracks:
        if "=" in item and not item.startswith("http"):
            custom_name, target = item.split("=", 1)
            download_track(target.strip(), custom_name.strip(), args.out_dir)
        else:
            download_track(item.strip(), None, args.out_dir)

if __name__ == "__main__":
    main()
