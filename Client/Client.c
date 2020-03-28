#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>


#include <sys/socket.h> 
#include <arpa/inet.h> 

#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define READ_CHUNK 1 
#define BATCH_SIZE 256

#define KNRM  "\x1B[00;1;"
#define KRED  "\x1B[31;1;"
#define KGRN  "\x1B[32;1;"
#define KYEL  "\x1B[33;1;"
#define KBLU  "\x1B[34;1;"
#define KMAG  "\x1B[35;1;"
#define KCYN  "\x1B[36;1;"
#define KWHT  "\x1B[37;1;"

#define BNRM  "40m"
#define BRED  "41m"
#define BGRN  "42m"
#define BYEL  "43m"
#define BBLU  "44m"
#define BMAG  "45m"
#define BCYN  "46m"
#define BWHT  "47m"

#define CLRS  "\033[H\033[J\n"

extern int errno ;


char ip[25];
int port;
int w, h;

typedef struct file{
	char *name;
	char type;
} sfl;



typedef struct panel{
	sfl ** sf;
	int len; 
} pan;


void * allocated_buffer_read(int socket, void *buf, int *size){

  *size = 0;
  int bytes_read;
  do{
    buf = (void *)realloc(buf, *size + READ_CHUNK);
    bytes_read = read(socket, buf + *size, READ_CHUNK);
    *size += bytes_read;
  } while (bytes_read == READ_CHUNK);

  buf = (void *)realloc(buf, *size);
  return buf;
}



int make_connection(){
    int sock = 0; 
    struct sockaddr_in serv_addr; 
    

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        exit(-1);
    } 
   

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        exit(-1);
    } 
   

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
       	exit(-1);
    } 

    return sock;
}

pan *unpack(void *data){

	sfl ** files = NULL;

	int file_nr =  *(int *)data;
	int offset = 4;

	files = (sfl **)realloc(files, file_nr * sizeof(sfl *));

	int i = 0;

	for (;i<file_nr;i++){

		files[i] = (sfl *)malloc(sizeof(sfl));
		files[i]->type =  *(char *)(data + offset);

		offset += 1;
		uint16_t len =  *(uint16_t *)(data + offset);
		// printf("%d ", len);
		files[i]->name = malloc(len + 1);
		offset += 2;
		strncpy(files[i]->name, (char *)(data) + offset, len);
		files[i]->name[len] = 0;
		// puts(files[i]->name);
		offset += len;
	}

	pan *pannel = (pan *)malloc(sizeof(pan));

	pannel->sf = files;
	pannel->len = file_nr;

	free(data);
	return pannel;
}



void progress_print(int progress){


	puts(CLRS);
	puts(KRED);
	puts(BNRM);
	printf("%d\n", progress);
	char pbar[104];
	strcpy(pbar, "||");
	memset(pbar + 2, '#', progress);
	memset(pbar + 2 + progress, ' ', 100 - progress);
	strcpy(pbar + 102, "||");
	puts(pbar);


}

int read_socket_write_to_file(int socket, int block_size, char *file, int total_blocks){

  FILE *fp = fopen(file,"wb");
  int blocks = 0;
  void *block = malloc(block_size);
  int bytes_read;
  int old_prc = -1, new_prc = -1;
  while((bytes_read = read(socket, block, block_size)) > 0) {

  	 new_prc = (blocks * 100) / total_blocks;
  	 if (new_prc != old_prc)
  	 	progress_print(new_prc);

  	 old_prc = new_prc;
     fwrite(block, 1, bytes_read, fp);
     blocks++;

  }

  return blocks;
}

char *get_local_file_path(char *remote_file, char *local_dir) {
    char *filename = strrchr(remote_file,'/');

    if (!filename) {
        return NULL;
    }
    filename += 1;
    char *local_file_path = malloc(strlen(local_dir)+strlen(filename) + 1);
    strcpy(local_file_path, local_dir);
    strcat(local_file_path, "/");
    strcat(local_file_path, filename);
    
    return local_file_path;
}

