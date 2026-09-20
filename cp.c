#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]){
  if(argc != 3){
    //need two file names atleast
    printf(1, "Invalid number of arguments.\n");
    exit();
  }
  
  int src = open(argv[1], O_RDONLY);
  if(src == -1){
    printf(1, "Source file not found.\n");
    exit();
  }

  int dest = open(argv[2], O_WRONLY | O_CREATE);
  if(dest == -1){
    printf(1, "Couldnt open destination file\n");
    exit();
  }
  
  char c = 0;
  while(read(src, &c, 1) > 0){
    write(dest, &c, 1);
  }
  
  //exit();   //it requires thissssssss whattttt ?! Apparently xv6 doesnt have a runtime library to which the value might return to...
            //Really need to see how execution works on this thing, no crt0 start script ?

}
