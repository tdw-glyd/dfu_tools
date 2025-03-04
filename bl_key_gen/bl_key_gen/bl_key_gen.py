import struct
import os
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa


def binary_to_c_array(input_data, output_file, array_name="certBuf"):
    """
    Converts binary data to a C byte array.

    Args:
        input_data: Either a file path or bytes object
        output_file: Path to output header file
        array_name: Name for the C array

    Returns:
        None
    """
    # Read the binary data
    if isinstance(input_data, str):
        with open(input_data, 'rb') as f:
            binary_data = f.read()
    else:
        binary_data = input_data

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
        f.write(f"// Auto-generated RSA key data\n")
        f.write(f"// Size: {array_size} bytes\n\n")
        f.write(c_array)
        f.write("\n")

    print(f"Binary data has been converted to C array in '{output_file}'")


def generate_rsa_keypair(key_size=2048, output_pem=None):
    """
    Generate a new RSA key pair with the specified key size.

    Args:
        key_size: Size of the RSA key in bits (default: 2048)
        output_pem: Optional path to save the private key in PEM format

    Returns:
        The RSA private key object
    """
    # Generate a new RSA key
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=key_size,
        backend=default_backend()
    )

    # Save the private key to a PEM file if requested
    if output_pem:
        with open(output_pem, 'wb') as f:
            f.write(private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            ))
        print(f"Private key saved to {output_pem}")

    return private_key