int download(char *remote_file, char *local_dir){
    
    int socket = make_connection();
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
        
    char* local_file_path = get_local_file_path(remote_file, local_dir);

    if (!local_file_path)
        return -1;

    int written_blocks = 0;

    written_blocks = read_socket_write_to_file(socket, block_size, local_file_path, blocks);
    printf("%d", written_blocks);
    if (written_blocks != blocks) {
        return -2;
    }
    
    return 0;
}
int upload(char *file_path, char *right_dir_path){
    int socket = make_connection();

    int* file_size;
    int* chunk_size;
    FILE* fp;
    void *cmd;
    char c='u';
    char blocks_c[4];
    int blocks=0;
    void *file_name;
    void *right_file_path;
    
    file_name=strrchr(file_path,'/');
    printf("file name=%s, right_dir path=%s\n",(char *)file_name,right_dir_path);

    right_file_path=(void *)malloc(strlen(right_dir_path)+ strlen((char *)file_name)+1);

    memcpy(right_file_path, right_dir_path, strlen(right_dir_path)+1);
    memcpy(right_file_path+strlen(right_dir_path), file_name, strlen((char *)file_name)+1);

    printf("right file name=%s\n",(char *)right_file_path);

    file_size=(int *)malloc(1);
    chunk_size=(int *)malloc(1);
    *chunk_size=4096;

    cmd=(void *)malloc(1);
    memcpy(cmd,(void *)&c,1);

    fp=fopen(file_path, "rb");
    fseek(fp, 0L, SEEK_END);
    *file_size=ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    cmd=(void *)realloc(cmd,5);
    memcpy(cmd+1, (void *)file_size, 4);

    cmd=(void *)realloc(cmd,9);
    memcpy(cmd+5, (void *)chunk_size, 4);

    cmd=(void *)realloc(cmd,9+strlen(right_dir_path)+ strlen((char *)file_name)+1); 
    memcpy(cmd+9, (void *)right_file_path, strlen((char *)right_file_path)+1);
 
    printf("file size=%d, block size=%d\n",*(int *)(cmd+1),*(int *)(cmd+5));

    send(socket , cmd , 9 + strlen(file_path) + 1 , 0 ); 
    read(socket, blocks_c, 4);
    blocks=*(int *)blocks_c;

    printf("received from server no of blocks=%d\n",blocks);
    
    void *buff;
    int bytes_read=0;


    int i = 0;
    buff=(void *)malloc(*chunk_size);
    int old_percent = -1, new_percent = -1;
    for(i=0; i<blocks; i++)
    {
    new_percent = ( i * 100 )/ blocks;

    if (new_percent != old_percent)
    	progress_print(new_percent);

    old_percent = new_percent;	

	bytes_read=fread(buff, 1, *chunk_size, fp);
	send(socket, buff, bytes_read, 0);
    }
    fclose(fp);
    close(socket);
    return 0;
}


int sv_remove(char *file){
	int socket = make_connection();
    
    char cmd[257] = "r";

    strcat(cmd, file);

    send(socket , cmd , strlen(cmd) , 0 ); 
    read(socket, cmd, READ_CHUNK);
    close(socket);
	return 0;

}

int local_remove(char *file){
	return remove(file);
}




pan *server_list_dir(char * dir){

	int socket = make_connection();
    
    char cmd[257] = "f";

    strcat(cmd, dir);

    send(socket , cmd , strlen(cmd) , 0 ); 

   
    void *buffer = NULL; 
    int sz;
    buffer = allocated_buffer_read( socket , buffer, &sz); 

    close(socket);

    return unpack(buffer);
}

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

	curent += 1;	
	files[curent] = NULL;

	files = (sfl **)realloc(files, (curent + 1)* sizeof(sfl *));

	pan *pannel = (pan *)malloc(sizeof(pan));

	pannel->sf = files;
	pannel->len = curent -1;

	return pannel;
}


void smartprint(int w, int h, pan * right, pan * left, int selected_left, int selected_right, int selected_panel){



	int half = w / 2;
	w = 20 + w;

	h -= 1;

	char matrix[w * h];
	
	memset(matrix, ' ', w * h);
	// matrix[ (h - 1) * w + w -1 ] = 'a';

	int i = 0;
	

	for (i = 0; i < h; i++)
		{
			strncpy(matrix + i * w, KNRM, 7);
			strncpy(matrix + i * w + 7, BNRM, 3);
			strncpy(matrix + i * w + half + 10, KNRM, 7);
			strncpy(matrix + i * w + half + 17, BNRM, 3);
			matrix[i * w + half + 20] = '|';
		}



	int maxi, offset;


	maxi = left->len < (h - 1)  ? left->len : (h - 1);
	offset = selected_left - maxi + 1> 0 ? selected_left - maxi + 1 : 0;


	for (i = 0;i < maxi; i++){
		if (left->sf[i + offset]->type == 15)
			strncpy(matrix + i * w, KRED, 7);
		else if (left->sf[i + offset]->type == 4)
			strncpy(matrix + i * w, KNRM, 7);
		else 
			strncpy(matrix + i * w, KBLU, 7);

		if (selected_panel == 0 && selected_left == i + offset)
			strncpy(matrix + i * w + 7, BGRN, 3);
		else
			strncpy(matrix + i * w + 7, BNRM, 3);


		int namelen = strlen(left->sf[i + offset]->name);
		strncpy(matrix + i * w + 10, left->sf[i + offset]->name, half < namelen  ? half : namelen );
	}


	maxi = right->len < (h - 1) ? right->len : (h - 1);
	offset = selected_right - maxi  + 1 > 0 ? selected_right - maxi +1	 : 0;


	for (i = 0;i < maxi; i++){
		if (right->sf[i + offset]->type == 15)
			strncpy(matrix + i * w + half + 10, KRED, 7);
		else if (right->sf[i + offset]->type == 4)
			strncpy(matrix + i * w + half + 10, KNRM, 7);
		else 
			strncpy(matrix + i * w + half + 10, KBLU, 7);


		if (selected_panel == 1 && selected_right == i + offset)
			strncpy(matrix + i * w + half + 17, BGRN, 3);
		else
			strncpy(matrix + i * w + half + 17, BNRM, 3);



		int namelen = strlen(right->sf[i + offset]->name);
		strncpy(matrix + i * w + 22 + half, right->sf[i + offset]->name, half < namelen  ? half : namelen );
	}

	puts(matrix);
}




