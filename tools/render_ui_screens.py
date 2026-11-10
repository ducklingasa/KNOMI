"""Export embedded GIFs and LVGL RGB565 images (requires Pillow)."""

import re
import os
import glob

from PIL import Image

def render_c_array_to_gif(c_file_path, output_gif_path):
    try:
        with open(c_file_path, 'r') as f:
            content = f.read()
            
        # Extract all hex bytes (e.g., 0x47, 0x49, 0x46)
        hex_values = re.findall(r'0x([0-9a-fA-F]{2})', content)
        
        if not hex_values:
            print(f"No hex data found in {c_file_path}")
            return
            
        # Convert hex strings back to raw bytes
        byte_data = bytes(int(x, 16) for x in hex_values)
        
        with open(output_gif_path, 'wb') as f:
            f.write(byte_data)
            
        print(f"Successfully rendered: {output_gif_path}")
        
    except FileNotFoundError:
        print(f"Error: {c_file_path} not found.")

def render_c_array_to_png(c_file_path, output_png_path):
    with open(c_file_path, 'r') as source:
        content = source.read()

    width = int(re.search(r'\.header\.w\s*=\s*(\d+)', content).group(1))
    height = int(re.search(r'\.header\.h\s*=\s*(\d+)', content).group(1))
    color_format = re.search(r'\.header\.cf\s*=\s*(\w+)', content).group(1)
    if color_format not in ('LV_IMG_CF_TRUE_COLOR', 'LV_IMG_CF_TRUE_COLOR_ALPHA'):
        raise ValueError(f'Unsupported color format in {c_file_path}: {color_format}')

    array = re.search(r'uint8_t\s+\w+\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
    byte_data = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', array.group(1)))
    has_alpha = color_format == 'LV_IMG_CF_TRUE_COLOR_ALPHA'
    bytes_per_pixel = 3 if has_alpha else 2
    if len(byte_data) != width * height * bytes_per_pixel:
        raise ValueError(f'Unexpected RGB565 data size in {c_file_path}: {len(byte_data)}')

    pixels = bytearray()
    for offset in range(0, len(byte_data), bytes_per_pixel):
        color = byte_data[offset] | (byte_data[offset + 1] << 8)
        red = (color >> 11) & 0x1F
        green = (color >> 5) & 0x3F
        blue = color & 0x1F
        pixels.extend(((red << 3) | (red >> 2),
                       (green << 2) | (green >> 4),
                       (blue << 3) | (blue >> 2),
                       byte_data[offset + 2] if has_alpha else 255))

    Image.frombytes('RGBA', (width, height), bytes(pixels)).save(output_png_path)
    print(f'Successfully rendered: {output_png_path}')


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    gif_source_dir = os.path.join(repo_root, 'src', 'ui', 'assets', 'gif')
    gif_target_dir = os.path.join(repo_root, 'docs', 'screens', 'gif')
    png_source_dirs = [os.path.join(repo_root, 'src', 'ui', 'assets', name)
                       for name in ('icons', 'images', 'reference')]
    png_target_dir = os.path.join(repo_root, 'docs', 'screens', 'png')
    os.makedirs(gif_target_dir, exist_ok=True)
    os.makedirs(png_target_dir, exist_ok=True)

    for source in sorted(glob.glob(os.path.join(gif_source_dir, 'gif_*.c'))):
        gif_file = os.path.basename(source)[4:-2] + '.gif'
        target = os.path.join(gif_target_dir, gif_file)
        render_c_array_to_gif(source, target)

    for png_source_dir in png_source_dirs:
        for source in sorted(glob.glob(os.path.join(png_source_dir, 'ui_img_*_png.c'))):
            png_file = os.path.basename(source)[7:-6] + '.png'
            target = os.path.join(png_target_dir, png_file)
            render_c_array_to_png(source, target)


if __name__ == '__main__':
    main()