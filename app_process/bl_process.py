# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
"""
    Application Header Creator for Glydways S32-based system boards.
    Copyright 2024 by Glydways, Inc.  All Rights Reserved.
"""
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
# $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$


import io, hashlib, hmac
import os
import argparse
import struct
from datetime import datetime
import base64

# THIS MUST MATCH THE SIGNATURE THE BOOTLOADER C-CODE USES!!!
app_image_header_signature = 0xACEDD00B

c_data_output_path = "C:/Glydways/bl_demo/bl_shared/interfaces/uart/src/key_data.h"

#
# Maps the names of the device types to their
# enumerated values
#
dev_type_map = {
    "VLC": 1,
    "ATP": 2,
    "ESTOP": 3,
    "VMC": 4,
    "CABIN": 5,
    "BMS": 6,
    "VGW": 7,
    "LOG": 8,
    "CABIN": 9,
    "FOO1": 10,
    "FOO2": 11,
    "FOO3": 12,
    "FOO4": 13,
    "FOO5": 14
}

import os
def binary_to_c_array(input_file, output_file, array_name="certBuf"):
    """
    Converts a binary input file to a C byte array.  Used as a debug
    tool to allow easy import of RSA keys as a data struct linked
    directly into the image, for testing.

    :param input_file:
    :param output_file:
    :param array_name:
    :return:
    """
    # Read the binary file
    with open(input_file, 'rb') as f:
        binary_data = f.read()

    # Prepare the header file content
    array_size = len(binary_data)
    header_guard = output_file.replace('.', '_').upper() + '_H'

    # Create C array declaration
    c_array = f"const unsigned char {array_name}[{array_size}] = {{\n"

    # Convert each byte to a formatted hex string
    hex_values = [f"0x{byte:02X}" for byte in binary_data]

    # Group the hex values into rows of 12 for better readability
    for i in range(0, len(hex_values), 12):
        c_array += "    " + ", ".join(hex_values[i:i + 12]) + ",\n"

    # Finalize the C array declaration
    c_array = c_array.rstrip(",\n") + "\n};\n"

    # Write the array to the output file
    with open(output_file, 'w') as f:
        f.write(f"#pragma once\n")
        f.write("\n")
        f.write(f"// Auto-generated from {input_file}\n")
        f.write(f"// Size: {array_size} bytes\n\n")
        f.write(c_array)
        f.write("\n")

    print(f"Binary data from '{input_file}' has been converted to C array in '{output_file}'")


from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
import binascii

