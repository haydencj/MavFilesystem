#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 1001
#define MAX_FILE_SIZE 1048576
#define HIDDEN 0x00000001
#define HIDDEN_MASK 0xFE
#define READ_ONLY 0x2
#define READ_MASK 0xFD

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

// 512 blocks just for free block map
uint8_t * free_blocks;
uint8_t * free_inodes;

// directory
struct directoryEntry
{
    char filename[64];
    short in_use;
    int32_t inode;
};

struct directoryEntry* directory;

// inode
struct inode
{
    int32_t  blocks[BLOCKS_PER_FILE];
    short    in_use;
    uint8_t  attribute;
    uint32_t file_size;
};

struct inode* inodes;

FILE    *fp;
char    image_name[64];
uint8_t image_open;



#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports four arguments

int32_t findFreeBlock()
{
    int i;
    for(i = 0; i < NUM_BLOCKS; i++)
    {
        if(free_blocks[i])
        {
            free_blocks[i] = 0;
            return i + 1001;
        }
    }

    return -1;
}

int32_t findFreeInode()
{
    int i;
    for(i = 0; i < NUM_FILES; i++)
    {
        if(free_inodes[i])
        {
            free_inodes[i] = 0;
            return i;
        }
    }

    return -1;
}

int32_t findFreeInodeBlock(int32_t inode)
{
    int i;
    for(i = 0; i < BLOCKS_PER_FILE; i++)
    {
        if(inodes[inode].blocks[i] == -1)
        {
            inodes[inode].blocks[i] = 0;
            return i;
        }
    }

    return -1;
}


void init()
{
    directory   = (struct directoryEntry*) &data[0][0];
    inodes      = (struct inode*) &data[20][0];
    free_blocks = (uint8_t*) &data[1000][0];
    free_inodes = (uint8_t*) &data[19][0];

    memset(image_name, 0, 64);
    image_open = 0;

    int i;
    for(i = 0; i < NUM_FILES; i++)
    {
        directory[i].in_use = 0;
        directory[i].inode  = -1;
        free_inodes[i]      = 1;

        memset(directory[i].filename, 0, 64);

        int j;
        for(j = 0; j < NUM_BLOCKS; j++)
        {
            inodes[i].blocks[j] = -1;
            inodes[i].in_use = 0; 
            inodes[i].attribute = 0x0;
            inodes[i].file_size = 0;
        }
    }

    int j;
    for(j = 0; j < NUM_BLOCKS; j++)
    {
        free_blocks[j] = 1;
    }
}

uint32_t df()
{
    int j;
    int count = 0;
    for(j = 0; j < NUM_BLOCKS; j++)
    {
        if(free_blocks[j])
        {
            count++;
        }
    }

    return count * BLOCK_SIZE;
}

void createfs(char * filename)
{
    fp = fopen(filename, "w");

    strncpy(image_name, filename, strlen(filename));

    memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);

    image_open = 1;

    int i;
    for(i = 0; i < NUM_FILES; i++)
    {
        directory[i].in_use = 0;
        directory[i].inode  = -1;
        free_inodes[i]      = 1;

        memset(directory[i].filename, 0, 64);

        int j;
        for(j = 0; j < NUM_BLOCKS; j++)
        {
            inodes[i].blocks[j] = -1;
            inodes[i].in_use = 0; 
            inodes[i].attribute = 0x0;
            inodes[i].file_size = 0;
        }
    }

    int j;
    for(j = 0; j < NUM_BLOCKS; j++)
    {
        free_blocks[j] = 1;
    }

    fclose(fp);
}

void savefs()
{
    if(image_open == 0)
    {
        printf("ERROR: Disk image is not open\n");
    }

    // fp = fopen(image_name, "w");
    FILE* fp2 = fopen(image_name, "w");

    fwrite(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp2);

    memset(image_name, 0, 64);

    fclose(fp2);
}

void openfs(char * filename)
{
    fp = fopen(filename, "r");

    if(fp == NULL)
    {
        printf("ERROR. File not found\n");
    }

    strncpy(image_name, filename, strlen(filename));

    fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);

    image_open = 1;
}

void closefs()
{
    if(image_open == 0)
    {
        printf("ERROR: Disk image is not open\n");
        return;
    }

    fclose(fp);

    image_open = 0;
    memset(image_name, 0, 64);
}

