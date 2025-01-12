import os
import sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from secrets import token_bytes
import struct
from datetime import datetime


def to_little_endian(data: bytes) -> bytes:
    """Convert bytes to little-endian representation."""
    return data[::-1]  # Reverse the byte order


def display_buffer_as_hex(buffer, bytes_per_line=16):
    """
    Displays the contents of a buffer as hexadecimal values.

    :param buffer: A bytes-like object (e.g., bytes, bytearray).
    :param bytes_per_line: Number of bytes to display per line (default: 16).
    """
    if not isinstance(buffer, (bytes, bytearray)):
        raise TypeError("Input must be a bytes-like object (bytes or bytearray).")

    offset = 0
    while offset < len(buffer):
        # Extract a chunk of the buffer
        chunk = buffer[offset:offset + bytes_per_line]

        # Convert to hexadecimal representation
        hex_values = ' '.join(f'{byte:02X}' for byte in chunk)

        # Create ASCII representation
        ascii_values = ''.join(chr(byte) if 32 <= byte <= 126 else '.' for byte in chunk)

        # Display offset, hex values, and ASCII representation
        print(f'{offset:08X}  {hex_values:<{bytes_per_line * 3}}  {ascii_values}')

        # Update offset
        offset += bytes_per_line

def create_metadata_binary(image_length, image_index, flash_base_address, device_type,
                           device_variant, version, image_tag, core_affinity):
    # Fixed Header Signature
    header_signature = 0xACEDD00B

    # Get the current date and time in ISO-8601 format
    current_datetime = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")

    # Ensure strings are null-terminated and fit within the defined length
    creation_datetime = current_datetime.encode('utf-8')[:31] + b'\0'
    version = version.encode('utf-8')[:31] + b'\0'
    image_tag = (image_tag.encode('utf-8')[:31] + b'\0') if image_tag else b'\0' * 32

    # Trailer Signature (constant value)
    trailer_signature = 0xF3EDB057

    # Pack the data into a binary format
    metadata = struct.pack(
        '<I I I I I I 32s 32s 32s B 3x I',
        header_signature,  # uint32_t
        image_index,  # uint32_t
        flash_base_address,  # uint32_t
        image_length,  # uint32_t
        device_type,  # uint32_t
        device_variant,  # uint32_t
        creation_datetime,  # Null-terminated UTF-8 string (32 bytes)
        version,  # Null-terminated UTF-8 string (32 bytes)
        image_tag,  # Null-terminated UTF-8 string (32 bytes)
        core_affinity,  # uint8_t
        trailer_signature  # uint32_t
    )

    return metadata
def aes_gcm_encrypt_with_iv_and_tag(input_file, key_file, image_index, flash_base_address,
                                    device_type, device_variant, version, core_affinity, image_tag=None):

    BLOCK_SIZE = 128

    # Read the AES key (128 bits = 16 bytes)
    with open(key_file, 'rb') as f:
        key = f.read()
        assert len(key) == 16, "Key must be 128 bits (16 bytes)"

    # Generate a random IV (96 bits = 12 bytes)
    iv = token_bytes(12)

    # Read the plaintext firmware file
    with open(input_file, 'rb') as f:
        firmware_data = f.read()

    # Pad firmware to 128-byte blocks if needed
    padding_len = (BLOCK_SIZE - len(firmware_data) % BLOCK_SIZE) % BLOCK_SIZE
    padded_firmware = firmware_data + (b'\x00' * padding_len)

    # Create metadata binary
    metadata = create_metadata_binary(len(padded_firmware), image_index, flash_base_address, device_type,
                                      device_variant, version, image_tag, core_affinity)

    # TEST ONLY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    unenc_output_file = os.path.splitext(input_file)[0] + ".une"
    with open(unenc_output_file, 'wb') as handle:
        handle.write(metadata)
        handle.write(padded_firmware)
    # ######################################################

    # Initialize AES-GCM cipher
    cipher = Cipher(algorithms.AES(key),
                    modes.GCM(iv),
                    backend=default_backend())
    encryptor = cipher.encryptor()

    # Encrypt the metadata
    encrypted_metadata = encryptor.update(metadata)

    encrypted_firmware = b''
    for i in range(0, len(padded_firmware), BLOCK_SIZE):
        block = padded_firmware[i:i + BLOCK_SIZE]
        encrypted_firmware += encryptor.update(block)

    # Finalize and get authentication tag
    encryptor.finalize()

    # Authentication tag (16 bytes for AES-GCM)
    tag = encryptor.tag

    # Add 2-byte filler (A5, 5A) between the tag and metadata
    filler = b'\xA5\x5A\xAA\x55'

    # Prepare the output filename with ".img" extension
    output_file = os.path.splitext(input_file)[0] + ".img"

    # Prepend the IV and Authentication Tag to the encrypted data
    with open(output_file, 'wb') as f:
        f.write(iv)          # Write the IV
        f.write(tag)         # Write the Authentication Tag
        f.write(filler)      # Write the padding bytes
        f.write(encrypted_metadata)
        f.write(encrypted_firmware)  # Write the ciphertext

    # Print details
    print(" Encryption complete:")
    print("       Input file:", input_file)
    print("      Output file:", output_file)
    print("        Key (hex):", key.hex())
    print("         IV (hex):", iv.hex())
    print("        Tag (hex):", tag.hex())
    print("  Metadata length:", len(metadata))
    print("      Orig Length:", os.path.getsize(input_file))
    print("     Image Length:", os.path.getsize(output_file))


# Main function to handle command-line arguments
if __name__ == "__main__":
    # Ensure correct usage
    if len(sys.argv) < 9:
        print("Usage: python aes_gcm_encrypt.py <firmware_file> <aes128_key_file> "
              "<image_index> <flash_base_address> <device_type> <device_variant> <version> <core_affinity> [<image_tag>]")
        sys.exit(1)

    # Command-line arguments
    firmware_file = sys.argv[1]
    aes_key_file = sys.argv[2]
    image_index = int(sys.argv[3])
    flash_base_address = int(sys.argv[4], 16)  # Assume hex input for FLASH base address
    device_type = int(sys.argv[5])
    device_variant = int(sys.argv[6])
    version = sys.argv[7]
    core_affinity = int(sys.argv[8])
    image_tag = sys.argv[9] if len(sys.argv) > 9 else None

    # Perform encryption
    aes_gcm_encrypt_with_iv_and_tag(firmware_file, aes_key_file, image_index, flash_base_address,
                                    device_type, device_variant, version, core_affinity, image_tag)

    """
    aes_gcm_encrypt_with_iv_and_tag(firmware_file, aes_key_file, image_index, flash_base_address,
                                    device_type, device_variant, version, core_affinity, image_tag)
    """
