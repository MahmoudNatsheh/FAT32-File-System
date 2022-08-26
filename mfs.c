/*
  Name: Mahmoud Natsheh
  ID:1001860023
*/

// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_NUM_ARGUMENTS 4

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

//structure to hold attributes of the fat32 system
struct  __attribute__((__packed__)) DirectoryEntry{
  char     DIR_Name[11];
  uint8_t  DIR_Attr;
  uint8_t  Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t  Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};
//creating an instance of direcotryEntry with 16 fields max to utilize
struct DirectoryEntry dir[16];

char    BS_OEMName[8];
int16_t BPB_BytsPerSec;
int8_t  BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t  BPB_NumFATS;
int16_t BPB_RootEntCnt;
char    BS_VolLab[11];
int32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

//global file pointer used to open the fat32.img file and allow all commands to utilize it
FILE *fp = NULL;

//used in the undel command, backup for original dir, used to restore the name once
//we call to restore/undelet the file
struct DirectoryEntry saved[16];

//code from slides used to calculate nextLB
int16_t NextLB( uint32_t sector)
{
  uint32_t FATAddress = ( BPB_BytsPerSec * BPB_RsvdSecCnt ) + ( sector * 4 );
  uint16_t val;
  fseek( fp, FATAddress, SEEK_SET );
  fread( &val, 2, 1, fp );
  return val;
}

//code from slides used to calculate offset
int LBAtoOffset(int32_t sector)
{
  return (( sector - 2 ) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec);
}

//modified code from github that takes in two parameters and checks if they are the same string
//returns 1 if same, 0 if not
int compare (char* a, char* b)
{
  char IMG_Name[12];
  strncpy(IMG_Name, a, 11);

  char input[12];
  strncpy(input, b, 11);

  char expanded_name[12];
  
  if( strncmp( b, "..", 2) != 0)
  {
    memset( expanded_name, ' ', 12 );
  
    char *token = strtok( input, "." );

    strncpy( expanded_name, token, strlen( token ) );

    token = strtok( NULL, "." );

    if( token )
    {
      strncpy( (char*)(expanded_name+8), token, strlen(token ) );
    }

    expanded_name[11] = '\0';

    int i;
    for( i = 0; i < 11; i++ )
    {
      expanded_name[i] = toupper( expanded_name[i] );
    }
  }
  else {
    strncpy( expanded_name, "..", 2);
    expanded_name[3] = '\n';
    if( strncmp( expanded_name, IMG_Name, 2 ) == 0 )
    {
      return 1;
    }

    return 0;
  }

  if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
  {
    return 1;
  }

  return 0;
}