def extract_rsa_keys_for_embedded(private_key, output_binary=None, c_header_output=None):
    """
    Extract RSA key components for embedded use and optionally convert to a C array.

    This follows your original extraction approach, combining modulus, public exponent,
    and private exponent into a single binary structure.

    Args:
        private_key: Either an RSA private key object or path to a PEM file
        output_binary: Optional path to save the binary data
        c_header_output: Optional path to save the C header file

    Returns:
        The binary data as bytes
    """
    # Define constants for maximum sizes (for a 2048-bit RSA key)
    RSA_MODULUS_SIZE = 256  # 2048 bits = 256 bytes
    RSA_EXPONENT_SIZE = 4  # Public exponent (e.g., 65537 fits in 4 bytes)
    RSA_PRIVATE_EXP_SIZE = 256  # Private exponent size (same as modulus for 2048-bit key)

    # Load the private key if a file path was provided
    if isinstance(private_key, str):
        with open(private_key, 'rb') as pem_file:
            private_key = serialization.load_pem_private_key(
                pem_file.read(),
                password=None,
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
    d = private_numbers.d  # Private exponent

    # Convert to binary in big-endian format (as in your original code)
    n_bin = n.to_bytes(RSA_MODULUS_SIZE, byteorder='big')
    e_bin = e.to_bytes(RSA_EXPONENT_SIZE, byteorder='big')
    d_bin = d.to_bytes(RSA_PRIVATE_EXP_SIZE, byteorder='big')

    # Get the lengths
    n_len = len(n_bin)
    e_len = len(e_bin)
    d_len = len(d_bin)

    # Pack data into a binary format (following your original structure)
    binary_data = (
            struct.pack("<I", n_len) + n_bin +  # Modulus
            struct.pack("<I", e_len) + e_bin +  # Public exponent
            struct.pack("<I", d_len) + d_bin  # Private exponent
    )

    # Save binary data if requested
    if output_binary:
        with open(output_binary, 'wb') as binary_file:
            binary_file.write(binary_data)
        print(f"RSA key components saved to {output_binary}")

    # Convert to C array if requested
    if c_header_output:
        binary_to_c_array(binary_data, c_header_output, "rsaKeyData")

    return binary_data


def extract_rsa_public_key_for_embedded(public_key, output_binary=None, c_header_output=None):
    """
    Extract RSA public key components for embedded use and optionally convert to a C array.

    This follows your original public key extraction approach.

    Args:
        public_key: Either an RSA public key object, private key object, or path to a PEM file
        output_binary: Optional path to save the binary data
        c_header_output: Optional path to save the C header file

    Returns:
        The binary data as bytes
    """
    # Load the key if a file path was provided
    if isinstance(public_key, str):
        with open(public_key, 'rb') as pem_file:
            pem_data = pem_file.read()
            try:
                # Try loading as public key first
                key = serialization.load_pem_public_key(pem_data, backend=default_backend())
            except:
                # If that fails, try loading as private key and extract public key
                key = serialization.load_pem_private_key(
                    pem_data, password=None, backend=default_backend()
                )
                if isinstance(key, rsa.RSAPrivateKey):
                    key = key.public_key()
                else:
                    raise ValueError("The provided key is not an RSA key")
    elif isinstance(public_key, rsa.RSAPrivateKey):
        # Extract public key from private key
        key = public_key.public_key()
    else:
        # Assume it's already a public key object
        key = public_key

    # Ensure the key is an RSA public key
    if not isinstance(key, rsa.RSAPublicKey):
        raise ValueError("Failed to obtain an RSA public key")

    # Extract the modulus and public exponent
    modulus = key.public_numbers().n
    exponent = key.public_numbers().e

    # Convert modulus and exponent to bytes
    modulus_bytes = modulus.to_bytes((modulus.bit_length() + 7) // 8, byteorder='big')
    mod_len = len(modulus_bytes)

    # Pad modulus to 256 bytes if smaller
    if len(modulus_bytes) < 256:
        modulus_bytes = b'\x00' * (256 - len(modulus_bytes)) + modulus_bytes

    exponent_bytes = exponent.to_bytes((exponent.bit_length() + 7) // 8, byteorder='big')

    # Prepare binary data according to your original format
    binary_data = (
            len(modulus_bytes).to_bytes(2, byteorder='little') +  # Modulus length (2 bytes)
            modulus_bytes +  # Modulus
            len(exponent_bytes).to_bytes(1, byteorder='big') +  # Exponent length (1 byte)
            exponent_bytes  # Exponent
    )

    # Save binary data if requested
    if output_binary:
        with open(output_binary, 'wb') as binary_file:
            binary_file.write(binary_data)
        print(f"RSA public key saved to {output_binary}")

    # Convert to C array if requested
    if c_header_output:
        binary_to_c_array(binary_data, c_header_output, "rsaPublicKeyData")

    return binary_data


def create_rsa_keypair_and_extract_to_c_arrays(key_size=2048,
                                               private_key_pem="private_key.pem",
                                               public_key_pem="public_key.pem",
                                               combined_header="rsa_key_data.h"):
    """
    Complete workflow: Create RSA key pair and extract both public and private keys to a single C header file.

    Args:
        key_size: Size of the RSA key in bits (default: 2048)
        private_key_pem: Path to save the private key PEM file
        public_key_pem: Path to save the public key PEM file
        combined_header: Path to save the combined C header with both keys

    Returns:
        Tuple of (private_key_object, private_key_binary, public_key_binary)
    """
    # 1. Generate the RSA key pair
    private_key = generate_rsa_keypair(key_size, private_key_pem)

    # 2. Save the public key to a PEM file
    public_key = private_key.public_key()
    with open(public_key_pem, 'wb') as f:
        f.write(public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        ))
    print(f"Public key saved to {public_key_pem}")

    # 3. Extract the complete key (private + public components)
    private_key_bin = extract_rsa_keys_for_embedded(
        private_key,
        output_binary=None  # Skip binary file output
    )

    # 4. Extract just the public key components
    public_key_bin = extract_rsa_public_key_for_embedded(
        public_key,
        output_binary=None  # Skip binary file output
    )

    # 5. Create a combined C header file with only the full key data
    with open(combined_header, 'w') as f:
        # Write header guard
        header_guard = combined_header.replace('.', '_').upper()
        f.write(f"#pragma once\n\n")
        f.write(f"// Auto-generated RSA key data\n")
        f.write(f"// Key size: {key_size} bits\n\n")

        # Write private key array (which includes all components)
        f.write("// RSA Key data (contains both public and private components)\n")
        f.write(f"#define RSA_KEY_LENGTH {len(private_key_bin)}\n")
        f.write(f"const unsigned char rsaKeyBuf[RSA_KEY_LENGTH] = {{\n")

        # Convert each byte to a formatted hex string
        hex_values = [f"0x{byte:02X}" for byte in private_key_bin]

        # Group the hex values into rows of 12 for better readability
        for i in range(0, len(hex_values), 12):
            f.write("    " + ", ".join(hex_values[i:i + 12]) + ",\n")

        # Finalize the array declaration
        f.write("};\n")

    print(f"RSA key data saved to {combined_header}")

    return private_key, private_key_bin, public_key_bin


# Example usage
if __name__ == "__main__":
    print("")
    print("::: Glydways Embedded Bootloader Key-Generation Tool :::")
    print("")

    #
    # Complete process: create keys and extract to C arrays
    #
    # IF YOU WANT DIFFERENT NAMES FOR THE INPUT OR OUTPUT FILES,
    # CHANGE THEM BELOW
    #
    private_key, private_key_bin, public_key_bin = create_rsa_keypair_and_extract_to_c_arrays(
        key_size=2048,
        private_key_pem="private_key.pem",
        public_key_pem="public_key.pem",
        combined_header="rsa_key_data.h"
    )

    print(f"Combined public/private key binary size: {len(private_key_bin)} bytes")
    print(f"Public key only binary size: {len(public_key_bin)} bytes")

    # Print key information for reference
    numbers = private_key.private_numbers()
    pub_numbers = numbers.public_numbers

    print(f"\nKey Information:")
    print(f"Modulus (n) bit length: {pub_numbers.n.bit_length()} bits")
    print(f"Public exponent (e): {pub_numbers.e}")
    print(f"Private exponent (d) bit length: {numbers.d.bit_length()} bits")
    print(f"\nBoth public and private key data written to rsa_key_data.h")