void list(char* attrib)
{
    int i;
    int not_found = 1;
    int list_hidden = 0;
    int list_attributes = 0;

    if(attrib == NULL)
    {
        // do nothing
    }
    else if(!strcmp(attrib, "-h"))
    {
        list_hidden = 1;
    }
    else if(!strcmp(attrib, "-a"))
    {
        list_attributes = 1;
    }
    else
    {
        printf("ERROR: Incorrect parameter %s.\n", attrib);
        return;
    }
    

    for(i = 0; i < NUM_FILES; i++)
    {
        //\TODO Add a checm to not list if the file is hidden
        if(directory[i].in_use)
        {
            not_found = 0;
            char filename[65];
            uint32_t inode_index = directory[i].inode;

            memset(filename, 0, 65);
            strncpy(filename, directory[i].filename, strlen(directory[i].filename));
 /*
                +h +r 1
                +h 1
                +r 1
                nothing: 0
            
            */


            if(((inodes[inode_index].attribute & HIDDEN) != 1) && (list_attributes))
            {
                printf("%s\tAttribute: ", filename);
                        
                for (int i = 7; i >= 0; i--) {
                    uint8_t mask = 1 << i;
                    uint8_t bit = (inodes[inode_index].attribute & mask) >> i;
                    printf("%d", bit);
                }

                printf("\n");
            

            }
            else if((inodes[inode_index].attribute & HIDDEN) != 1)
            {
                printf("%s\n", filename);

            }
            else if(list_hidden)
            {
                printf("%s\n", filename);
            }

        }

    }

    if(not_found)
    {
        printf("ERROR: No files found.\n");
    }
}

void insert(char * filename)
{
    // verify the filename isn't NULL
    if(filename == NULL)
    {
        printf("ERROR: Filename is NULL\n");
        return;
    }

    // verify the file exists
    struct stat buf;
    int ret = stat(filename, &buf);

    if(ret == -1)
    {
        printf("ERROR: File does not exist.\n");
        return;
    }

    // verify the file is not too big
    if(buf.st_size > MAX_FILE_SIZE)
    {
        printf("ERROR: File is too large.\n");
        return;
    }

    // verify that there is enough space
    if(buf.st_size > df())
    {
        printf("ERROR: Not enough free disk space.\n");
        return;
    }
    // find an empty directory entry
    int i;
    int directory_entry = -1;
    for(i = 0; i < NUM_FILES; i++)
    {
        if(directory[i].in_use == 0)
        {
            directory_entry = i;
            break;
        }
    }

    if(directory_entry == -1)
    {
        printf("ERROR: Could not find a free directory entry.\n");
    }

    // Open the input file read-only 
    FILE *ifp = fopen ( filename, "r" ); 
    printf("Reading %d bytes from %s\n", (int) buf . st_size, filename);
 
    // Save off the size of the input file since we'll use it in a couple of places and 
    // also initialize our index variables to zero. 
    int32_t copy_size   = buf . st_size;

    // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int32_t offset      = 0;               

    // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
    // memory pool. Why? We are simulating the way the file system stores file data in
    // blocks of space on the disk. block_index will keep us pointing to the area of
    // the area that we will read from or write to.
    int block_index = -1;

      // find a free inode
      int32_t inode_index = findFreeInode();

      if(inode_index == -1)
      {
        printf("ERROR: Can not find a free inode.\n");
        return;
      }    

      // place the file info in the directory
      directory[directory_entry].in_use = 1;
      directory[directory_entry].inode = inode_index;
      strncpy(directory[directory_entry].filename, filename, strlen(filename));

      inodes[inode_index].file_size = buf.st_size;
      inodes[inode_index].in_use = 1;
 
    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size > 0 )
    {
      // Index into the input file by offset number of bytes.  Initially offset is set to
      // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
      // then increase the offset by BLOCK_SIZE and continue the process.  This will
      // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
      fseek( ifp, offset, SEEK_SET );
 
      // Read BLOCK_SIZE number of bytes from the input file and store them in our
      // data array. 

      // find a free block

      block_index = findFreeBlock();
      if(block_index == -1)
      {
        printf("ERROR: Can not find a free block.\n");
      }    

     

      int32_t bytes  = fread( data[block_index], BLOCK_SIZE, 1, ifp );

      
      // save the block in the inode
      int32_t inode_block = findFreeInodeBlock(inode_index);
      inodes[inode_index].blocks[inode_block] = block_index;


      // If bytes == 0 and we haven't reached the end of the file then something is 
      // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
      // It means we've reached the end of our input file.
      if( bytes == 0 && !feof( ifp ) )
      {
        printf("ERROR: An error occured reading from the input file.\n");
        return;
      }

      // Clear the EOF file flag.
      clearerr( ifp );

      // Reduce copy_size by the BLOCK_SIZE bytes.
      copy_size -= BLOCK_SIZE;
      
      // Increase the offset into our input file by BLOCK_SIZE.  This will allow
      // the fseek at the top of the loop to position us to the correct spot.
      offset    += BLOCK_SIZE;
    }

    // We are done copying from the input file so close it out.
    fclose( ifp );
 

}

