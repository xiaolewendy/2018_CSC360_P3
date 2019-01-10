/*CSC360 A03 SFS: Part II: disklist
 *Student: Xiaole (Wendy) Tan
 */

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
char *f_map;
int contain_sublayer = 0;

//FUNCTIONS

//FUNCTION: read disk label
void print_disklabel(char *label, char *file){
  //Volume label: The boot sector->starting byte:43, length 11(bytes)
  int i;
  for(i=0; i<11; i++){
    label[i] = file[i+43];
  }
  //find label in root directory (SECTOR: 19) 0x2600
  if(label[0] == ' '){
    //move to root directory (Sector size: 512 bytes)
    file = file + SEC_SIZE *19;
    while(file[0] != 0x00){
      //offset 11: attributes -> mask: 0x08 Volume label
      if(file[11] == 8){
        //read label
        for(i=0; i<8; i++){
          label[i] = file[i];
        }
      }
    //read next entry
    file = file + 32;
    }
  }
  printf("%s\n", label);
}

//FUNCTION: travser and list info regarding directories
int list_directory(char *file, char *dir, int layer){

  if(file[0] == 0x00){
    return 0;
  }

  //print sublayer title ONCE
  if(layer > contain_sublayer){
    printf("%s\n", dir);
    printf("==================\n");
    contain_sublayer++;
  }

  //info
  char type; //type: F for file/D for directory
  int  file_size; //10 characters to show the file size in bytes
  char fname [15]; //file name
  int y, m, d, h, min; //creation date and creation time

  //record info
  //skip if first logical cluster = 1/0
  int logical_cluster = (file[26] & 0xFF) + (file[27] << 8);
  if(logical_cluster == 1 || logical_cluster == 0){
    file = file + 32;
    return list_directory(file, dir, layer);//skip entry
  }else{
    //type
    if((file[11] & 0x10) == 0x10){ //attr subdirectory bit
      type = 'D';
    }else{
      type = 'F';
    }
    //size
    file_size = (file[28] & 0xFF) + ((file[29] & 0xFF) << 8) + ((file[30] & 0xFF) << 16) + ((file[31] & 0xFF) << 24); //little endian
    //file name (first field, 8 bytes)
    int i;
    int name_length = 0;
    int total_length = 0;
    for(i=0; i<8; i++){
      if(file[i] == ' '){
        break;
      }
      fname[i] = file[i];
      name_length++;
      total_length++;
    }
    //read extension part
    if(type == 'F'){
      fname[name_length] = '.';
      name_length++;
      total_length++;
      //name extension
      for(i=0; i<3; i++){
        if(file[i+8] == ' '){
          break;
        }
        fname[i+name_length] = file[i+8];
        total_length++;
      }
    }
    fname[total_length] = '\0';

    //creation date and time
    int creation_date = (file[16] & 0xFF) + (file[17] << 8);
    y = ((creation_date & 0xFE00) >> 9) + 1980; //high 7 bits + 1980
    m = (creation_date & 0x01E0) >> 5; //middle 4 bits
    d = creation_date & 0x001F;//low 5 bits
    int creation_time = (file[14] & 0xFF) + (file[15] << 8);
    h = (creation_time & 0xF800) >> 11; //high 5 bits
    min = (creation_time & 0x07E0) >> 5; //middle 6 bits

    //if subdirectory
    if(type == 'D'){
      int sub_location = SEC_SIZE * (logical_cluster + 31) + 64; //move to sub sector and skip entry . and ..
      char *sub_dire = f_map - SEC_SIZE * 19 + sub_location; //record sub location
      int sub_layer = layer + 1;
      file = file + 32;
      //output directory info (date: mm/dd/yyyy)
      printf("%c %10d %20s %2d/%2d/%4d %d:%d\n", type, file_size, fname, m, d, y-1980, h, min);
      //TRAVERSE CURRENTLEVEL THEN SUBLEVEL
      return list_directory(file, dir, layer) + list_directory(sub_dire, fname, sub_layer);
    }else{
      file = file + 32; //next entry
      //output file info (date: mm/dd/yyyy)
      printf("%c %10d %20s %2d/%2d/%4d %d:%d\n", type, file_size, fname, d, m, y, h, min);
      return list_directory(file, dir, layer);
    }
  }
  return 0;
}

//MAIN
int main(int argc, char *argv[]){
  //catch wrong input format
  if(argc < 2){
    printf("ERROR: Invalid arugument. Enter: disklist <file system image>.\n");
    exit(1);
  }

  //open read file system
  int file_system = open(argv[1], O_RDONLY);
  if(file_system < 0){
    printf("ERROR: Failed to open file system image.\n");
    exit(1);
  }

  //map to memory
  struct stat f_info;
  fstat(file_system, &f_info);
  f_map = mmap(NULL, f_info.st_size, PROT_READ, MAP_SHARED, file_system, 0);

  //print root level directory name (disk label)
  char* root = malloc(sizeof(char));
  print_disklabel(root, f_map);
  printf("==================\n");

  //start searching file starting from root directory, layer 0
  f_map = f_map + SEC_SIZE * 19; //move pointer to root directory
  list_directory(f_map, root, 0);
  f_map = f_map - SEC_SIZE * 19; //restore f_map pointer


  //free memory/ finish
  free(root);
  close(file_system);
  munmap(f_map, f_info.st_size);
  return 0;
}