static int compare(const void *p1, const void *p2){
	sfl * a = *(sfl **)p1;
	sfl * b = *(sfl **)p2;

	if (a->type > b->type) return 1;
	if (b->type > a->type) return -1;

	return strcmp(a->name, b->name);
}

int get_key(){
	int c;
		c = getchar();

		if (c == 27){
			c = getchar();
			if ( c == 91){
				c = getchar();
					return c - 65;
			}
		}

		return c;

}


static int stream_makeraw(FILE *const stream){

	struct termios  raw, actual;
    int             fd;

    if (!stream)
        return -1;

    if (setvbuf(stream, NULL, _IONBF, 0))
        return -1;

    fflush(stream);

    
    fd = fileno(stream);
    if (fd)
        return fd;

   
    tcflush(fd, TCIOFLUSH);


    if (tcgetattr(fd, &raw))
        return -1;
    
    raw.c_cflag |= CREAD;    
    raw.c_lflag &= ~ECHO;
    raw.c_lflag &= ~ICANON;
    raw.c_cc[VMIN] = 1;  
    raw.c_cc[VTIME] = 0; 

    if (tcsetattr(fd, TCSAFLUSH, &raw))
        return -1;


    if (tcgetattr(fd, &actual)) {
		return -1;
    }

    if (actual.c_iflag != raw.c_iflag ||
        actual.c_oflag != raw.c_oflag ||
        actual.c_cflag != raw.c_cflag ||
        actual.c_lflag != raw.c_lflag) {
		return -1;
	}

    return 0;
}



void fr(pan *panel){


	// fprintf(fp, "i will free %d elements\n", panel->len);
	int i;
	for (i = 0; i < panel->len; i++)
	{
		// fprintf(fp, "i will free %d  :", i );
		// fprintf(fp, "%s\n", panel->sf[i]->name); 
		free(panel->sf[i]->name);
		free(panel->sf[i]);
	}

	// fprintf(fp, "Orice dar inainte\n" );
	free(panel->sf);

	// fprintf(fp, "Orice\n" );
	// fclose(fp);

}



