#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <jansson.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>

int main(int argc, char const* argv[]) {
	if (argv[1] == NULL){
		printf("Missing a port number\n");
		return -1;
	}

	int port = atoi(argv[1]);
   	int server_fd, new_socket;
   	struct sockaddr_in address;
   	socklen_t addrlen = sizeof(address);
   	char buffer[2000] = { 0 };

   	ssize_t bytes_received;
 
	// Creating socket file descriptor
   	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       	perror("socket failed");
       	return -1;
   	}

   	// Setting up server address
	address.sin_family = AF_INET;
   	address.sin_addr.s_addr = INADDR_ANY;
   	address.sin_port = htons(port);
 
	//Bind
   	if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
       	perror("bind failed");
       	return -1;
   	}

   	//listen
   	if (listen(server_fd, 3) < 0) {
       	perror("listen");
       	return -1;
   	}

   	//accept
   	if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
       	perror("accept");
       	return -1;
   	}

   	// Open a file for writing the config file data
   	FILE *file = fopen("config.txt", "wb");
   	if (file == NULL) {
       	perror("fopen");
       	close(new_socket);
       	close(server_fd);
       	return -1;
   	}

   	// Receive and write the file data
   	while ((bytes_received = recv(new_socket, buffer, sizeof(buffer), 0)) > 0) {
       	fwrite(buffer, 1, bytes_received, file);
   	}

   	if (bytes_received < 0) {
       	perror("recv");
   	}

   	fclose(file);
   	close(new_socket);
   	close(server_fd);
    	
	//Probing Phase begins
	//parsing config file
	json_error_t error;
	json_t *jsonData = json_load_file("config.txt", 0, &error);

	if (!jsonData){
		printf("Error: could not read config file\n");
		return -1;
	}

	json_t *udp_dest_port = json_object_get(jsonData, "udp_dest_port");

	int probing_server_fd;
	struct sockaddr_in probing_server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	
	//create udp server socket
	if ((probing_server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		printf("error creating socket");
		return -1;
	}


	
	// Set up the server address
   	probing_server_addr.sin_family = AF_INET;
    probing_server_addr.sin_port = htons(atoi(json_string_value(udp_dest_port)));
    probing_server_addr.sin_addr.s_addr = INADDR_ANY;
	

	//binding
	int bindErrCode = bind(probing_server_fd, (struct sockaddr*)&probing_server_addr, sizeof(probing_server_addr));
	if(bindErrCode < 0){
		perror("bind");
        printf("Couldn't bind to the port\n");
        close(probing_server_fd);
        return -1;
	}

	//initialize variables for select and timeout
	struct timeval timeout;
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(probing_server_fd, &readfds);
	
	//low entropy probing begins
	json_t *num_udp_packets = json_object_get(jsonData, "num_udp_packets");

	struct timeval firstLowPacketTime;
	struct timeval latestLowPacketTime;
	bool firstPacket = true;
	bool lowProbing = true;
	int packetsReceived = 0;
	while (lowProbing) {

		int activity = select(probing_server_fd + 1, &readfds, NULL, NULL, &timeout);
		if (activity == -1){
			perror("select error");
			return -1;
		} else if (activity == 0){
			//handles timeouts
			lowProbing = false;
		} else {
			//receiving packets
			ssize_t bytes_received = recvfrom(probing_server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
	       	if (bytes_received < 0) {
	           		perror("recvfrom");
	           		return -1;
	       	}
			gettimeofday(&latestLowPacketTime, 0);
			if (firstPacket){
				firstLowPacketTime = latestLowPacketTime;
				firstPacket = false;
			}
			
			
	       	buffer[bytes_received] = '\0';
	       	packetsReceived++;
	       	if (packetsReceived >= atoi(json_string_value(num_udp_packets))){
	       		lowProbing = false;
	       	}
	
		}
		
       	
   	}
   	

	//High entropy probing
	json_t *inter_meas_time = json_object_get(jsonData, "inter_meas_time");
	timeout.tv_sec = atoi(json_string_value(inter_meas_time)) + 15;
    timeout.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(probing_server_fd, &readfds);
    
	struct timeval firstHighPacketTime;
	struct timeval latestHighPacketTime;
	firstPacket = true;
	bool highProbing = true;
	packetsReceived = 0;
	while (highProbing) {

		int activity = select(probing_server_fd + 1, &readfds, NULL, NULL, &timeout);
		if (activity == -1){
			perror("select error");
			return -1;
		} else if (activity == 0){
			//handles timeouts
			highProbing = false;
		} else {
			//receiving packets
			ssize_t bytes_received = recvfrom(probing_server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
	       	if (bytes_received < 0) {
	           		perror("recvfrom");
	           		return -1;
	       	}
			gettimeofday(&latestHighPacketTime, 0);
			if (firstPacket){
				firstHighPacketTime = latestHighPacketTime;
				firstPacket = false;
			}
			
			
	       	buffer[bytes_received] = '\0';
	       	packetsReceived++;
	       	if (packetsReceived >= atoi(json_string_value(num_udp_packets))){
	       		highProbing = false;
	       	}
			
		}
	
       	
   	}

	//calculates the time taken for low and high entropy data
   	long lowProbingTime = (latestLowPacketTime.tv_sec * 1000 + latestLowPacketTime.tv_usec / 1000) - (firstLowPacketTime.tv_sec * 1000 + firstLowPacketTime.tv_usec / 1000);
	long highProbingTime = (latestHighPacketTime.tv_sec * 1000 + latestHighPacketTime.tv_usec / 1000) - (firstHighPacketTime.tv_sec * 1000 + firstHighPacketTime.tv_usec / 1000);
	close(probing_server_fd);

	//post-probing phase begins
	json_t *tcp_post_port = json_object_get(jsonData, "tcp_post_port");
	port = atoi(json_string_value(tcp_post_port));
   	int post_server_fd, post_client_fd;
   	struct sockaddr_in post_server_addr;
   	socklen_t post_server_addrlen = sizeof(post_server_addr);

   	//create socket
  	if ((post_server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      	perror("socket failed");
      	exit(EXIT_FAILURE);
  	}

   	//set up address
	post_server_addr.sin_family = AF_INET;
   	post_server_addr.sin_addr.s_addr = INADDR_ANY;
   	post_server_addr.sin_port = htons(port);
 
	//Bind
   	if (bind(server_fd, (struct sockaddr*)&post_server_addr, sizeof(post_server_addr)) < 0) {
       	perror("bind failed");
       	return -1;
   	}

	//listen
   	if (listen(post_server_fd, 3) < 0) {
       	perror("listen");
       	exit(EXIT_FAILURE);
   	}

	//accept
   	if ((post_client_fd = accept(post_server_fd, (struct sockaddr*)&post_server_addr, &post_server_addrlen)) < 0) {
       	perror("accept");
       	exit(EXIT_FAILURE);
   	}

	//computes result of compression
	char* result;
	if ((highProbingTime - lowProbingTime) > 100){
		result = "Compression detected!";
	} else {
		result = "No compression was detected.";
	}

	//Sends the client the final result
   	send(post_client_fd, result, strlen(result), 0);

	close(post_client_fd);
	close(post_server_fd);
	json_decref(jsonData);
	return 0;
}