void delete(char* filename)
{
    if(filename == NULL)
    {
        printf("ERROR: Filename not specified.\n");
        return;
    }

    int delete_index = -1;
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(strcmp(filename, directory[i].filename) == 0)
        {
            delete_index = i;
            break;
        }
        
    }

    if(delete_index == -1)
    {
        printf("ERROR: File does not exist.\n");
        return;
    }

    uint32_t inode_index = directory[delete_index].inode;
    if((inodes[inode_index].attribute & READ_ONLY) == 2)
    {
        printf("ERROR: %s is read-only.\n", filename);
    }
    else
    {
        directory[delete_index].in_use = 0;
        inode_index = directory[delete_index].inode;
        inodes[inode_index].in_use = 0;
    }

    return;
    
}
void undel(char* filename)
{
    if(filename == NULL)
    {
        printf("ERROR: Filename not specified.\n");
        return;
    }
    
    int undelete_index = -1;
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(strcmp(filename, directory[i].filename) == 0)
        {
            undelete_index = i;
            break;
        }
        
    }

    if(undelete_index == -1)
    {
        printf("ERROR: File does not exist.\n");
        return;
    }

    directory[undelete_index].in_use = 1;
    uint32_t inode_index = directory[undelete_index].inode;
    inodes[inode_index].in_use = 1;
}

void retrieve(char* filename, char* new_filename)
{
  int i;
  int directory_location = -1;
  for(i = 0; i < NUM_FILES; i++)
  {
    if(!strcmp(directory[i].filename, filename))
    {
      directory_location = i;
      break;
    }
  }


  if(directory_location == -1)
  {
    printf("ERROR: File not found\n");
    return;
  }


  int file_inode = directory[directory_location].inode;
  

  if(new_filename == NULL)
  {
    fp = fopen(filename, "w");
  }
  else
  {
    fp = fopen(new_filename, "w");
  }

  
  // Record the file size to know how many bytes to copy,
  // initialize offset and current_block
  int32_t offset = 0;
  int32_t current_block = 0;
  int32_t copy_size = inodes[file_inode].file_size;


  while(inodes[file_inode].blocks[current_block] != (int32_t) -1)
  {
    int32_t bytes;

    // Save off the current block within our inode that has our data
    int32_t block_index = inodes[file_inode].blocks[current_block];

    fseek(fp, offset, SEEK_SET);

    if(copy_size < BLOCK_SIZE)
    {
      bytes = fwrite(data[block_index], (int) copy_size, 1, fp);
    }
    else
    {
      bytes = fwrite(data[block_index], BLOCK_SIZE, 1, fp);
    }


    if( (bytes == 0) && (inodes[file_inode].blocks[current_block] != -1) )
    {
      printf("ERROR: An error occurred writing to the specified file\n");
      return;
    }

    offset += BLOCK_SIZE;
    copy_size -= BLOCK_SIZE;
    current_block ++;
  }



  fclose(fp);

}

void read_bytes(char* filename, uint32_t start_byte, uint32_t req_num_bytes)
{
  int file_location = -1;
  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    if(!strcmp(directory[i].filename, filename))
    {
      file_location = i;
      break;
    }
  }


  if(file_location == -1)
  {
    printf("ERROR: File not found\n");
    return;
  }


  if(req_num_bytes == 0)
  {
    printf("ERROR: No bytes to read\n");
    return;
  }

  
  int32_t file_inode = directory[file_location].inode;
  if(req_num_bytes > inodes[file_inode].file_size)
  {
    printf("ERROR: Request exceeds file size\n");
    return;
  }


  uint32_t file_size = inodes[file_inode].file_size;
  if( (start_byte + req_num_bytes) > file_size)
  {
    printf("ERROR: Specifications of request exceed file size\n");
    return;
  }

  
  uint32_t start_block_index = 0;
  uint32_t temp_start_byte = start_byte;
  uint32_t req_bytes_temp = req_num_bytes;
  uint32_t num_blocks_to_read = 1;

  // Find out how many blocks we need to read
  while(req_bytes_temp > (uint32_t)BLOCK_SIZE)
  {
    num_blocks_to_read++;
    req_bytes_temp -= BLOCK_SIZE;
  }
  
  
  // Find out the first index of our block array within inode
  // we should start looking at
  // We are also able to then record the byte we should start
  // reading at within the start block
  while(temp_start_byte > (uint32_t)BLOCK_SIZE)
  {
    start_block_index++;
    temp_start_byte -= (uint32_t)BLOCK_SIZE;
  }
       

  int32_t remaining_bytes = req_num_bytes;
  int32_t data_block_location = inodes[file_inode].blocks[start_block_index];
  int32_t curr_block_index = start_block_index;

  while(remaining_bytes != 0)
  {
    if(temp_start_byte == 1023)
    {
      temp_start_byte = 0;
      data_block_location = inodes[file_inode].blocks[curr_block_index];
    }

    printf("%x", data[data_block_location][temp_start_byte]);

    temp_start_byte++;
    remaining_bytes--;
  }
  printf("\n");

  return;
}

