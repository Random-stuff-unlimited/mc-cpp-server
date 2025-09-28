#include "protocol.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "struct.h"
#include "queue.h"

int read_varint(int sock)
{
	int value = 0, position = 0;
	unsigned char byte;
	while (1) {
		if (read(sock, &byte, 1) <= 0) return -1;
		value |= (byte & 0x7F) << position;
		if (!(byte & 0x80)) break;
		position += 7;
		if (position >= 32) return -1;
	}
	return value;
}

void write_varint(int sock, int value)
{
	while (1) {
		if ((value & ~0x7F) == 0) {
			unsigned char b = value;
			write(sock, &b, 1);
			return;
		}
		unsigned char b = (value & 0x7F) | 0x80;
		write(sock, &b, 1);
		value >>= 7;
	}
}

static int read_varint_from_buffer(char** buffer) {
	int value = 0;
	int position = 0;
	unsigned char current_byte;
	char* ptr = *buffer;
	while (1) {
		current_byte = *ptr;
		ptr++;
		value |= (current_byte & 0x7F) << position;
		if ((current_byte & 0x80) == 0) break;
		position += 7;
	}
	*buffer = ptr;
	return value;
}

static void read_string_from_buffer(char** buffer, char* dest, int max_len) {
	int len = read_varint_from_buffer(buffer);
	if (len > max_len - 1) len = max_len - 1;
	memcpy(dest, *buffer, len);
	dest[len] = '\0';
	*buffer += len;
}

static int varint_len(int value) {
	int len = 0;
	do {
		len++;
		value >>= 7;
	} while (value != 0);
	return len;
}

int handle_handshake(int client_sock)
{
    printf("Handshake process...\n");
    char packet_buffer[2048];

    int handshake_len = read_varint(client_sock);

    if (handshake_len <= 0 || handshake_len > (int)sizeof(packet_buffer))
	{
        fprintf(stderr, "Error: Invalid handshake length or client disconnected (%d bytes).\n", handshake_len);
        return (-1);
    }

    int bytes_read = 0;
    while (bytes_read < handshake_len)
	{
        int result = read(client_sock, packet_buffer + bytes_read, handshake_len - bytes_read);
        if (result <= 0)
		{
            perror("read handshake data");
            fprintf(stderr, "Error: Unable to read handshake data.\n");
            return (-1);
        }
        bytes_read += result;
    }
    printf("Handshake received (length: %d)\n", handshake_len);

    char* buffer_ptr = packet_buffer;
    
    int packet_id = read_varint_from_buffer(&buffer_ptr);
    if (packet_id != 0x00)
	{
        fprintf(stderr, "Error: Unexpected handshake packet ID: 0x%02X\n", packet_id);
        return (-1);
    }

    int protocol_version = read_varint_from_buffer(&buffer_ptr);
    
    char server_addr[256];
    read_string_from_buffer(&buffer_ptr, server_addr, sizeof(server_addr));
    
    buffer_ptr += 2; 

    int next_state = read_varint_from_buffer(&buffer_ptr);

    printf("Handshake analyzed: Protocol Version=%d, Address='%s', Requested State=%d\n", 
           protocol_version, server_addr, next_state);

    if (next_state == 1 || next_state == 2)
	{
        return (next_state);
    }

    fprintf(stderr, "Error: Unknown next state requested: %d\n", next_state);
    return (-1);
}

