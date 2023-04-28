# OS-Project-3
Group Members: Mattan Pelah, Jonathan Guzman, Ricardo Ledezma

Division of Labor:
   Part 1: Jonathan Guzman
   Part 2: Jonathan Guzman
   Part 3: Jonathan Guzman
   Part 4: Jonathan Guzman and Mattan Pelah
   Part 5: Jonathan Guzman and Mattan Pelah
   Part 6: Jonathan Guzman and Mattan Pelah
   Extra Credit: Jonathan Guzman
   
Files:
  Makefile: 
      A makefile which will produce an executable called "filesys" and object file called "filesys.o" when using the "make" command so long as source file "filesys.c" is in the current working directory. The command "make clean" will remove all object files ending in .o
      
  filesys.c:
      The source file for our project, which can be compiled using "gcc filesys.c" to create the "a.out" executable, or using "gcc -o filesys filesys.c" to create the "filesys" executable
      
 How to use Makefile:
    While the file "filesys.c" and the file "Makefile" is in the current working directory, use the "make" command to create an object file called "filesys.o" and an executable you can use to run the file system program called "filesys".
    When you are done, you can remove all object files (ending in .o) including "filesys.o" by using the command "make clean", also while "Makefile" is in the current working directory.
    
Bugs:
    When ran on valgrind, valgrind reports that line 477, which contains the command "fseek(fp, newClusterOffset, SEEK_SET);" is utilizing an uninitialized byte, presumably newClusterOffset. newClusterOffset is guaranteed to be initialized at the beginning of each iteration of this section of code, so it is unclear why valgrind reports this. This appears in the valgrind report file during runtime, and seems to have absolutely no symptoms on how the executable runs or how data is read or written from the file image.
    
Extra Credit:
    rm -r [FILENAME] : This project supports recursive directory deletion
    dump [FILENAME] : This project supports the hexdumping "dump" function. Note, this implementation of dump formats output different for file contents vs. directory contents.