def extract_rsa_public_key_components(pem_file_path, binary_output_path):
    # Read the PEM file
    with open(pem_file_path, 'rb') as pem_file:
        pem_data = pem_file.read()

    # Load the public key from PEM data
    public_key = serialization.load_pem_public_key(pem_data)

    if isinstance(public_key, rsa.RSAPublicKey):
        # Extract the modulus and public exponent
        modulus = public_key.public_numbers().n
        exponent = public_key.public_numbers().e

        # Convert modulus and exponent to bytes
        modulus_bytes = modulus.to_bytes((modulus.bit_length() + 7) // 8, byteorder='big')
        mod_len = len(modulus_bytes)
        if len(modulus_bytes) < 256:
            modulus_bytes = b'\x00' * (256 - len(modulus_bytes)) + modulus_bytes

        exponent_bytes = exponent.to_bytes((exponent.bit_length() + 7) // 8, byteorder='big')

        modulus_bytes_le = modulus_bytes[::-1]
        exponent_bytes_le = exponent_bytes[::-1]

        # Prepare binary data to save
        with open(binary_output_path, 'wb') as binary_file:
            # Write modulus size (2 bytes) and modulus
            binary_file.write(len(modulus_bytes_le).to_bytes(2, byteorder='little'))
            binary_file.write(modulus_bytes)

            # Write exponent size (1 byte) and exponent
            binary_file.write(len(exponent_bytes_le).to_bytes(1, byteorder='big'))
            binary_file.write(exponent_bytes)

        print(f"RSA public key saved to {binary_output_path}")

    else:
        print("The PEM file does not contain an RSA public key.")

# EXTRACT PUBLIC/PRIVATE KEY COMPONENTS
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa

# Define constants for maximum sizes (for a 2048-bit RSA key)
RSA_MODULUS_SIZE = 256  # 2048 bits = 256 bytes
RSA_EXPONENT_SIZE = 4   # Public exponent (e.g., 65537 fits in 4 bytes)
RSA_PRIVATE_EXP_SIZE = 256  # Private exponent size (same as modulus for 2048-bit key)

def extract_rsa_keys_for_embedded_little_endian(private_key_pem_path: str, output_binary_filename: str):
    # Load the private key from a PEM file
    with open(private_key_pem_path, 'rb') as pem_file:
        private_key = serialization.load_pem_private_key(
            pem_file.read(),
            password=None,  # Add password if the key is encrypted
            backend=default_backend()
        )

    # Ensure the key is an RSA key
    if not isinstance(private_key, rsa.RSAPrivateKey):
        raise ValueError("The provided key is not an RSA private key")

    # Extract private key numbers (components)
    private_numbers = private_key.private_numbers()
    public_numbers = private_numbers.public_numbers

    # Extract essential components
    n = public_numbers.n  # Modulus
    e = public_numbers.e  # Public exponent
    d = private_numbers.d # Private exponent

    # Convert to binary in little-endian format
    n_bin = n.to_bytes(RSA_MODULUS_SIZE, byteorder='big')
    d_bin = d.to_bytes(RSA_PRIVATE_EXP_SIZE, byteorder='big')
    e_bin = e.to_bytes(RSA_EXPONENT_SIZE, byteorder='big')

    n_len = len(n_bin)
    d_len = len(d_bin)
    e_len = len(e_bin)

    binary_data = (
        struct.pack(f'<I', n_len) + n_bin +  # Modulus (little-endian)
        struct.pack(f'<I', e_len) + e_bin +  # Public exponent (little-endian)
        struct.pack(f'<I', d_len) + d_bin    # Private exponent (little-endian)
    )

    with open(output_binary_filename, 'wb') as binary_file:
        binary_file.write(binary_data)

    # TEST ONLY! SAVE A C DATA STRUCTURE OF THE BINARY FILE
    binary_to_c_array(output_binary_filename, c_data_output_path)


    return

def sign_image_file(src_image_filename, private_pem):
    """
    Given a binary image file, this will generate the signature and save to
    a filenam that matches the source name, with a ".sig" extension.

    :param src_image_file:
    :return: Name of the signature file
    """

    signature_output_file_name = None

    try:
        if src_image_filename is not None and private_pem is not None:

            # Remove extension from the image file name and create the output name
            base_file_name = os.path.splitext(src_image_filename)[0]
            signature_output_file_name = base_file_name + ".sig"

            # Build the shell command to generate the signature.
            cmd = "openssl dgst -sha256 -sign " + private_pem + " -out " + signature_output_file_name + " " + src_image_filename

            # Execute the shell command
            print(f" Image file to sign      : {src_image_filename} ")
            print(f" Image file size         : {get_image_file_size(src_image_filename)} bytes.")
            print(f" Output signature file   : {signature_output_file_name}")

            os.system(cmd)
    except Exception as error:
        print(f"Exception while signing image: {error}")

    return signature_output_file_name

def create_patterned_bytearray(pattern, length):
    """
    Creates a bytearray buffer that is filled with the pattern provided.

    :param pattern: 2-byte pattern to fill the buffer with
    :param length: Length of the buffer to be created.
    :return: The bytearray buffer
    """
    # Convert the pattern into bytes
    pattern_bytes = [pattern >> 8, pattern & 0xFF]

    # Repeat the pattern until the byte array is of the desired length
    full_pattern = pattern_bytes * (length // len(pattern_bytes))
    return full_pattern[:length]


def get_mapfile_item_address(map_file_path, key):
    """
    Searches the "map_file_path" to find the section key and if located,
    returns the address (as an integer).

    :param map_file_path:  The path and name of the map file
    :param key: The name of the key whose address is desired.
    :return: The integer value of the key, if found.
    """
    try:
        with open (map_file_path, "r") as f:
            datafile = f.readlines()
            found = False
            for line in datafile:
                if key in line:
                    if "*(" not in line:
                        key_list = line.split()
                        if len(key_list) > 1:
                            return int(key_list[1], 16)
    except Exception as error:
        print(f"Exception while retrieving mapfile key: {error}")

    return 0


def get_image_file_size(file_path):
    """
    Given the name of a file, return its size, in bytes

    :param file_path: Name of the file to get size of
    :return: Size in bytes.
    """
    try:
        return os.path.getsize(file_path)
    except Exception as err:
        print(f"Exception while fetching image size: {e}")
        return 0


def hash_app_SHA256(file_path):
    """
    Use a SHA256 hash algorithm to return a 32-byte digest that
    was computed over the file whose name was provided.

    :param file_path: Name of file to hash
    :return: The 32-byte hash digest and its length, as a tuple.
    """
    try:
        with open (file_path, "rb") as f:
            digest = hashlib.file_digest(f, "sha256")
            return digest.digest(), digest.digest_size
    except Exception as error:
        print(f"Exception while performing SHA256 hash: {error}")
        return 0, 0


def get_device_name_list_string():
    """
    Returns a string that lists the available device types, using the
    'dev_type_map' dictionary.

    :return: String list of names
    """
    try:
        dev_list = "["
        for x in dev_type_map:
            dev_list += x + ','
        dev_list = dev_list.rstrip(',') + ']'
    except Exception as error:
        print(f"Exception while building device name list: {error}")
        dev_list = '[NONE]'

    return dev_list


def dev_type_id_from_name(name_str):
    """
    Given a device type name, this will attempt to return the enumerated
    value that is associated with it.

    :param name_str: Device type name
    :return: Integer enumeration value
    """
    try:
        ret = dev_type_map.get(name_str)
        return ret
    except Exception as error:
        print(f"Exception while retrieving device name type: {error}")
        return -1


def config_arg_parser():
    """
    Sets up the command-line parser to include the required and
    optional arguments, etc.  Returns the parser object that we
    use to parse the command-line.

    :return: ArgumentParser object instance
    """
    try:
        clparser = argparse.ArgumentParser("bl_process",
                                         description="Generates application image header and security content.")

        help_str = "The name of the input file, without extension."
        clparser.add_argument("-s",
                            action='store',
                            help=help_str,
                            required=True)

        help_str = "The name of the output file. Will always end with '.img'."
        clparser.add_argument("-o",
                            action='store',
                            help=help_str,
                            required=False)

        help_str = "The type of device " + get_device_name_list_string() + '.'
        clparser.add_argument("-t",
                            action='store',
                            help=help_str,
                            required=True)

        help_str = "The image version (MM.mm.rr)."
        clparser.add_argument("-v",
                            action='store',
                            help=help_str,
                            required=True)

        help_str = "Image flags."
        clparser.add_argument("-f",
                            action='store',
                            help=help_str,
                            required=False)

        help_str = "Name of the (input) private key .pem file."
        clparser.add_argument("-private",
                            action='store',
                            help=help_str,
                            required=False)

        help_str = "Name of the (input) public key .pem file."
        clparser.add_argument("-public",
                            action='store',
                            help=help_str,
                            required=False)

    except Exception as error:
        print(f"Exception while configuring command-line parser: {error}")
        clparser = None

    return clparser


def get_iso_datetime():
    """
    Gets the current date/time as an ISO 8601 formatted string.

    :return: ISO formatted string
    """
    try:
        # iso_date = datetime.now().replace(microsecond=0).isoformat().encode('ascii')
        iso_date = datetime.now().replace(microsecond=0).isoformat()
        return iso_date
    except Exception as error:
        print(f"Exception while fetching ISO date/time: {error}")
        return ""


def write_output_image(src_image_filename, image_device_type, image_flags, image_version, output_image_filename, public_key_pem, private_key_pem):
    """
    Creates the final image file, using command-line inputs, the .map file and things like
    the current date & time to create:

    1. Certificate that will be the first data in the image.
    2. Metadata that helps the bootloader know how to boot the image.
    3. The image data itself, including the original IVT produced by
       the tools.

    :param src_image_filename:
    :param image_device_type:
    :param image_flags:
    :param image_version:
    :param output_image_filename:
    :return: nothing
    """

    # Fill "padding" with alternating byte values
    fill_pattern = 0xAA55
    fill_pattern_string = "0x%0.2X" % fill_pattern

    try:
        # Where are we running from?
        cwd = os.getcwd()
        src_filename = os.path.splitext(src_image_filename)[0]

        # Create filenames with path info, get file sizes, etc.
        device_type_id = dev_type_id_from_name(image_device_type)
        src_filename = cwd + '/' + src_filename
        src_image_name = src_filename + ".bin"
        map_filename = src_filename + ".map"
        image_size = get_image_file_size(src_image_name)
        intermediate_file_name = src_filename + ".int"


        #
        # Convert public key PEM filename to binary
        #
        if public_key_pem is not None:
            public_key_filename = cwd + "/" + os.path.splitext(public_key_pem)[0] + ".pem"
            public_key_binary_filename = cwd + "/" + "public" + ".bin"
            extract_rsa_public_key_components(public_key_filename, public_key_binary_filename)

        #
        # Convert the private key PEM filename to binary
        #
        if private_key_pem is not None:
            private_key_filename = cwd + "/" + os.path.splitext(private_key_pem)[0] + ".pem"
            private_key_binary_filename = cwd + "/" + "key_data" + ".bin"
            extract_rsa_keys_for_embedded_little_endian(private_key_filename, private_key_binary_filename)


        # Build the final output file name
        if output_image_filename is None:
            output_filename = src_filename + ".img"
        else:
            output_filename = cwd + '/' + os.path.splitext(output_image_filename)[0] + ".img"

    except Exception as error:
        print(f"Exception while buidling file names: {error}")
        return

    try:
        # format an ISO 8601 date/time stamp
        dt_string = get_iso_datetime()
        struct_datetime = dt_string.encode('ascii').ljust(32, b'\x00')

        struct_version = image_version.encode('ascii').ljust(32, b'\x00')

        padding  = create_patterned_bytearray(fill_pattern, 176)

        # Display what we know
        print("")
        print(f" Device Type ID          : {image_device_type}")
        print(f" Image flags             : {hex(image_flags)}")
        print(f" Source base file name   : {src_filename}")
        print(f" Source binary file name : {src_image_name}")
        print(f" Intermediate file name  : {intermediate_file_name}")
        print(f" Output file name        : {output_filename}")
        print(f" Source image file size  : {image_size} bytes")
        print(f" Date/Time               : {dt_string}")
        print(f" Version                 : {image_version}")
        print(f" Fill pattern            : {fill_pattern_string}")

    except Exception as error:
        print(f"Exception while building date/time & version keys...: {error}")
        return

    try:
        # Build the structure format string
        fmt = '<III32s32sI176s'

        #
        # Create the binary structure of the header data
        #
        packed_data = struct.pack(fmt,
                                  app_image_header_signature,
                                  image_flags,
                                  image_size,
                                  struct_datetime,
                                  struct_version,
                                  device_type_id,
                                  bytes(padding))
    except Exception as error:
        print(f"Exception while setting up the C-STRUCT header data: {error}")
        return

    try:
        with open(src_image_name, 'rb') as src:
            # Load the binary into a buffer
            bin_data = src.read()

        with open(intermediate_file_name, 'wb') as f:
            # Write metadata and the binary, in that order
            f.write(packed_data)
            f.write(bin_data)

        #
        # Now create a signature over the combined metadata and image
        #
        signature_filename = sign_image_file(intermediate_file_name, private_key_pem)
        if signature_filename is not None:
            # Now add the signature to the top of the binary.
            with open(intermediate_file_name, 'rb') as src:
                bin_data = src.read()

            with open(signature_filename, "rb") as sig:
                signature = sig.read()

            with open(output_filename, "wb") as f:
                f.write(signature)
                f.write(bin_data)

        print(f" Final image file size   : {get_image_file_size(output_filename)} bytes.")
    except Exception as error:
        print(f"Exception while writing image file: {error}")

    return

if __name__ == "__main__":

    image_flags = 0
    device_type = -1

    # Config cmd-line parser and parse the arguments
    parser = config_arg_parser()
    args = parser.parse_args()

    public_key_pem = None
    if args.public is not None:
        public_key_pem = args.public

    private_key_pem = None
    if args.private is not None:
        private_key_pem = args.private

    src_output_filename = None
    if args.o is not None:
        src_output_filename = args.o

    if args.f:
        image_flags_str = args.f
        image_flags = int(image_flags_str, 16)

    # Build the image file, with its certs, metadata, etc.
    write_output_image(args.s, args.t, image_flags, args.v, src_output_filename, public_key_pem, private_key_pem)

    print("")
    print(" Done!")
    print("")

    exit(0)

