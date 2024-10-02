# Convert various image formats to 16-bit 565 format and save as C header file.
# Run "pip install pillow" to get the PIL library.
from PIL import Image
import os

def convert_to_565(image):
    """Convert an image to 16-bit 565 format."""
    width, height = image.size
    pixels = list(image.getdata())
    pixel_data = []

    for pixel in pixels:
        r, g, b = pixel[:3]
        r = (r >> 3) & 0x1F
        g = (g >> 2) & 0x3F
        b = (b >> 3) & 0x1F
        pixel_data.append((r << 11) | (g << 5) | b)

    return pixel_data

def save_as_c_header(image_name, pixel_data, width, height):
    """Save the pixel data as a C header file."""
    header_name = os.path.splitext(image_name)[0] + ".h"
    with open(header_name, 'w') as header_file:
        header_file.write(f"#ifndef {os.path.splitext(image_name)[0].upper()}_H\n")
        header_file.write(f"#define {os.path.splitext(image_name)[0].upper()}_H\n\n")
        header_file.write(f"const unsigned char {os.path.splitext(image_name)[0]}_data[{width} * {height} * 2] = {{\n")

        for i, pixel in enumerate(pixel_data):
            if i % width == 0:
                header_file.write("\n    ")
            header_file.write(f"0x{(pixel >> 8) & 0xFF:02X}, 0x{pixel & 0xFF:02X}, ")

        header_file.write("\n};\n\n")
        header_file.write("#endif\n")

def process_bitmap(image_path):
    """Process a bitmap image and convert it to 565 format."""
    image = Image.open(image_path)
    width, height = image.size

    if width != 240 or height != 320:
        print(f"Image {image_path} is not 240x320, resizing.")
        image = image.resize((240,320), Image.ANTIALIAS)
        width, height = image.size

    image = image.convert("RGB")

    # Flip the image 90 degrees (270 degrees counterclockwise) if width is less than height
    if width < height:
        image = image.transpose(Image.ROTATE_270)

    image = image.transpose(Image.FLIP_TOP_BOTTOM)  # Mirror the image horizontally
    #flip the image 90 degrees
    #image = image.transpose(Image.ROTATE_270)
    pixel_data = convert_to_565(image)
    save_as_c_header(os.path.basename(image_path), pixel_data, width, height)
    print(f"Processed {image_path} and saved as C header.")

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python image.py <image1> <image2> ...")
        sys.exit(1)

    for image_path in sys.argv[1:]:
        process_bitmap(image_path)