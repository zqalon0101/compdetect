#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <jansson.h>

int main(int argc, char *argv[]){
	//Pre-Probing Phase
	if (argv[1] == NULL){
		printf("Provide a config file\n");
		return 0;
	}
	char* config_file_path = argv[1];
	//first parse data from config.json file
	json_error_t error;
	json_t *jsonData = json_load_file(config_file_path, 0, &error);

	if (!jsonData){
		printf("Error: could not read config file\n");
		return -1;
	}

	json_t *serv_ip = json_object_get(jsonData, "serv_ip");
	json_t *tcp_pre_port = json_object_get(jsonData, "tcp_pre_port");
	
	//pre probing tcp connection
	struct sockaddr_in serv_addr;
	ssize_t bytes_read;

	int sockfd =  socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		printf("Error: Could not create socket\n");
		return 1;
	}

	//setting up server address
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(json_string_value(tcp_pre_port)));
	if (inet_pton(AF_INET, json_string_value(serv_ip), &serv_addr.sin_addr) <= 0){
		printf("invalid address\n");
		close(sockfd);
		return -1;		
	}	

	int status = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (status < 0){
		printf("couldn't connect\n");
		return -1;
	}

	// Open the file for reading
	FILE *file = fopen(config_file_path, "rb");
	if (file == NULL) {
            perror("fopen");
	    close(sockfd);
            exit(1);
        }	

	// Read and send the file data
	char buffer[1024] = { 0 };
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		send(sockfd, buffer, bytes_read, 0);
    	}

    	if (bytes_read < 0) {
        	perror("fread");
  	}

	fclose(file);
	close(sockfd);

	//Probing Phase begins
	
	int probing_client_fd;
	struct sockaddr_in probing_client_addr;
	struct sockaddr_in probing_server_addr;

	if ((probing_client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Could not create socket");
		return -1;
	}	

	//binding to local port
	json_t *udp_source_port = json_object_get(jsonData, "udp_source_port");
	probing_client_addr.sin_family = AF_INET;
	probing_client_addr.sin_port = htons(atoi(json_string_value(udp_source_port)));
	probing_client_addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(probing_client_fd, (struct sockaddr*)&probing_client_addr, sizeof(probing_client_addr)) < 0){
		perror("bind");
		close(probing_client_fd);
		return -1;
	}
	//setting server info
	json_t *udp_dest_port = json_object_get(jsonData, "udp_dest_port");

	probing_server_addr.sin_family = AF_INET;
	probing_server_addr.sin_port = htons(atoi(json_string_value(udp_dest_port)));
	if (inet_pton(AF_INET, json_string_value(serv_ip), &(probing_server_addr.sin_addr)) <= 0){
		printf("invalid server address\n");
		close(probing_client_fd);
		return -1;		
	}

	// Set the Don't Fragment option for the socket
    int flag = IP_PMTUDISC_DO;
    if (setsockopt(probing_client_fd, IPPROTO_IP, IP_MTU_DISCOVER, &flag, sizeof(int)) == -1) {
        perror("don't fragment sockopt");
        close(probing_client_fd);
        return -1;
    }


	json_t *udp_payload_size = json_object_get(jsonData, "udp_payload_size");
	json_t *num_udp_packets = json_object_get(jsonData, "num_udp_packets");
	
	int numPackets = atoi(json_string_value(num_udp_packets));

	//sending low entropy packets
	char* lowEntropyData;
	for(int i = 0; i < numPackets; i++){
		lowEntropyData = calloc(atoi(json_string_value(udp_payload_size)) - 40, sizeof(char));
		unsigned short int id = i;

		//assigs a unique id to a packet
		lowEntropyData[0] = (id & 0xFF00) >> 8;
		lowEntropyData[1] = id & 0x00FF;
		memset(lowEntropyData + 2, 0, atoi(json_string_value(udp_payload_size)) - 42);
		if (sendto(probing_client_fd, lowEntropyData, atoi(json_string_value(udp_payload_size)) - 42, 0, (struct sockaddr*)&probing_server_addr, sizeof(probing_server_addr)) < 0) {
	       	perror("sending error");
	       	close(probing_client_fd);
	       	free(lowEntropyData);
	       	return -1;
		}
		free(lowEntropyData);
	}

	//intermittent time
	json_t *inter_meas_time = json_object_get(jsonData, "inter_meas_time");
	sleep(atoi(json_string_value(inter_meas_time)));

	//high entropy probing starts
	FILE* random_file = fopen("random_file", "r");
	char* highEntropyData = malloc(atoi(json_string_value(udp_payload_size)) - 40);
	fgets(highEntropyData, atoi(json_string_value(udp_payload_size)) - 40, random_file);
	for(int i = 0; i < numPackets; i++){
		unsigned short int id = i;

		highEntropyData[0] = (id & 0xFF00) >> 8;
		highEntropyData[1] = id & 0x00FF;
		
		if (sendto(probing_client_fd, highEntropyData, atoi(json_string_value(udp_payload_size)) - 42, 0, (struct sockaddr*)&probing_server_addr, sizeof(probing_server_addr)) < 0) {
	       	perror("high entropy sending error");
	       	close(probing_client_fd);
	       	free(highEntropyData);
	       	return -1;
		}
	}
	free(highEntropyData);
	close(probing_client_fd);

	//post probing phase begins
	struct sockaddr_in post_serv_addr;

	//create socket
	int post_sock_fd =  socket(PF_INET, SOCK_STREAM, 0);
	if (post_sock_fd < 0){
		printf("Error: Could not create socket\n");
		return 1;
	}

	//set up address
	json_t *tcp_post_port = json_object_get(jsonData, "tcp_post_port");

	post_serv_addr.sin_family = AF_INET;
	post_serv_addr.sin_port = htons(atoi(json_string_value(tcp_post_port)));
	if (inet_pton(AF_INET, json_string_value(serv_ip), &post_serv_addr.sin_addr) <= 0){
		printf("invalid address\n");
		close(post_sock_fd);
		return -1;		
	}	

	//connect
	do{
		sleep(1);
		status = connect(post_sock_fd, (struct sockaddr *)&post_serv_addr, sizeof(post_serv_addr));
	}while(status < 0);

	//receive final result from server
	char result[64] = { 0 };
	if (read(post_sock_fd, result, 64 - 1) < 0){
		perror("coudn't read");
		close(post_sock_fd);
		return -1;
	}

	//print result to console
	printf("%s\n", result);

	close(post_sock_fd);
	json_decref(jsonData);
	return 0;

}