void encrypt(char* filename, char* key)
{
    if(filename == NULL)
    {
        printf("ERROR: Filename not specified.\n");
        return;
    }

    FILE *readFile;
    FILE *writeFile;
 
    readFile = fopen(filename, "r");
    writeFile = fopen(filename, "r+");
    char c;

    if(!readFile || !writeFile)
    {
        printf("ERROR: File does not exist.\n");
        return;
    }

    do
    {
        c = fgetc(readFile);
        if(feof(readFile))
        {
            break;
        }
        c = c ^ *key;
        fputc(c, writeFile);
    } 
    while(1);
    
    fclose(readFile);
    fclose(writeFile);
}

void attrib(char* attribute, char* filename)
{
    int change_attrib_index = -1;
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(strcmp(filename, directory[i].filename) == 0)
        {
            change_attrib_index = i;
            break;
        }
    }

    if(change_attrib_index == -1)
    {
        printf("ERROR: File not found.\n");
        return;
    }

    uint32_t inode_index = directory[change_attrib_index].inode;

    // +h, -h, +r, -r
    if(strcmp(attribute, "+h") == 0)
    {
        inodes[inode_index].attribute |= HIDDEN;
        return;
    }
    if(strcmp(attribute, "+r") == 0)
    {
        inodes[inode_index].attribute |= READ_ONLY;
        return;
    }
    if(strcmp(attribute, "-h") == 0)
    {
        inodes[inode_index].attribute &= HIDDEN_MASK;
        return;
    }
    if(strcmp(attribute, "-r") == 0)
    {
        inodes[inode_index].attribute &= READ_MASK;
        return;
    }

    printf("ERROR: Incorrent attribute.\n");



}

int main()
{

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

  fp = NULL;

  init();

  while( 1 )
  {
    // Print out the msh prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      token[i] = NULL;
    }

    int   token_count = 0;                                 

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;                                         

    char *working_string  = strdup( command_string );                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    if(strcmp("createfs", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified.\n");
            continue;
        }

        createfs(token[1]);
    }

    if(strcmp("savefs", token[0]) == 0)
    {
        savefs();
    }

    if(strcmp("open", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue; 
        }
        openfs(token[1]);
    }

    if(strcmp("close", token[0]) == 0)
    {
        closefs();
    }

    if(strcmp("list", token[0]) == 0)
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }

        list(token[1]);
    }

    if(strcmp("df", token[0]) == 0)
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }
        printf("%d bytes free\n", df());
    }

    if(strcmp("insert", token[0]) == 0)
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue; 
        }

        insert(token[1]);
    }

    if(!strcmp("retrieve", token[0]))
    {
      if(!image_open)
      {
        printf("ERROR: Disk image is not open");
        continue;
      }

      if(token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }

      retrieve(token[1], token[2]);
    }

    if(!strcmp("read", token[0]))
    {
      if(!image_open)
      {
        printf("ERROR: Disk image is not open\n");
        continue;
      }

      read_bytes(token[1], (uint32_t) atoi(token[2]), (uint32_t) atoi(token[3]) );
    }

    if (strcmp("encrypt", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue; 
        }

        if(token[2] == NULL)
        {
            printf("ERROR: No key specified\n");
            continue; 
        }

        encrypt(token[1], token[2]);
    }

    if (strcmp("decrypt", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue; 
        }

        if(token[2] == NULL)
        {
            printf("ERROR: No key specified\n");
            continue; 
        }

        encrypt(token[1], token[2]);
    }

    if(strcmp("delete", token[0]) == 0)
    {
      delete(token[1]);
    }
    
    if (strcmp("undel", token[0]) == 0)
    {
        undel(token[1]);
    }

    // attrib +h filename.txt
    if(strcmp("attrib", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No attribute listed.\n");
            continue;
        }
        if(token[2] == NULL)
        {
            printf("ERROR: No filename listed.\n");
            continue;
        }

        attrib(token[1], token[2]);
    }

    if(strcmp("quit", token[0]) == 0)
    {
        exit(0);
    }

    // // Now print the tokenized input as a debug check
    // // \TODO Remove this for loop and replace with your shell functionality

    // int token_index  = 0;
    // for( token_index = 0; token_index < token_count; token_index ++ ) 
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );  
    // }


    // Cleanup allocated memory
    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      if( token[i] != NULL )
      {
        free( token[i] );
      }
    }

    free( head_ptr );

  }

  free( command_string );

  return 0;
  // e2520ca2-76f3-90d6-0242ac120003
}
