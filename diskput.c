/*CSC360 A03 SFS: Part IV: diskput
 *Student: Xiaole (Wendy) Tan
 *Vnumber: V00868734*/

//INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

//DEFINE & GLOBAL
#define SEC_SIZE 512  //sector size
int SEC_COUNT;        //# of sector in the disk
int file_system;      //open ima file
char *f_map;          //mmap disk
int system_size;      //ima size
int put_file;         //open put file
char *p_map;          //mmap put file
int put_size;         //put file size
char f_name [15];     //put file name

//HELPER FUNTIONS
//FUNCTION: read file name from input
void get_filename(char * input){
  char path_cpy [strlen(input)];
  char * tok;
  char fname [15];
  strcpy(path_cpy, input);
  tok = strtok(path_cpy, "/");
  while(tok != NULL){
    strcpy(fname, tok);
    tok = strtok(NULL, "/");
  }
  //lask token is always the file name
  strcpy(f_name, fname);
}

//FUNCTION: look the fat entry
int FAT_packing(int entry, char* disk_map){
  disk_map += SEC_SIZE;
  int next_sector;
  int low_high; //lower four for even; higher four for odd
  int eight;    //all eight bits
  int fun = (int)(3 * entry/2);

  //FAT start from 0x200 = 512 after first sector
  //if entry is even
  if(entry % 2 == 0){
    low_high = disk_map[1 + fun] & 0x0F; //low bit
    eight = disk_map[fun] & 0xFF;
    next_sector = (low_high << 8) + eight;
  //if entry is odd
  }else{
    low_high = disk_map[fun] & 0xF0; //high bit
    eight = disk_map[1 + fun] & 0xFF;
    next_sector = (low_high >> 4) + (eight << 4);
  }
  return next_sector;
}

//FUNCTION: read disk size
int read_disksize(char *file){
  //byte 19: total sector count (2 bytes)
  SEC_COUNT = file[19] + (file[20] << 8); //little endian
  int total_size = SEC_COUNT * SEC_SIZE;
  return total_size;
}

//FUNCTION: read free size of the disk
int read_freesize(char *file){
  int empty_sector = 0;
  int i;
  //first 2 entry are reserved, ignore useless entries after 2848
  for(i=2; i<2848; i++){
    //if unused
    if(FAT_packing(i, file) == 0x00){
      empty_sector ++;
    }
  }
  int free = empty_sector * SEC_SIZE;
  return free;
}

//FUNTION: find if subdirectory exits in given directory;
char * find_sub(char * sub_name, char * disk_map){
  //find subdirectory in upper case
  int s;
  for(s=0; s<strlen(sub_name); s++){
    sub_name[s] = toupper(sub_name[s]);
  }

  //find file
  int i;
  char fname [8];
  while(disk_map[0] != 0x00){
    //read file name
    for(i=0; i<8; i++){
      if(disk_map[i] == ' '){
        fname[i] = '\0';
        break;
      }
      fname[i] = disk_map[i];
    }

    //check if it is subdirectory
    if(strncmp(sub_name, fname, strlen(sub_name)) == 0 && (disk_map[11] & 0x10) == 0x10){
      int logical_cluster = (disk_map[26] & 0xFF) + (disk_map[27] << 8);
      int sub_location = SEC_SIZE * (logical_cluster + 31) + 64; //move to sub sector and skip entry . and ..
      disk_map = f_map + sub_location; //record sub location
      return disk_map;
    }
    disk_map = disk_map + 32;
  }
  //sub not found
  printf("The directory not found.\n");
  close(put_file);
  munmap(p_map, put_size);
  close(file_system);
  munmap(f_map, system_size);
  exit(1);
}

//FUNTION: read file path given, return the directory to put new file
char * where_to_put(char * path){
  char path_cpy [strlen(path)];
  char * tok;
  char * location = f_map + SEC_SIZE * 19;
  //make a copy of path for tokenization
  strncpy(path_cpy, path, strlen(path));
  tok = strtok(path, "/");
  while(tok != NULL){
    //if sub_name match retrun location
    if(strncmp(f_name, tok, strlen(f_name)) == 0){
      return location;
    }else{
      //find subdirectory
      location = find_sub(tok, location);
      tok = strtok(NULL, "/");
    }
  }
  return location;
}

//FUNTION: find the next empty fat index
int find_freeFAT(){
  char * fat = f_map;
  int n;
  for(n=2; n<2849; n++){
    if(FAT_packing(n, fat) == 0x00){
      return n;
    }
  }
  return 0;
}

//FUNTION: update FAT entry
void update_FAT(int fat_n, int set_value){
  char * ptr = f_map + SEC_SIZE;
  int fun = (int)(3 * fat_n / 2);
  //fat_n is even
  if(fat_n % 2 == 0){
    ptr[fun + 1] = (set_value >> 8) & 0x0F; //lower four bits
    ptr[fun] = set_value & 0xFF;
  //fat_n is odd
  }else{
    ptr[fun] = (set_value << 4) & 0xF0; //higher four bits
    ptr[fun + 1] = (set_value >> 4) & 0xFF;
  }
}

