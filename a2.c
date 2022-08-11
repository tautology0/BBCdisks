#include <stdio.h>
#include <stdlib.h>
#include "common.h"

int a2_read_sector(dblock,track,sector,handle)
char *dblock;
int track,sector;
FILE *handle;
{
   int offset,success;

   /* tracks start at 0 - 256 bytes/sector */
   offset=track*(16 * 256);

   /* work out sector offset - sectors start at 0! */
   offset+=(sector*256); /* offset for the sector start */

   if ( fseek( handle, offset, SEEK_SET) != 0 ) return -1;
   success=fread( (void *) dblock, 256, 1, handle);
   if ( success != 1 ) return -1;
   return 0;
}

int a2_list_directory( direct, handle )
directory_type *direct;
FILE *handle;
{
   unsigned char *direct_block, *offset;
   int current_entry=0, i, j, end=0, success=0, namesize=0, entries=0;
   int next_track=17, next_sector=0;

   direct_block=(char *)malloc(260);
   offset=direct_block;

   /* first read the location of the catalogue */
   success=a2_read_sector( direct_block, next_track, next_sector, handle );
   if ( success == -1 )
   {
      free( direct_block );
      return -1;
   }
   offset=direct_block;

   /* Get the directory entries from the current sector */
   next_track=direct_block[1];
   next_sector=direct_block[2];

   while ( end == 0 )
   {
      success=a2_read_sector( direct_block, next_track, next_sector, handle );
      if ( success == -1 )
      {
         free( direct_block );
         return -1;
      }
      offset=direct_block;

      /* Get the directory entries from the current sector */
      next_track=direct_block[1];
      next_sector=direct_block[2];

      /* Move to the start of the directory entries */
      offset+=0x0b;

      if (next_track == 0 && next_sector == 0)
      {
         entries=(next_sector / 0x20);
      }
      else
      {
         entries=7;
      }

      for ( i=0; i < entries; i++ )
      {
         direct[current_entry].flags=offset[2];
         direct[current_entry].start_track=offset[0];
         direct[current_entry].start_sector=offset[1];
         namesize=0;
         for ( j=3; j < 0x20; j++)
         {
            direct[current_entry].name[j-3]=(offset[j] & 0x7f);
            namesize++;
         }

         if (strlen(direct[current_entry].name) == 0)
         { /* name doesn't exist; assume the directory ain't right! */
            current_entry--;
            end=1;
            break;
         }
         direct[current_entry].name[namesize]='\0';

         direct[current_entry].size=offset[0x21]+(offset[0x22]*256);
         offset+=0x23;

         direct[current_entry].owner_permissions=7;
         direct[current_entry].group_permissions=7;
         direct[current_entry].world_permissions=7;
         direct[current_entry].owner=0;
         direct[current_entry].group=0;

         if (direct[current_entry].start_track == 0xff) 
         { /* Is not a real file! */
            current_entry--;
         }
      current_entry++;
      }

      if ( next_track == 0 )
      { /* end of directory */
         end=1;
         break;
      }
   }
   free( direct_block );
   return current_entry;
}

int a2_copy_file( inhandle, outhandle, direct )
FILE *inhandle, *outhandle;
directory_type direct;
{
   unsigned char *direct_block, *offset;
   int i, j, success=0, endsize=254;
   int next_track=direct.start_track, next_sector=direct.start_sector;

   direct_block=(char *)malloc(260);
   printf ("Copying out file %s\n",direct.name);

   while ( next_track != 0 )
   {
      success=a2_read_sector( direct_block, next_track, next_sector, inhandle ); 
      if ( success == -1 )
      {
         free( direct_block );
         return -1;
      }
      next_track=direct_block[0];
      next_sector=direct_block[1];

      if ( next_track == 0 )
      {
         endsize=next_sector - 2;
      }
      offset=direct_block+2;
      success=fwrite( offset, endsize, 1, outhandle );
      if ( success != 1 )
      {
         free( direct_block );
         return -1;
      }
   }
   return 0;
}
