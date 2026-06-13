with open("./program.bin", mode='rb') as file:
    fileContent = file.read()
    
    # Loop through the file 4 bytes at a time
    for i in range(0, len(fileContent), 4):
        # Grab a 4-byte chunk (one 32-bit instruction)
        chunk = fileContent[i:i+4]
        
        # Reverse the order of the bytes in this chunk to fix Little-Endian
        big_endian_chunk = chunk[::-1]
        
        # Convert the fixed chunk to a binary string
        binary_string = "".join(f"{byte:08b}" for byte in big_endian_chunk)
        
        print(binary_string)