//FUNTION: update directory info
void update_directory(char * put_location, int fat_n){
  //find free address in directory
  char * location = put_location;
  while(location[0] != 0x00){
    location += 32;
  }

  //update filename field
  int i;
  int ex_start = -1;
  int done = 0;
  for(i=0; i<9; i++){
    char c = toupper(f_name[i]);
    if(f_name[i] == '.'){
      done = 1;
      ex_start = i;
    }
    if(done != 1){
      location[i] = c;
    }else{
      location[i] = ' ';
    }
  }
  //update file extension
  done  = 0;
  ex_start++; //skip .
  for(i=0; i<3; i++){
    char e = toupper(f_name[ex_start + i]);
    if(e == '\0'){
      done = 1;
    }
    if(done != 1){
      location[i+8] = e;
    }else{
      location[i+8] = ' ';
    }
  }
  //update attribute field
  location[11] = 0x00;

  //update creation date
  for(i=12; i<18; i++){
    location[i] = 0;
  }
  time_t t = time(NULL);
  struct tm * curr_t = localtime(&t);
  int y = curr_t->tm_year + 1900;
  int m = curr_t->tm_mon + 1;
  int d = curr_t->tm_mday;
  int h = curr_t->tm_hour;
  int min = curr_t->tm_min;
  //16/17 store creation date (in little endian)
  location[17] |= (y - 1980) << 0x01;
  location[17] |= (m - ((location[16] & 0xE0) >> 5)) >> 3;
  location[16] |= (m - ((location[17] & 0x01) << 3))<< 5;
  location[16] |= (d & 0x1F);
  //14/15 store creation time (in little endian)
  location[15] |= (h << 3) & 0xF8;
  location[15] |= (min - ((location[14] & 0xE0) >> 5)) >> 3;
  location[14] |= (min - ((location[15] & 0x07) << 3)) << 5;

  //update first logical cluster
  location[26] = (fat_n - (location[27] << 8)) & 0xFF;
  location[27] = (fat_n - location[26]) >> 8;

  //update file size
  location[28] = (put_size & 0x000000FF);
  location[29] = (put_size & 0x0000FF00) >> 8;
  location[30] = (put_size & 0x00FF0000) >> 16;
  location[31] = (put_size & 0xFF000000) >> 24;
}

//FUNTION: put file in disk
void put_file_indisk(char* location){
  int rem_size = put_size;
  int fat_n = find_freeFAT();
  update_directory(location, fat_n);
  int i;

  //put content in disk
  while(rem_size > 0){
    int p_addr = (fat_n + 31) * SEC_SIZE;
    for(i=0; i<SEC_SIZE; i++){
      if(rem_size == 0){
        update_FAT(fat_n, 0xFFF); //set last
        return;
      }
      f_map[i + p_addr] = p_map[put_size - rem_size];
      rem_size--;
    }
    //update and get free fat entry
    update_FAT(fat_n, 0x69);
    int new_freeFAT = find_freeFAT();
    update_FAT(fat_n, new_freeFAT);
    fat_n = new_freeFAT;
  }
}

//MAIN
int main(int argc, char *argv[]){
  //catch wrong input format
  if(argc < 3){
    printf("ERROR: Invalid arugument. Enter: diskput <file system image> <file to put>.\n");
    exit(1);
  }

  //open read file system
  file_system = open(argv[1], O_RDWR);
  if(file_system < 0){
    printf("ERROR: Failed to open file system image.\n");
    exit(1);
  }

  //map disk to memory
  struct stat f_info;
  fstat(file_system, &f_info);
  system_size = f_info.st_size;
  f_map = mmap(NULL, f_info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_system, 0);

  //open given file
  get_filename(argv[2]);
  put_file = open(f_name, O_RDWR);
  if(put_file < 0){
    printf("File not found.\n");
    close(file_system);
    munmap(f_map, f_info.st_size);
    exit(1);
  }

  //map put_file to memory
  struct stat put_info;
  fstat(put_file, &put_info);
  put_size = put_info.st_size;
  p_map = mmap(NULL, put_info.st_size, PROT_READ, MAP_SHARED, put_file, 0);

  //check the path to put the file (subdirectory or root directory)
  char * put_location = where_to_put(argv[2]);

  //check if disk has enough space to store file
  int disk_size = read_disksize(f_map);
  int free_size = read_freesize(f_map);
  //if not enough free space to store the file
  if(free_size < put_size){
    printf("No enough free space in the disk image.\n");
    close(put_file);
    munmap(p_map, put_info.st_size);
    close(file_system);
    munmap(f_map, f_info.st_size);
    exit(1);
  }

  //go through FAT entries to find unsed sectors and copy content
  put_file_indisk(put_location);

  //free memory/ finish
  close(put_file);
  munmap(p_map, put_size);
  close(file_system);
  munmap(f_map, system_size);
  return 0;
}
