#!/usr/bin/env python3

"""

Download a GGUF model from Hugging Face or a direct URL.
Place in scripts/download_model.py, run from project root.

Examples:

# Default: download recommended model
python scripts/download_model.py

# Download a specific Hugging Face file by URL
python scripts/download_model.py https://huggingface.co/TheBloke/MythoMist-7B-GGUF/resolve/main/mythomist-7b.Q4_K_M.gguf

# Use a model ID (requires --filename)
python scripts/download_model.py TheBloke/MythoMist-7B-GGUF --filename mythomist-7b.Q4_K_M.gguf

# Custom output directory and filename
python scripts/download_model.py --output-dir my_models --filename custom_name.gguf https://...

# Skip the "overwrite?" prompt
python scripts/download_model.py --force

"""

import os
import sys
import argparse
import urllib.request
import urllib.parse
import shutil

# Default settings
DEFAULT_MODEL_URL = "https://huggingface.co/TheBloke/MythoMist-7B-GGUF/resolve/main/mythomist-7b.Q4_K_M.gguf"
DEFAULT_FILENAME = "mythomist-7b.Q4_K_M.gguf"
DEFAULT_OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "res", "models")

def parse_args():
    parser = argparse.ArgumentParser(description="Download a GGUF model file.")
    parser.add_argument("model", nargs="?", help="Model URL or Hugging Face repo ID (e.g., TheBloke/MythoMist-7B-GGUF)")
    parser.add_argument("--filename", help="Output filename (required if using repo ID, optional otherwise)")
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR, help="Directory to save the file (default: res/models)")
    parser.add_argument("--force", action="store_true", help="Overwrite existing file without asking")
    return parser.parse_args()

def get_url_and_filename(args):
    """Determine download URL and output filename from arguments."""
    if not args.model:
        # Use default model
        url = DEFAULT_MODEL_URL
        filename = DEFAULT_FILENAME
    elif args.model.startswith(("http://", "https://")):
        url = args.model
        # Extract filename from URL if not given
        if args.filename:
            filename = args.filename
        else:
            filename = os.path.basename(urllib.parse.urlparse(url).path)
            if not filename:
                filename = DEFAULT_FILENAME
    else:
        # Assume it's a Hugging Face repo ID
        repo = args.model.rstrip("/")
        if not args.filename:
            print(f"Error: When using a repo ID, you must specify --filename (e.g., --filename model.Q4_K_M.gguf)")
            sys.exit(1)
        filename = args.filename
        url = f"https://huggingface.co/{repo}/resolve/main/{filename}"
    return url, filename

def download_file(url, dest_path, force=False):
    """Download file with progress indicator, skip if exists and not forced."""
    if os.path.exists(dest_path) and not force:
        print(f"File already exists: {dest_path}")
        response = input("Overwrite? [y/N]: ").strip().lower()
        if response != 'y':
            print("Skipping download.")
            return False
    print(f"Downloading {url} ...")
    try:
        # Stream download with progress
        with urllib.request.urlopen(url) as response, open(dest_path, 'wb') as out_file:
            total = int(response.headers.get('Content-Length', 0))
            downloaded = 0
            chunk_size = 8192
            while True:
                chunk = response.read(chunk_size)
                if not chunk:
                    break
                out_file.write(chunk)
                downloaded += len(chunk)
                if total > 0:
                    percent = downloaded / total * 100
                    print(f"\rProgress: {percent:.1f}%", end='', flush=True)
            print()  # new line after progress
        return True
    except Exception as e:
        print(f"Download failed: {e}")
        return False

def main():
    args = parse_args()
    url, filename = get_url_and_filename(args)
    # Ensure output directory exists
    os.makedirs(args.output_dir, exist_ok=True)
    dest_path = os.path.join(args.output_dir, filename)
    success = download_file(url, dest_path, force=args.force)
    if success:
        print(f"Model saved to: {dest_path}")

if __name__ == "__main__":
    main()