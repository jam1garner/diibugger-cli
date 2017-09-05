data = None

with open("server.bin", "rb") as f:
    data = f.read()

with open("code.h", "w") as f:
    header = "#pragma once\nstatic const unsigned char code_bin[] = {\n"

    if type(data[0]) is int:
        for byte in data:
            header += ' 0x%02x,' % byte
    else:
        for byte in data:
            header += ' 0x%02x,' % ord(byte)

    header = header.rstrip(",\n ")
    header += "\n};\nstatic const unsigned int code_length = "
    header += str(len(data)) + ";\n"

    f.write(header)
