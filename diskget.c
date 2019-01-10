/*CSC360 A03 SFS: Part III: diskget
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

//DEFINE & GLOBAL
#define SEC_SIZE 512
int name_length = 0;
int ex_length = 0;
char * f_map;
int found_filesize = 0;

//FUNCTIONS
//FUNCTION: lookup the fat entry
int FAT_packing(int entry, char* disk_map){
  //FAT start from 0x200 = 512 after first sector
  disk_map += SEC_SIZE;
  int next_sector;
  int low_high; //lower four for even; higher four for odd
  int eight;    //all eight bits
  int fun = (int)(3 * entry/2);

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


/* FUNTION: Given to be copied file name,
   search in disk and if exist return entry else file not found */
int search_for_file(char *file, char *copyfile_name, char *copyfile_ex){
  //starting at root directory, search for file
  if(file[0] == 0x00){
    return 0;//file not found
  }

  //record current entry file name
  int curname_length = 0;
  char curr_fname[10];
  int i;
  for(i=0; i<8; i++){
    if(file[i] == ' '){
      curr_fname[i] = '\0';
      break;
    }
    curr_fname[i] = file[i];
    curname_length++;
  }

  //record current entry file extension
  char curr_ex[3];
  int curex_length = 0;
  for(i=0; i<3; i++){
    if(file[i+8] == ' '){
      curr_ex[i] = '\0';
      break;
    }
    curr_ex[i] = file[i+8];
    curex_length++;
  }

  //record first logical sector
  int cluster_number = (file[26] & 0xFF) + (file[27] << 8);

  //if subdirectory
  if((file[11] & 0x10) == 0x10){
    int sub_location = SEC_SIZE * (cluster_number + 31) + 64; //move to sub sector and skip entry . and ..
    char *sub_dire = f_map - SEC_SIZE * 19 + sub_location; //record sub location
    file = file + 32;
    return search_for_file(file, copyfile_name, copyfile_ex) + search_for_file(sub_dire, copyfile_name, copyfile_ex);
  //if file found, return
  }else if(strncmp(curr_fname, copyfile_name, strlen(copyfile_name)) == 0 && strncmp(curr_ex, copyfile_ex, strlen(copyfile_ex)) == 0
          && curname_length == strlen(copyfile_name) && curex_length == strlen(copyfile_ex)){
    found_filesize = (file[28] & 0xFF) + ((file[29] & 0xFF) << 8) + ((file[30] & 0xFF) << 16) + ((file[31] & 0xFF) << 24);
    return cluster_number;
  }else{
    file = file + 32;
    return search_for_file(file, copyfile_name, copyfile_ex);
  }
}


//FUNTION: copy content of the founded file to the newly created file
void copy_to_newfile(char * disk_map, char * new_map, int first_sector){
  int n_sector = first_sector;
  int rem_size = found_filesize;
  int p_addr;
  int i;

  do{
    //first cluster
    if(rem_size == found_filesize){
      p_addr = (31 + n_sector) * SEC_SIZE;
    //find the next sector after first
    }else{
      n_sector = FAT_packing(n_sector, disk_map);
      p_addr = (31 + n_sector) * SEC_SIZE;
    }
    //copy content (sector by sector)
    for(i=0; i<SEC_SIZE; i++){
      if(rem_size == 0) return;
      new_map[found_filesize - rem_size] = disk_map[i + p_addr];
      rem_size--;
    }
  }while(FAT_packing(n_sector, disk_map) != 0xFFF); //last cluster
}


//MAIN
int main(int argc, char *argv[]){
  //catch wrong input format
  if(argc < 3){
    printf("ERROR: Invalid arugument. Enter: diskget <file system image> <file to be copyed>.\n");
    exit(1);
  }

  //open read file system
  int file_system = open(argv[1], O_RDWR);
  if(file_system < 0){
    printf("ERROR: Failed to open file system image.\n");
    exit(1);
  }

  //map to memory
  struct stat f_info;
  fstat(file_system, &f_info);
  f_map = mmap(NULL, f_info.st_size, PROT_READ, MAP_SHARED, file_system, 0);

  //convert file name to upper case
  char * copy_file = argv[2];
  char copy_filename [10];
  char copy_fileex[4];
  int name_or_ex = 0; //0=namepart/ 1=extensionpart
  int i;
  for(i=0; copy_file[i] != '\0'; i++){
    if(copy_file[i] == '.'){
      copy_filename[name_length] = '\0';
      name_or_ex = 1;
      i++;
    }
    if(name_or_ex == 0){
      copy_filename[name_length] = toupper(copy_file[i]); //convert to upper
      name_length++;
    }else{
      copy_fileex[ex_length] = toupper(copy_file[i]);
      ex_length++;
    }
  }
  copy_fileex[ex_length] = '\0';

  //search filename: if match get first cluster number and size else file not found
  f_map = f_map + SEC_SIZE * 19; //move to root directory
  int cluster_number = search_for_file(f_map, copy_filename, copy_fileex);
  f_map = f_map - SEC_SIZE * 19; //restore mapping pointer

  //if failed to find file in disk
  if(cluster_number == 0){
     printf("File not found.\n");
     close(file_system);
     munmap(f_map, f_info.st_size);
     exit(0);
  }

  //create new file in current directory to be wriiten
  int new_file = open(copy_file, O_RDWR | O_CREAT, 0644);

  //if create fail, error
  if(new_file < 0){
    printf("ERROR: failed to open new file\n");
    close(new_file);
    close(file_system);
    munmap(f_map, f_info.st_size);
    exit(1);
  }
  //seek file (according found file size) move to the end of file
  int seek_return = lseek(new_file,found_filesize-1, SEEK_SET);
  //if seek failed
  if(seek_return == -1){
    printf("ERROR: failed to seek new file.\n");
    close(new_file);
    close(file_system);
    munmap(f_map, f_info.st_size);
    exit(1);
  }
  seek_return = write(new_file, "", 1); //write to mark the end of file
  //if write filed
  if(seek_return != 1){
    printf("ERROR: failed to write to new file.\n");
    close(new_file);
    close(file_system);
    munmap(f_map, f_info.st_size);
    exit(1);
  }

  char * new_map = mmap(NULL, found_filesize, PROT_WRITE, MAP_SHARED, new_file, 0);

  //start copying file contain to new file
  copy_to_newfile(f_map, new_map, cluster_number);

  //free memory/ finish
  close(new_file);
  munmap(new_map, found_filesize);
  close(file_system);
  munmap(f_map, f_info.st_size);
  return 0;
}
