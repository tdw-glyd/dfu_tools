#! python
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
"""
    APPLICATION: image_builder.py
    DESCRIPTION: Creates a final binary image file, formatted per the structure
                 expected by the bootloader.  This includes:

                 - Image binary
                 - Image metadata
                 - AES-GCM Initialization Vector (IV)
                 - AES-GCM Authentication tag

                 All this is encrypted using AES128-GCM, which includes
                 both authentication data AND encrypted data.
"""
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

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


def calculate_crc32(data_bytes):
    '''
    Computes a CRC-32 over the original binary (non-encrypted) binary
    image file (not including header metadata)

    :param data_bytes:
    :return: The CRC32 result.
    '''
    # CRC lookup table (same as in C version)
    crc_table = [
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
        0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
        0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
        0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
        0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
        0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
        0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
        0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
        0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
        0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
        0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
        0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
        0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
        0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
        0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
        0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
        0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
        0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
        0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
        0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
        0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    ]

    # Initialize CRC value
    crc = 0xFFFFFFFF

    # Process each byte in the data
    for byte in data_bytes:
        index = (crc ^ byte) & 0xFF
        crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_table[index]

    # Final XOR
    crc = ~crc & 0xFFFFFFFF

    return crc

def create_metadata_binary(image_length, image_index, flash_base_address, device_type,
                           device_variant, version, image_tag, raw_image_crc, core_affinity):
    """
    Builds the image metadata using the input parameters.

    :param image_length:
    :param image_index:
    :param flash_base_address:
    :param device_type:
    :param device_variant:
    :param version:
    :param image_tag:
    :param raw_image_crc
    :param core_affinity:
    :return:
    """
    # Fixed Header Signature
    header_signature = 0xACEDD00B

    # Get the current date and time in ISO-8601 format
    current_datetime = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")

    # Ensure strings are null-terminated and fit within the defined length
    creation_datetime = current_datetime.encode('utf-8')[:31] + b'\0'
    version = version.encode('utf-8')[:31] + b'\0'
    image_tag = (image_tag.encode('utf-8')[:27] + b'\0') if image_tag else b'\0' * 28

    # Trailer Signature (constant value)
    trailer_signature = 0xF3EDB057

    # Pack the data into a binary format
    metadata = struct.pack(
        '<I I I I I I 32s 32s 28s I B 3x I',
        header_signature,  # uint32_t
        image_index,  # uint32_t
        flash_base_address,  # uint32_t
        image_length,  # uint32_t
        device_type,  # uint32_t
        device_variant,  # uint32_t
        creation_datetime,  # Null-terminated UTF-8 string (32 bytes)
        version,  # Null-terminated UTF-8 string (32 bytes)
        image_tag,  # Null-terminated UTF-8 string (28 bytes)
        raw_image_crc, # uint32_t CRC of original image
        core_affinity,  # uint8_t
        trailer_signature  # uint32_t
    )

    return metadata
def aes_gcm_encrypt_with_iv_and_tag(input_file, key_file, image_index, flash_base_address,
                                    device_type, device_variant, version, core_affinity, image_tag=None):
    """
    Given the AES-128 key and various metadata, this will pre-pend the image binary with
    the formatted metedata, then encrypts the result using AES128-GCM and outputs a ".img"
    file that can be transferred to the target via the DFU mode protocols.

    NOTE: We compute a CRC32 over the original, un-encrypted file so that following
          installation to the targe FLASH memory, the board can verify that the
          written data matches what we computed here.

    :param input_file:
    :param key_file:
    :param image_index:
    :param flash_base_address:
    :param device_type:
    :param device_variant:
    :param version:
    :param core_affinity:
    :param image_tag:
    :return:
    """

    BLOCK_SIZE = 128

    # Read the AES key (128 bits = 16 bytes)
    try:
        with open(key_file, 'rb') as f:
            key = f.read()
            assert len(key) == 16, "Key must be 128 bits (16 bytes)"
    except Exception as error:
        print(f"Error while opening key file {key_file}: {error}")
        return

    # Generate a random IV (96 bits = 12 bytes)
    iv = token_bytes(12)

    # Read the plaintext firmware file
    try:
        with open(input_file, 'rb') as f:
            firmware_data = f.read()
    except Exception as error:
        print(f"Error while opening source firmware file {input_file}: {error}")
        return

    # Pad firmware to 128-byte blocks if needed
    padding_len = (BLOCK_SIZE - len(firmware_data) % BLOCK_SIZE) % BLOCK_SIZE
    padded_firmware = firmware_data + (b'\x00' * padding_len)

    # Compute the CRC32 over the original data
    raw_image_crc = calculate_crc32(padded_firmware)

    # Create metadata binary
    metadata = create_metadata_binary(len(padded_firmware), image_index, flash_base_address, device_type,
                                      device_variant, version, image_tag, raw_image_crc, core_affinity)

    # TEST ONLY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    unenc_output_file = os.path.splitext(input_file)[0] + ".une"
    try:
        with open(unenc_output_file, 'wb') as handle:
            handle.write(metadata)
            handle.write(padded_firmware)
    except Exception as error:
        print(f"Error while opening un-encrypted file {unenc_output_file}: {error}")
        return
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
    try:
        with open(output_file, 'wb') as f:
            f.write(iv)          # Write the IV
            f.write(filler)      # Write the padding bytes (to align to 16-bytes)
            f.write(tag)         # Write the Authentication Tag
            f.write(encrypted_metadata)
            f.write(encrypted_firmware)  # Write the ciphertext
    except Exception as error:
        print(f"Error while opening output file {output_file}: {error}")
        return

    # Print details
    print(" Encryption complete:")
    print("       Input file:", input_file)
    print("      Output file:", output_file)
    print("        Key (hex):", key.hex())
    print("         IV (hex):", iv.hex())
    print("        Tag (hex):", tag.hex())
    print("  Metadata length:", len(metadata))
    print(f"FLASH Target Addr: 0x{flash_base_address:08X}")
    print("      Orig Length:", os.path.getsize(input_file))
    print(f"  Raw Image CRC32: 0x{raw_image_crc:08X}")
    print("     Image Length:", os.path.getsize(output_file))


# Main function to handle command-line arguments
if __name__ == "__main__":
    """
    Primary program entry point.
    """
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

