Zachary Alon

#Compression Detection Program

The Compression Detection Program detects if there is network compression occurring between a client system and server system.  

## Build Requirements

- GCC (version 7.0 or later)
- Jansson library (version 2.13.1-1.1build3)

## Building the Project

1. Ensure you have GCC installed (version 7.0 or later).
2. Install Jansson library on both client and server systems using the following command:

	sudo apt-get install libjansson-dev

3. Build the project using the following commands: 
	#on the client system
	gcc -o compdetect_client compdetect_client.c -ljansson
	#on the server system
	gcc -o compdetect_server compdetect_server.c -ljansson

##Executing the Project

1. Set desired values in a config file in JSON format, including the ip address of the server system. JSON key names must match the names of the attached sample config file.
2. A file named "random_file" must be in the same directory as compdetect_client and contain a set of randomly generated bits
3. Start up the server program using the following command:

	./compdetect_server <port>
	
	<port> must match the value of "tcp_pre_port" in the client's config file.

4. Execute the client program using the following command:

	./compdetect_client <config-file-path>

	<config-file-path> must be the file path of the config file.

##Other info

This program only works on Linux.

The server will wait up to 15 seconds after the last probing packet it received if not all packets sent are accounted for.

##Viewing Results

Once the program has finished running, the client will print "Compression detected!" or "No compression was detected." 

##Incomplete features

This project is missing part 2 functionality and standalone.pcap.
	
	