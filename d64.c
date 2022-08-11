#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d64.h"

int d64_read_sector(dblock,track,sector,handle)
char *dblock;
int track,sector;
FILE *handle;
{
   int offset,success;

   /* Due to Commodore strangeness we have to vary the offset as sectors
      are different depending upon the track! */

   if ( track <= 17 )
   {
      offset=((track-1)*(21*256)); /* offset for the track start */
   }
   else if ( track <= 24 )
   { 
      offset=0x16500+((track-18)*(19*256));
   }
   else if ( track <= 30 )
   {
      offset=0x1ea00+((track-25)*(18*256));
   }
   else
   {
      offset=0x25600+((track-31)*(17*256));
   }

   /* work out sector offset - sectors start at 0! */
   offset+=((sector)*256); /* offset for the sector start */

   if ( fseek( handle, offset, SEEK_SET) != 0 ) return -1;
   success=fread( (void *) dblock, 256, 1, handle);
   if ( success != 1 ) return -1;
   return 0;
}

int d64_list_directory( direct, handle )
directory_type *direct;
FILE *handle;
{
   unsigned char *direct_block, *offset;
   int current_entry=0, i, j, end=0, success=0, namesize=0, entries=0;
   int next_track=18, next_sector=1;

   direct_block=(char *)malloc(260);
   offset=direct_block;

   while ( end == 0 )
   {
      success=d64_read_sector( direct_block, next_track, next_sector, handle );
      if ( success == -1 )
      {
         free( direct_block );
         return -1;
      }
      offset=direct_block;

      /* Get the directory entries from the current sector */
      next_track=direct_block[0];
      next_sector=direct_block[1];

      if (next_track == 0)
      {
         entries=(next_sector / 0x20);
      }
      else
      {
         entries=8;
      }

      for ( i=0; i < entries; i++ )
      {
         direct[current_entry].flags=offset[2];
         direct[current_entry].start_track=offset[3];
         direct[current_entry].start_sector=offset[4];
         namesize=0;
         for ( j=5; j < 21; j++)
         {
            if ( offset[j] != 0xa0 )
            {
               direct[current_entry].name[j-5]=offset[j];
               namesize++;
            }
         }

         if (strlen(direct[current_entry].name) == 0)
         { /* name doesn't exist; assume the directory ain't right! */
            current_entry--;
            end=1;
            break;
         }
         direct[current_entry].name[namesize]='\0';

         direct[current_entry].size=offset[0x1e]+(offset[0x1f]*256);
         offset+=0x20;

         direct[current_entry].owner_permissions=7;
         direct[current_entry].group_permissions=7;
         direct[current_entry].world_permissions=7;
         direct[current_entry].owner=0;
         direct[current_entry].group=0;

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

int d64_copy_file( inhandle, outhandle, direct )
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
      success=d64_read_sector( direct_block, next_track, next_sector, inhandle ); 
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
