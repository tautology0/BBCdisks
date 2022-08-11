#ifndef D64
#define D64

/* directory filetype */
typedef struct
{
   char name[255];
   int owner_permissions;
   int group_permissions;
   int world_permissions;
   int owner;
   int group;
   int flags;
   int start_track;
   int start_sector;
   int size;
} directory_type;

int d64_read_sector( char *dblock, int track, int sector, FILE *handle );
int d64_list_directory( directory_type *direct, FILE *handle );
int d64_copy_file( FILE *inhandle, FILE *outhandle, directory_type direct );

#endif
