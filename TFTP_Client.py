#!/usr/bin/python
from socket import *
from struct import *
from os.path import exists
import threading
from time import sleep

TFTP_SERVER_IP = '192.168.1.111'  # Board IP
TFTP_SERVER_PORT_NUMBER = 69
TFTP_CLIENT_SRC_PORT = 10000  # init src port
CHECKSUM = 0

TFTP_DATA_PACKET_SIZE = 1024  # 2B (opcode) | 2B (block number) | 0-512B (Block data) 516
TFTP_BLOCK_DATA = 512

DOWNLOADS_DIR = './Downloads/'
UPLOADS_DIR = './Uploads/'

filename_to_read = ''
received_data = ''  # variable to store our data content
is_last_block = False

sock = socket(AF_INET, SOCK_DGRAM)


def write_to_file(file_name, file_data):
    f = open(DOWNLOADS_DIR + file_name, 'w')
    f.write(file_data)
    f.close()
    print("File successfully downloaded")


def write_res(ack_data, file_name, socket_port):
    global is_last_block
    # 2B (opcode - 4ACK or 5NACK) | 2B (block number)
    opcode_receive = int.from_bytes(ack_data[0:2], "little")

    if opcode_receive == 5:
        err_desc = int.from_bytes(ack_data[2:4], "little")

        if err_desc == 6:
            # File already exists
            print("File already exists")
            is_last_block = True
            return
        else:
            # Error writing data to memory
            print("Error in writing data to memory")
            is_last_block = True
            return

    block_number = int.from_bytes(ack_data[2:4], "little")

    f = open(UPLOADS_DIR + file_name, 'r')
    f.seek(TFTP_BLOCK_DATA * block_number)
    data_content = f.read(TFTP_BLOCK_DATA)
    f.close()
    # Null terminated string
    data_content += '\0'

    if len(data_content) == 1:
        # last block
        print('File successfully uploaded to server!')
        is_last_block = True
    else:
        # Send Data packet (Block) to server
        # OPCODE (DATA=3) 2B | block_number (2B) | data content (nB <=512, Per block)
        tftp_data = pack('HH', 3, block_number) + bytes(data_content, 'utf-8')
        sock.sendto(tftp_data, (TFTP_SERVER_IP, socket_port))
        is_last_block = False


def read_res(data, socket_port):
    global received_data
    global is_last_block

    # 2B (opcode) | 2B (block number) | 0-512B (Block data)
    opcode_receive = int.from_bytes(data[0:2], "little")
    block_number = int.from_bytes(data[2:4], "little")
    read_data_content = data[4:].decode('utf-8')

    if opcode_receive == 5:
        # Indicate an error
        print("File not found")
        is_last_block = True
        return

    if read_data_content != 0:
        received_data += read_data_content

    if len(read_data_content) < TFTP_BLOCK_DATA:
        # Last packet
        is_last_block = True
    else:
        # Potentially more to come
        is_last_block = False

    # Send ACK back
    # OPCODE (ACK=4) 2B| block_number (2B)
    tftp_ack = pack('HH', 4, block_number)
    sock.sendto(tftp_ack, (TFTP_SERVER_IP, socket_port))


def read_req(filename, mode):
    # Read request

    # TFTP payload
    # OPCODE (READ=1) 2B| filename nBytes | ACK DATA 1Byte | Mode (netascii-txt) nBytes | unused 1Byte
    # Null terminated string
    filename += '\0'
    mode += '\0'
    tftp_data = pack('H', 1) + bytes(filename, 'utf-8') + pack('B', 0) + bytes(mode, 'utf-8') + pack('B', 0)
    sock.sendto(tftp_data, (TFTP_SERVER_IP, TFTP_SERVER_PORT_NUMBER))


def write_req(filename, mode):
    # Write request

    # TFTP payload
    # OPCODE (WRITE=2) 2B| filename nBytes | ACK DATA 1Byte | Mode (netascii-txt) nBytes | unused 1Byte
    filename += '\0'
    mode += '\0'
    tftp_data = pack('H', 2) + bytes(filename, 'utf-8') + pack('B', 0) + bytes(mode, 'utf-8') + pack('B', 0)
    sock.sendto(tftp_data, (TFTP_SERVER_IP, TFTP_SERVER_PORT_NUMBER))


def show_files_from_server_req():
    # Show files request

    # OPCODE (SHOW=9)
    show_req = pack('H', 9)
    sock.sendto(show_req, (TFTP_SERVER_IP, TFTP_SERVER_PORT_NUMBER))


def show_res(file_list):
    index = 1
    read_files_content_raw = file_list.decode('utf-8')
    if read_files_content_raw == '\t':
        print("No files to show")
    else:
        read_files_content = read_files_content_raw.split(".txt")
        for file in read_files_content:
            if file != '':
                print(str(index) + ")" + file + ".txt")
                index = index + 1


def tftp_session():
    is_session = True
    global received_data
    global is_last_block
    global filename_to_read
    sock.bind(('', 0))
    socket_port = sock.getsockname()[1]
    print(sock.getsockname()[1])

    # Start Session
    while is_session:
        received_data = ''
        is_last_block = False

        # 1=read/2=write
        try:
            operation = int(input("Type 0 to show current files in server\nType 1 to read from server, 2 to write to "
                                  "server: "))
        except:
            print("Invalid option, Try again")
            is_last_block = False
            return
        if operation == 1:
            # Read session

            # Get file name /including .txt extension
            filename_to_read = input("Type the name of the file to read: ")
            # Send RRQ
            read_req(filename_to_read, 'netascii')
            while not is_last_block:
                # Waiting for block data
                data = sock.recvfrom(TFTP_DATA_PACKET_SIZE)
                # Reading block data and acknowledge server
                read_res(data[0], socket_port)

            # Processing message
            write_to_file(filename_to_read, received_data)
        elif operation == 2:
            # Write session

            # Get file name /including .txt extension
            filename_to_write = input("Type the name of the file to write: ")
            # Check if file exists
            if exists(UPLOADS_DIR + filename_to_write) is False:
                print("No such file!")
                break
            # Send WRQ
            write_req(filename_to_write, 'netascii')
            while not is_last_block:
                # Waiting for ACK from server
                ack_data = sock.recvfrom(TFTP_DATA_PACKET_SIZE)
                write_res(ack_data[0], filename_to_write, socket_port)
        elif operation == 0:
            # Send request for showing current files in server
            show_files_from_server_req()
            #  Waiting for respond from server
            data = sock.recvfrom(TFTP_DATA_PACKET_SIZE)
            show_res(data[0])
            is_last_block = False
        else:
            print("Invalid option")
            is_last_block = True

        print("Thank you, Bye")
        is_session = False


def main():
    tftp_session()
    sock.close()


if __name__ == "__main__":
    main()
