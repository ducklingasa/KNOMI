import re
import os
import glob

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

repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
gif_source_dir = os.path.join(repo_root, 'src', 'gif')
gif_target_dir = os.path.join(repo_root, 'docs', 'screens')

for source in sorted(glob.glob(os.path.join(gif_source_dir, 'gif_*.c'))):
    gif_file = os.path.basename(source)[4:-2] + '.gif'
    target = os.path.join(gif_target_dir, gif_file)
    render_c_array_to_gif(source, target)