void handle_status_request(int client_sock)
{
    char packet_buffer[2048];

    int request_len = read_varint(client_sock);
    if (request_len < 0)
		return;
    read(client_sock, packet_buffer, request_len);

    const char *json_response =
        "{"
        "\"version\":{\"name\":\"1.21.5\",\"protocol\":770},"
        "\"players\":{\"max\":42,\"online\":0,\"sample\":[]},"
        "\"description\":{\"text\":\"§aServeur Minecraft en C!\"}"
        "}";
    
    int json_len = strlen(json_response);
    int total_len = 1 + varint_len(json_len) + json_len;
    
    write_varint(client_sock, total_len);
    write_varint(client_sock, 0x00);
    write_varint(client_sock, json_len);
    write(client_sock, json_response, json_len);
    printf("Response JSON sent.\n");


    int total_ping_len = read_varint(client_sock);
    if (total_ping_len <= 0)
	{
        printf("The client did not send a ping packet.\n");
        return;
    }

    int ping_packet_id = read_varint(client_sock);
    if (ping_packet_id != 0x01)
	{
        fprintf(stderr, "Error: Unexpected packet ID after status (expected 0x01, received 0x%02X)\n", ping_packet_id);
        return;
    }

    int payload_len = total_ping_len - varint_len(ping_packet_id);
    if (payload_len != 8) {
        fprintf(stderr, "Error: Ping payload size is incorrect (%d bytes instead of 8)\n", payload_len);
        return;
    }
    
    char payload_buffer[8];
    if (read(client_sock, payload_buffer, sizeof(payload_buffer)) != sizeof(payload_buffer))
	{
        fprintf(stderr, "Error reading ping payload.\n");
        return;
    }
    
    printf("Ping received, Pong sent...\n");
    write_varint(client_sock, total_ping_len);
    write_varint(client_sock, 0x01);
    write(client_sock, payload_buffer, sizeof(payload_buffer));
    printf("Pong sent successfully.\n");
}

void send_disconnect_message(int client_sock, const char *reason_text)
{
    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload), "{\"text\":\"%s\",\"color\":\"red\"}", reason_text);
    
    int json_len = strlen(json_payload);
	int reason_part_len = varint_len(json_len) + json_len;
    int total_packet_len = varint_len(0x00) + reason_part_len;

    write_varint(client_sock, total_packet_len);
    write_varint(client_sock, 0x00);
    write_varint(client_sock, json_len);
    write(client_sock, json_payload, json_len);
    
    printf("Disconnect packet sent for cause: '%s'\n", reason_text);
}

void add_player(t_server *server, int client_sock, const char* player_name)
{
    pthread_mutex_lock(&server->player_lock);

    for (unsigned int i = 0; i < server->player_max; i++)
	{
        if (!server->players_lst[i].connected) {
            t_player *p = &server->players_lst[i];
            p->socket_fd = client_sock;
            p->connected = 1;
            strncpy(p->username, player_name, sizeof(p->username) - 1);
            p->username[sizeof(p->username) - 1] = '\0'; 
            p->x = p->y = p->z = 0;
            p->health = 20;

            printf("Player '%s' added to the server on socket %d.\n", p->username, p->socket_fd);
            pthread_mutex_unlock(&server->player_lock);
            return;
        }
    }
    pthread_mutex_unlock(&server->player_lock);
    printf("The server is full. Connection for '%s' refused.\n", player_name);
	send_disconnect_message(client_sock, "Server Full");
    close(client_sock);
}

int handle_login_start(int client_sock, t_server *server)
{
    printf("Processing login attempt...\n");
    char packet_buffer[2048]; 

    int login_packet_len = read_varint(client_sock);
    if (login_packet_len <= 0 || login_packet_len > (int)sizeof(packet_buffer)) {
        fprintf(stderr, "Error: Invalid login packet length.\n");
        return 1;
    }
    
    if (read(client_sock, packet_buffer, login_packet_len) != login_packet_len) {
        fprintf(stderr, "Error: Unable to read login packet.\n");
        return 1;
    }
    
    char* login_ptr = packet_buffer;
    int login_packet_id = read_varint_from_buffer(&login_ptr);

    if (login_packet_id != 0x00) {
        printf("Unexpected login packet ID: 0x%02X\n", login_packet_id);
        return 1;
    }

    char player_name[32] = {0};
    read_string_from_buffer(&login_ptr, player_name, sizeof(player_name));
    add_player(server, client_sock, player_name);
    return (0);
}
