#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#define PATH_MAX 4096 //chars in a path name including null
#define READ_CHUNK 1024 
#define BATCH_SIZE 128




extern int errno ;


typedef struct file{
  char *name;
  char type;
} sfl;


typedef struct panel{
  sfl ** sf;
  int len; 
} pan;


pan *local_list_dir(char * curent_director){

  DIR * d;
  struct dirent *dir;
  
  sfl ** files = NULL;

  int size = 0, curent = 0;

  d = opendir(curent_director);

  if (d){
    while ((dir = readdir(d)) != NULL){

      if (curent >= size){
        files = (sfl **)realloc(files, (size + BATCH_SIZE) * sizeof(sfl *));
        size += BATCH_SIZE;
      }

      files[curent] = (sfl *)malloc(sizeof(sfl));
      files[curent]->name = malloc(strlen(dir->d_name));
      strcpy(files[curent]->name, dir->d_name);
      files[curent]->type = dir->d_type;
      curent += 1;
    }
    closedir(d);
  }
  else{
    if (curent >= size){
        files = (sfl **)realloc(files, (size + BATCH_SIZE) * sizeof(sfl *));
        size += BATCH_SIZE;
      }

      files[curent] = (sfl *)malloc(sizeof(sfl));
      files[curent]->name = malloc(18);
      strcpy(files[curent]->name, "Permission Denied");
      files[curent]->type = 15;
      curent += 1;
      
      files[curent] = (sfl *)malloc(sizeof(sfl));
      files[curent]->name = malloc(3);
      strcpy(files[curent]->name, "..");
      files[curent]->type = 4;
      curent += 1;
  }

  files[curent] = NULL;

  files = (sfl **)realloc(files, (curent + 1)* sizeof(sfl *));

  pan *pannel = (pan *)malloc(sizeof(pan));

  pannel->sf = files;
  pannel->len = curent;

  return pannel;
}



char * allocated_buffer_read(int socket, char *buf, int *size){

  *size = 0;
  int bytes_read;
  do{
    buf = (char *)realloc(buf, *size + READ_CHUNK);
    bytes_read = read(socket, buf, READ_CHUNK);
    *size += bytes_read;
  } while (bytes_read == READ_CHUNK);

  buf = (char *)realloc(buf, *size);
  return buf;

}

void * pack(pan *files, int * d_len){

  void *s = NULL;
  int full_size = 4, offset = 4;
  int i = 0;

  s = (void *)realloc(s, 4);

  memcpy(s, (void *)&files->len, 4);

  for (; i < files->len; i++){
    uint16_t name_size = (uint16_t)strlen(files->sf[i]->name);
    s = (void *)realloc(s, full_size + name_size + 2);
    full_size = full_size + name_size + 2;

    memcpy(s + offset, (void *)&name_size, 2);
    offset += 2;
    memcpy(s + offset, (void *)files->sf[i]->name, (int)name_size);
    offset += name_size;
  }

  *d_len = full_size;
  return s;

}

void connection_handle(int socket){
  char *s = NULL;
  pan *files;
  int size;
  s = allocated_buffer_read(socket, s, &size);
  puts(s);

  switch (s[0]){
    case 'f':  //The client wants the list of files from the directory starting from s + 1
      files = local_list_dir(s+1);
      int d_len;
      void *res = pack(files, &d_len);

      printf("%d\n", d_len);
      int sent_bytes = send(socket, res, d_len, 0);
      printf("%d\n", sent_bytes);
      free(res);
      break;
  }
  
  free(s);
}


int main(int argc, char* argv[]){
  int sockfd, connfd;
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen = sizeof(rmt_addr);

  // if(argc < 3){
  //     printf("Please call: %s <directory> <port>\n", argv[0]);
  //     exit(1);
  //   }

  
  if(chdir(argv[1]) < 0){
      printf("CHDIR error");
    }

  
  //AF_INET - IPv4 protocol, SOCK_STREAM - TCP 
  sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd == -1){
    printf("Socket error\n");
  }

  int port = atoi(argv[2]);

  local_addr.sin_family = AF_INET; 
  local_addr.sin_addr.s_addr = INADDR_ANY; 
  local_addr.sin_port = htons( port ); 

  if( bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) == -1){
    printf("Bind error\n");
  }

  if(listen(sockfd,5) == -1){
    printf("Listen error\n");
  }
  puts("Listenting");

  while(1){
    //create a new socket for each connection

    connfd = accept(sockfd,(struct sockaddr *)&rmt_addr,  &rlen);

    if(connfd == -1){
      printf("Accept error\n");
      continue;
    }
    pid_t pid = fork();

    if(pid == 0){ //new process
      puts("New connection");

      connection_handle(connfd);
      return 0;
      
      }
    else if(pid == -1){//fork error
      printf("fork() error\n");
    }

    else{ //parrent process
      close(connfd);
    }
  }
  close(sockfd);
  exit(0);


  return 0;
}