int iterface(void){
	stream_makeraw(stdin);

	char left_dir[256];
	char right_dir[256];

	getcwd(left_dir, 256);
	getcwd(right_dir, 256);

	pan *right_panel = local_list_dir(right_dir);
	pan *left_panel = server_list_dir(left_dir);

	qsort(right_panel->sf, right_panel->len, sizeof(sfl *), compare);
	qsort(left_panel->sf, left_panel->len, sizeof(sfl *), compare);
	

	int selected_left = 0, selected_right = 0;

	int selected_panel = 0;

	struct winsize w;	


	while (1){
	
		// printf(CLRS);
		// puts(right_dir);
		
		int result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if (result)
			return result;
		if (w.ws_row < 25 || w.ws_col < 100){
			puts(KRED "The client need a windows size of at least 25 by 100. Please resize your windows");
			while (w.ws_row < 25 || w.ws_col < 100){
				int result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
				if (result)
					return result;
			}
		}
		


		int width = w.ws_col;
		int height = w.ws_row;


		smartprint(width, height, right_panel, left_panel, selected_left, selected_right, selected_panel);


		int key = get_key();

		switch (key){
			case 1:
				if (selected_panel)
					selected_right += 1;
				else
					selected_left += 1;
				break;
			case 0:
				if (selected_panel)
					selected_right -= 1;
				else
					selected_left -= 1;
				break;
			case 9:
				if (selected_panel)
					selected_panel = 0;
				else
					selected_panel = 1;
				break;
			case 'c':
				if (selected_panel && right_panel->sf[selected_right]->type != 4) //sa fie fisier nu folder
					{
						char full_path[270];
						strcpy(full_path, right_dir);
						strcat(full_path, "/");
						strcat(full_path, right_panel->sf[selected_right]->name);
						download(full_path, left_dir);

						left_panel = local_list_dir(left_dir);
						qsort(left_panel->sf, left_panel->len, sizeof(sfl *), compare);

					}
				else
				{
					if (left_panel->sf[selected_left]->type == 4)
						break;

					char full_path[270];
					strcpy(full_path, left_dir);
					strcat(full_path, "/");
					strcat(full_path, left_panel->sf[selected_left]->name);
					upload(full_path, right_dir);

					right_panel = server_list_dir(right_dir);
					qsort(right_panel->sf, right_panel->len, sizeof(sfl *), compare);
					smartprint(width, height, right_panel, left_panel, selected_left, selected_right, selected_panel);
				}
				break;
			case 'r':
				if (selected_panel && right_panel->sf[selected_right]->type != 4)
				{
					char full_path[270];
					strcpy(full_path, right_dir);
					strcat(full_path, "/");
					strcat(full_path, right_panel->sf[selected_right]->name);
					sv_remove(full_path);

					right_panel = server_list_dir(right_dir);
					qsort(right_panel->sf, right_panel->len, sizeof(sfl *), compare);
				}
				else
				{
					if (left_panel->sf[selected_left]->type == 4)
						break;
					char full_path[270];
					strcpy(full_path, left_dir);
					strcat(full_path, "/");
					strcat(full_path, left_panel->sf[selected_left]->name);
					local_remove(full_path);
					left_panel = local_list_dir(left_dir);
					qsort(left_panel->sf, left_panel->len, sizeof(sfl *), compare);
				}
				break;
			case '\n':
				if (selected_panel){
					if (right_panel->sf[selected_right]->type == 4 && strcmp(right_panel->sf[selected_right]->name, ".")){


						if (strcmp(right_panel->sf[selected_right]->name, "..") != 0)
						{
							strcat(right_dir, "/");
							strcat(right_dir, right_panel->sf[selected_right]->name);
							// fr(right_panel);
							selected_right = 0;
							right_panel = server_list_dir(right_dir);
							qsort(right_panel->sf, right_panel->len, sizeof(sfl *), compare);
						}
						else
						{
							strcpy(strrchr(right_dir, '/'), "\0");
							if (strlen(right_dir) == 0)
								strcpy(right_dir, "/");
							
							// fr(right_panel);

							// puts(right_dir);
							selected_right = 0;
							right_panel = server_list_dir(right_dir);
							qsort(right_panel->sf, right_panel->len, sizeof(sfl *), compare);
							}
						}
					}
				else{

					if (left_panel->sf[selected_left]->type == 4  && strcmp(left_panel->sf[selected_left]->name, ".")){
						if (strcmp(left_panel->sf[selected_left]->name, "..") != 0){
							strcat(left_dir, "/");
							strcat(left_dir, left_panel->sf[selected_left]->name);
							// fr(left_panel);
							selected_left = 0;
							left_panel = local_list_dir(left_dir);
							qsort(left_panel->sf, left_panel->len, sizeof(sfl *), compare);
						}
						else{
							// fr(left_panel);
							selected_left = 0;
							strcpy(strrchr(left_dir, '/'), "\0");
							if (strlen(left_dir) == 0)
								strcpy(left_dir, "/");
							left_panel = local_list_dir(left_dir);
							qsort(left_panel->sf, left_panel->len, sizeof(sfl *), compare);
							}
						}
				}
		}


		if (selected_left < 0)
			selected_left = left_panel->len -1;
		if (selected_left > left_panel->len - 1)
			selected_left = 0;

		if (selected_right < 0)
			selected_right = right_panel->len - 1;
		if (selected_right > right_panel->len - 1)
			selected_right = 0;
		

		}


		return 0;

}




// int main() 
// { 
  

//     char *hello = "f.."; 
//     int sock = make_connection();
//     send(sock , hello , strlen(hello) , 0 ); 

   
//     void *buffer = NULL; 
//     int sz;
//     buffer = allocated_buffer_read( sock , buffer, &sz); 
//     printf("%d", *(int *)buffer);
//     printf("%d\n", sz);

//     unpack(buffer);
//     return 0; 
// } 



int main(int argc, char* argv[]){

	  if(argc != 3){
      printf("Please call: %s <ip> <port>\n", argv[0]);
      exit(1);
    }

    strcpy(ip, argv[1]);
	port = atoi(argv[2]);

	return iterface();

	// printf("%d  %d\n", strlen(KNRM), strlen(BNRM));

	// stream_makeraw(stdin);


	// pan d = local_list_dir("");
	// int i = 0;
	// for (; i < d.len; i++ )
	// 	printf("%d %s\n", d.sf[i]->type, d.sf[i]->name);
}
