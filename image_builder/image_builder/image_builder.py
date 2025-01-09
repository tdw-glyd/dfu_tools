import os
import sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from secrets import token_bytes

def to_little_endian(data: bytes) -> bytes:
    """Convert bytes to little-endian representation."""
    return data[::-1]  # Reverse the byte order

def aes_gcm_encrypt_with_iv_and_tag(input_file, key_file):
    # Read the AES key (128 bits = 16 bytes)
    with open(key_file, 'rb') as f:
        key = f.read()
        assert len(key) == 16, "Key must be 128 bits (16 bytes)"

    # Generate a random IV (96 bits = 12 bytes)
    iv = token_bytes(12)

    # Read the plaintext firmware file
    with open(input_file, 'rb') as f:
        plaintext = f.read()

    # Initialize AES-GCM cipher
    cipher = Cipher(algorithms.AES(key), modes.GCM(iv), backend=default_backend())
    encryptor = cipher.encryptor()

    # Encrypt the plaintext
    ciphertext = encryptor.update(plaintext) + encryptor.finalize()

    # Authentication tag (16 bytes for AES-GCM)
    tag = encryptor.tag

    iv_le = iv    # to_little_endian(iv)
    tag_le = tag  # to_little_endian(tag)

    # Prepare the output filename with ".img" extension
    output_file = os.path.splitext(input_file)[0] + ".img"

    # Prepend the IV and Authentication Tag to the encrypted data
    with open(output_file, 'wb') as f:
        f.write(iv_le)        # Write the IV
        f.write(tag_le)       # Write the Authentication Tag
        f.write(ciphertext)  # Write the ciphertext

    # Print details
    print("Encryption complete!")
    print("Input file:", input_file)
    print("Output file:", output_file)
    print("Key (hex):", key.hex())
    print("IV (hex):", iv.hex())
    print("Tag (hex):", tag.hex())


# Main function to handle command-line arguments
if __name__ == "__main__":
    # Ensure correct usage
    if len(sys.argv) != 3:
        print("Usage: python aes_gcm_encrypt.py <firmware_file> <aes128_key_file>")
        sys.exit(1)

    # Command-line arguments
    firmware_file = sys.argv[1]
    aes_key_file = sys.argv[2]

    # Perform encryption
    aes_gcm_encrypt_with_iv_and_tag(firmware_file, aes_key_file)