int main()
{

  int i = 0;
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  //while user did not enter quit or exit keep looping and getting user input
  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    //if user inputs blank line then ignore and wait for next user input
    if(token[0] == NULL)
    {

    }
    else
    {
      //if user input is open
      if(strcmp(token[0], "open") == 0)
      {
        //if there is not a currently open file then open a file and read it
        if(fp == NULL)
        {
          fp = fopen(token[1], "r");

          //if file had problems opening tell the user
          if(fp == NULL)
          {
            printf("Error: File system image not found.\n");
          }
          //if file successfully opened move in the file and read values needed
          else
          {

            fseek(fp, 11, SEEK_SET);
            fread(&BPB_BytsPerSec, 2, 1, fp);

            fseek(fp, 13, SEEK_SET);
            fread(&BPB_SecPerClus, 1, 1, fp);

            fseek(fp, 14, SEEK_SET);
            fread(&BPB_RsvdSecCnt, 2, 1, fp);

            fseek(fp, 16, SEEK_SET);
            fread(&BPB_NumFATS, 1, 1, fp);

            fseek(fp, 36, SEEK_SET);
            fread(&BPB_FATSz32, 1, 4, fp);

            fseek(fp, 0x100400, SEEK_SET);
            fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
          }
        }
        //if we already have a file open, don't open another until file is closed
        else
        {
          printf("Error: File system image is already open.\n");
        }
      }
      //if user input is close
      else if(strcmp(token[0], "close") == 0)
      {
        //if there is no file open then ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open.\n");
        }
        //if there is a file open close it and set indicator as needed
        else
        {
          fclose(fp);
          fp = NULL;
        }
      }
      //if user input is info
      else if(strcmp(token[0], "info") == 0)
      {
        //if no file is open ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open.\n");
        }

        //if there is a file open print information about the file in decimal and hexadecimal
        else
        {
          printf("BPB_BytsPerSec:%10d %10x\n", BPB_BytsPerSec, BPB_BytsPerSec);
          printf("BPB_SecPerClus:%10d %10x\n", BPB_SecPerClus, BPB_SecPerClus);
          printf("BPB_RsvdSecCnt:%10d %10x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
          printf("BPB_NumFATS:   %10d %10x\n", BPB_NumFATS, BPB_NumFATS);
          printf("BPB_FATSz32:   %10d %10x\n", BPB_FATSz32, BPB_FATSz32);        
        }
      }
      //if user input is stat
      else if(strcmp(token[0], "stat") == 0)
      {
        int found = 0;
        //if no file is open ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            found = compare(dir[i].DIR_Name, token[1]);
            //if user requested file is found print stats about it
            if(found)
            {
              printf("File Attribute          Size          Starting Cluster Number\n");
              printf("%-10d              %-10d    %-10d\n", dir[i].DIR_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
              break;
            }
          }
          //if user reuested file is not found ignore user input
          if(!found)
          {
            printf("Error: File not found\n");
          }
        }
      }
      //if user input is cd
      else if(strcmp(token[0], "cd") == 0)
      {
        int found;
        //if no file is open ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            found = compare(dir[i].DIR_Name, token[1]);
            //if user requested file is found go/open the file/directory by allocating the correct amount of offset
            if(found) 
            {
              int cluster = dir[i].DIR_FirstClusterLow;
              //if head node is selected then set cluster to 2 (offset value to read)
              if(cluster == 0)
              {
                cluster = 2;
              }
              int offset = LBAtoOffset(cluster);
              fseek(fp, offset, SEEK_SET);
              fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
              break;
            }
          }
          //if user requested file is not found, ignore user input
          if(!found)
          {
            printf("Error: File not found\n");
          }
        }
      }
      //if user input is ls
      else if(strcmp(token[0], "ls") == 0)
      {
        //if no file is open ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            //if the file requested by user is of the right formatt/attributes and it is not "deleted" 
            //('?' is used to indicate that file is deleted) show it to user
            if(((dir[i].DIR_Attr == 0x01) || (dir[i].DIR_Attr == 0x10) || (dir[i].DIR_Attr == 0x20)) && (dir[i].DIR_Name[0] != '?'))
            {
              char name[12];
              memcpy(name, dir[i].DIR_Name, 11);
              name[11] = '\0';
              printf("%s\n", name);
            }
          }
        }
      }
      //if user input is get
      else if(strcmp(token[0], "get") == 0)
      {
        //search directory for file
        //save cluster number low
        //save file size
        int found;
        //if no file is open ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            found = compare(dir[i].DIR_Name, token[1]);
            //if file requested by user is found then take it and put it in users current working directory
            if(found)
            {

              //read the user requested file into buffer, which will be used to write to the users current working directory
              int offset = LBAtoOffset(dir[i].DIR_FirstClusterLow);
              fseek(fp, offset, SEEK_SET);
              
              //create a new file in the users current working directory named the current files name on fat32.img 
              FILE *ofp = fopen(token[1], "w");
              uint8_t buffer[512];
              
              //take the size of the file so we know how large to write to the new file on user's cwd
              int size = dir[i].DIR_FileSize;

              //keep printing (reading and writeing) the contents of the file to the new file on users cwd until the size is 
              //done, indicating we have copied the entier file to users cwd
              while(size >= BPB_BytsPerSec)
              {
                fread(&buffer, 512, 1, fp);
                fwrite(&buffer, 512, 1, ofp);

                size = size - BPB_BytsPerSec;
                //new offset
                int cluster = NextLB(cluster);
                if(cluster > -1)
                {
                  offset = LBAtoOffset(cluster);
                  fseek(fp, offset, SEEK_SET);
                }
              }
              if(size > 0)
              {
                fread(buffer, size, 1, fp);
                fwrite(buffer, size, 1, ofp);
              }
              fclose(ofp);
              break;
            }
          }
          //if user requested file is not found, ignore the user input
          if(!found)
          {
            printf("Error: File not found\n");
          }
        }
      }
      //if user input is read
      else if(strcmp(token[0], "read") == 0)
      {
        int found;
        //if no file is open, ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            int offset = atoi(token[2]);
            found = compare(dir[i].DIR_Name, token[1]);
            //if user requested file is found on fat32.img, then print to the console the requested contents of the file that the user requested
            if(found)
            {
              //using the user input of the offset and how much to read, set the offset and seek file pointer, store the contents 
              //requested from file in a buffer and print buffer to screen once all data from file is collected
              int offset = LBAtoOffset(dir[i].DIR_FirstClusterLow);
              fseek(fp, offset, SEEK_SET);
              fseek(fp, atoi(token[2]), SEEK_CUR);
              
              char buffer[512];
              
              fread(buffer, atoi(token[3]), 1, fp);

              for(i = 0; i < atoi(token[3]); i++)
              {
                printf("%d ", buffer[i]);
                if(i == (atoi(token[3]) - 1))
                {
                  printf("%d\n", i);
                }
              }
              break;
            }
          }
          //if user requested file is not found on fat32.img, then ignore user input
          if(!found)
            {
              printf("Error: File not found\n");
            }
        }
      }
      //if user input is del
      else if(strcmp(token[0], "del") == 0)
      {
        int found;
        //if no file is open, ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        else
        {
          //store the contents of the fat32.img into a backup copy to recover later
          //just make recovery copy to recover deleted items
          fseek(fp, 0x100400, SEEK_SET);
          fread(&saved[0], sizeof(struct DirectoryEntry), 16, fp);

          //look through all files on fat32.img and find the file requested by user
          for(i = 0; i < 16; i++)
          {
            found = compare(dir[i].DIR_Name, token[1]);
            //if user requested file is found on the fat32.img then change its DIR_NAME to '?'
            //this symbol indicated it has been temporarly deleted from the fat32.img
            if(found)
            {
              dir[i].DIR_Name[0] = '?';
              break;
            }
          }
          //if user requested file is not found of the far32.img, ignore user input
          if(!found)
          {
            printf("Error: File not found\n");
          }
        }
      }
      //if user input is undel
      else if(strcmp(token[0], "undel") == 0)
      {
        int found;
        //if no file is open, ignore user input
        if(fp == NULL)
        {
          printf("Error: File system not open\n");
        }
        //look through all files on fat32.img and find the file requested by user
        for(i = 0; i < 16; i++)
        {
          found = compare(saved[i].DIR_Name, token[1]); 
          //check if the file exist on the backup copy we made in the delete command function and the file matches what we have in the original copy
          //and if the original copy currently has the symbol we used to indicate deleted, then restore the orgianl copy using our backup
          if((dir[i].DIR_Name[0] == '?') && (dir[i].DIR_FirstClusterLow == saved[i].DIR_FirstClusterLow) && compare(saved[i].DIR_Name, token[1]) == 1)
          {
            dir[i].DIR_Name[0] = saved[i].DIR_Name[0];
            break;
          }
        }
        //if the file was not found, then ignore user input
        if (!found)
        {
          printf("Error: File not found\n");
        }
      }
      //if user input is quit or exit
      else if((strcmp(token[0], "quit") == 0)|| (strcmp(token[0], "exit") == 0))
      {
        //end the program and exit
        exit(0);
      }
    }
    
    //close all the current working files not closed
    free( working_root );

  }
  return 0;
}



