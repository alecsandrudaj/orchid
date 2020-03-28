#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>

int read_socket_write_to_file(int socket, int block_size, char *file){

  FILE *fp = fopen(file,"wb");
  int blocks = 0;
  void *block = malloc(block_size);
  int bytes_read;
  while((bytes_read = read(socket, block, block_size)) > 0) {
     fwrite(block, 1, bytes_read, fp);
     blocks++;
  }

  return blocks;
}

char *get_local_file_path(char *remote_file, char *local_dir) {
    char *filename = strchr(remote_file,'/');

    if (!filename) {
        return NULL;
    }
    filename += 1;
    char *local_file_path = malloc(strlen(local_dir)+strlen(filename));
    strcpy(local_file_path, local_dir);
    strcat(local_file_path, filename);
    
    return local_file_path;
}

int download(int socket, char *remote_file, char *local_dir){
    
    char *request = malloc(strlen(remote_file)+1);
    request[0] = 'd';
    strcpy(request+1, remote_file);

    send(socket , request , strlen(request) , 0 ); 

    int file_size = 0;
    int block_size = 0;
    int blocks = 0;

    read(socket, &file_size, sizeof(int));
    read(socket, &block_size, sizeof(int));

    if (file_size%block_size == 0)
        blocks = file_size/block_size;
    else
        blocks = file_size/block_size + 1;

    send(socket, &blocks, sizeof(int), 0);
    
    printf("%d %d %d\n", file_size, block_size, blocks);
    
    char* local_file_path = get_local_file_path(remote_file, local_dir);

    if (!local_file_path)
        return -1;

    int written_blocks = 0;

    written_blocks = read_socket_write_to_file(socket, block_size, local_file_path);
    printf("%d", written_blocks);
    if (written_blocks != blocks) {
        return -2;
    }
    
    return 0;
}

int main(int argc, char const *argv[]) 
{ 
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char *hello = "f.."; 
    //void buffer[1024] = {0}; 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(8080); 
       
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    download(sock, "./md5.txt", "./");

   

    //valread = read( sock , buffer, 1024); 
    //printf("%d\n", valread);
    return 0; 
} 