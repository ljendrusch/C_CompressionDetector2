### CS 621
### Logan Jendrusch
### Prof. Vahab Pournaghshband

# Compression Detection Client / Server
This repository contains a client program and server program that attempt to detect data compression on the path between two machines connected to the internet.

## Setup
- Clone / download the repository anywhere onto your machine
- Ensure you have a C compiler for your hardware; the Makefile is set up to use gcc
- Open a terminal

## Usage
1. Server machine IP address
    On the server machine, run [ifconfig] in a terminal
    Note the value for inet
    In config.json, change the "server_ipa" value to match (keep the quotes)
2. Install and run the programs
    On each machine open a terminal and enter:
        cd (download_path)/compr_detect_cs
        make
    First start the server program on the server machine:
        bin/server 8787
    Then run the client program on the client machine:
        bin/client config.json

The port to listen to on the server (8787 here) may be changed to your liking, but the "port_tcp" value in config.json must match. Additionally, other values in config.json may be changed as needed.
