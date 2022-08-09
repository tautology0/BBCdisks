#include <stdio.h>
#include <stdlib.h>
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include "common.h"

FILE *inhandle;
// Memory's cheap so we'll just reserve enough space to cover all
// eventualities
bbc_directory_type directory[144];
bbc_disk_type diskinfo;
int num_files;
char bootflags[10][4] = { "off", "load", "run", "exec" };

int bbc_read_sector(dblock,track,sector,handle)
unsigned char *dblock;
int track,sector;
FILE *handle;
{
   int offset,success;

   /* tracks start at 0 - 256 bytes/sector */
   offset=track*(10 * 256);

   /* work out sector offset - sectors start at 0! */
   offset+=(sector*256); /* offset for the sector start */

   if ( fseek( handle, offset, SEEK_SET) != 0 ) return -1;
   success=fread( (void *) dblock, 256, 1, handle);
   if ( success != 1 ) return -1;
   return 0;
}

int bbc_write_sector(dblock,track,sector,handle)
char *dblock;
int track,sector;
FILE *handle;
{
   int offset,success;

   /* tracks start at 0 - 256 bytes/sector */
   offset=track*2560;

   /* work out sector offset - sectors start at 0! */
   offset+=(sector*256); /* offset for the sector start */

   if ( fseek( handle, offset, SEEK_SET) != 0 ) return -1;
   success=fwrite( (void *) dblock, 256, 1, handle);
   if ( success != 1 ) return -1;
   return 0;
}

int bbc_list_directory( direct, handle )
bbc_directory_type *direct;
FILE *handle;
{
   unsigned char *direct_block, *offset=0;
   int current_entry=0, j, end=0, success=0, namesize=0;
   int next_track=0, next_sector=0;

   direct_block=(unsigned char *)malloc(260);
   offset=direct_block;

   /* Read the filenames first */
   success=bbc_read_sector( direct_block, next_track, next_sector, handle );
   if ( success == -1 )
   {
      free( direct_block );
      return -1;
   }
   offset=direct_block;

   // Copy the first half of the diskname
   strncpy(diskinfo.name,(char *)offset,8);
   offset+=8;

   while( ( end == 0 ) && ( current_entry < 31 ) )
   {
      /* first read the directory entry */
      // dirname:7 contains the lock bit
      direct[current_entry].name[0]=offset[7]&0x7f;
      if (offset[7]&0x80) direct[current_entry].locked=1;
      direct[current_entry].name[1]='.';

      namesize=2;
      for ( j=0; j < 7 ; j++)
      {
         // cut off the top bit as this may contain extra info
         direct[current_entry].name[j+2]=(offset[j]&0x7f);
         if (offset[j]!=' ') namesize++;
      }

      if (strlen(direct[current_entry].name) == 0)
      { /* name doesn't exist; assume the directory ain't right! */
         current_entry--;
         end=1;
      }
      direct[current_entry].name[namesize]='\0';
      current_entry++;
      offset+=8;
   }

   /* Now read the directory information */
   success=bbc_read_sector( direct_block, 0, 1, handle );
   if ( success == -1 )
   {
      free( direct_block );
      return -1;
   }
   offset=direct_block;

   // Copy the rest of the disk name and the metadata
   strncat(diskinfo.name,(char *)offset,4);
   diskinfo.cycle=offset[4];
   diskinfo.bootflag=(offset[6]&0x30) >> 4;
   offset+=8;

   current_entry=0;
   end=0;
   while( ( end == 0 ) && ( current_entry < 31 ) )
   {
      direct[current_entry].load=offset[0]+(offset[1]*256)+((offset[6]&0x0c)>>2)*65536;
      direct[current_entry].exec=offset[2]+(offset[3]*256)+((offset[6]&0xc0)>>6)*65536;
      direct[current_entry].size=offset[4]+(offset[5]*256)+((offset[6]&0x30)>>4)*65536;

      direct[current_entry].start_track=(offset[7]+((offset[6]&0x03)*256))/10;
      direct[current_entry].start_sector=(offset[7]+((offset[6]&0x03)*256))%10;

      if (strlen(direct[current_entry].name)==0)
      { /* end of directory */
         current_entry--;
         break;
      }

      current_entry++;
      offset+=8;
   }

   if (current_entry==31) current_entry--;
   free( direct_block );
   return current_entry+1;
}

char *bbc_read_file( block, offset, size, inhandle, direct )
char *block;
off_t offset;
size_t size;
FILE *inhandle;
bbc_directory_type direct;
{
   int track=direct.start_track, sector=direct.start_sector;
   int success=0, copy_size;
   unsigned char *direct_block;
   int block_offset,out_offset=0;
   direct_block=(unsigned char *)malloc(260);

   // First we need to work out which sector the offset is in
   while (offset > 256)
   {
      sector++;
      sector%=10;
      track=(sector==0)?track+1:track;
      offset -= 256;
   }

   // Now we should be at the sector containing the information
   // So we read it
   while (size>0)
   {
      copy_size=256;
      success=bbc_read_sector( direct_block, track, sector, inhandle );
      if ( success == -1 )
      {
         free( direct_block );
         return NULL;
      }
      // if we're part way through a sector here's were we do it
      block_offset=offset;
      if (offset>0)
      {
         copy_size=256-offset;
      }
      offset=0;
      if (copy_size > size) copy_size=size;

      memcpy(block+out_offset,direct_block+block_offset,copy_size);
      out_offset+=copy_size;

      size-=copy_size;
      sector++;
      sector%=10;
      track=(sector==0)?track+1:track;
   }

   return block;
}

int bbc_copy_file( inhandle, outhandle, direct )
FILE *inhandle, *outhandle;
bbc_directory_type direct;
{
   unsigned char *direct_block;
   int i, success=0, endsize=256;
   int next_track=direct.start_track, next_sector=direct.start_sector;
   int sectors=direct.size/256+1, filesize=direct.size;

   direct_block=(unsigned char *)malloc(260);

   for ( i=0; i<sectors; i++ )
   {
      success=bbc_read_sector( direct_block, next_track, next_sector, inhandle );
      if ( success == -1 )
      {
         free( direct_block );
         return -1;
      }
      next_sector++;
      next_sector%=10;
      next_track=(next_sector==0)?next_track+1:next_track;

      if ( filesize < 256 )
      {
         endsize=filesize;
      }
      else
      {
         filesize-=256;
      }
      success=fwrite( direct_block, endsize, 1, outhandle );
      if ( success != 1 )
      {
         free( direct_block );
         return -1;
      }
   }
   return 0;
}

int bbc_assign_sector( directory, nofiles, direct )
bbc_directory_type *direct;
bbc_directory_type *directory;
int nofiles;
{
   char *map;
   int i,j,sector_count,start_sector;
   /* first draw up a map of what sectors are free - this is totally
      inefficient - so sue me! */

   map=calloc(1,500);
   memset(map,0,500);
   /* Reserve the catalogue */
   map[0]=map[1]=1;

   for (i=0; i<nofiles; i++)
   {
      sector_count=(directory[i].size/256)+1;
      start_sector=(directory[i].start_track*10)+directory[i].start_sector;

      for (j=start_sector; j<start_sector+sector_count; j++)
      {
         map[j]=1;
      }
   }

   /* Now find free space long enough! */
   sector_count=direct->size/256+1;
   i=0;
   do
   {
      while (map[i]==1 && i < 400) i++;
      if ( i < 400 )
      {
         j=i;
         while (map[j]==0 && j < 400 ) j++;
         if ( (j-i) >= sector_count )
         {
            /* Huzzah enough free space! */
            start_sector=i;
            break;
         }
      }
   } while ( i < 400 );

   direct->start_sector=start_sector % 10;
   direct->start_track=start_sector / 10;

   return 0;
}

int bbc_put_file( inhandle, outhandle, direct, directory, nofiles )
FILE *inhandle, *outhandle;
bbc_directory_type direct;
bbc_directory_type *directory;
int nofiles;
{
   int i,j;
   int sector_start=2;
   i=j=0;
   sector_start=1;
   return 0;
}

static int lookup_name(const char *path)
{
   int i=0, entry=-1;
   do
   {
      if (strcmp(path+1, directory[i].name) == 0)
      {
         // Woo! we've got a match!
         entry = i;
      }
      i++;
   } while (entry == -1 && i < num_files);

   return entry;
}

static int bbc_getattr(const char *path, struct stat *stbuf)
{
   int entry=-1, res=0;

   // First black out the statbuf
   memset( stbuf, 0, sizeof(struct stat) );

   // Standard stuff
   stbuf->st_uid = getuid();
   stbuf->st_gid = getgid();
   // Check whether it's the root directory
   if (strcmp(path, "/") == 0)
   {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
   }
   else
   {
      // Now we've got to convert the path to a target
      entry = lookup_name(path);
      fprintf(stderr,"%s %d\n",path,entry);

      if (entry != -1)
      {
         // Now return everything
         stbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
         if (!directory[entry].locked)
         {
            stbuf->st_mode |= S_IWUSR;
         }
         stbuf->st_size = directory[entry].size;
         // put the inode as the index into the directory for ease!
         stbuf->st_ino = entry;
         stbuf->st_nlink = 1;
      }
      else
      {
         res = -ENOENT;
      }
   }

   return res;
}

static int bbc_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
   int i=0;

   (void) offset;
   (void) fi;

   if (strcmp(path, "/") != 0)
      return -ENOENT;

   filler(buf, ".", NULL, 0);
   filler(buf, "..", NULL, 0);
   for (i=0; i<num_files; i++)
   {
      filler(buf, directory[i].name, NULL, 0);
   }

   return 0;
}

static int bbc_open(const char *path, struct fuse_file_info *fi)
{
   int entry=-1;

   // first find the name
   entry = lookup_name(path);
   if (entry == -1)
      return -ENOENT;

   // Cool we have a valid name!
   // Now set the file handle to the directory entry block so we
   // can use it
   fi->fh=entry;
   return 0;
}

static int bbc_release(const char *path, struct fuse_file_info *fi)
{
   return 0;
}

static int bbc_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
   int entry=-1;

   entry = lookup_name(path);
   if (entry == -1)
      return -ENOENT;
   if (size==0 || size>directory[entry].size) size=directory[entry].size;
   bbc_read_file( buf, offset, size, inhandle, directory[entry] );

   return size;
}

static int bbc_getxattr(const char *path, const char *name,
                        char *value, size_t size)
{
   int entry=-1;
   char buffer[256];
   int sizeout=0;

   // First check if we're talking about the root file
   if (strcmp(path,"/")==0)
   {
      // Now check for the xattr we support:
      // user.bbc.bootflag
      // user.bbc.title
      if (strcmp(name,"user.bbc.title")==0)
      {
         strncpy(buffer,diskinfo.name,15);
         sizeout=strlen(buffer);
      }
      else if (strcmp(name,"user.bbc.cycle")==0)
      {
         sizeout=snprintf(buffer,10,"%d",diskinfo.cycle);
      }
      else if (strcmp(name,"user.bbc.bootflag")==0)
      {
         if (diskinfo.bootflag < 4)
         {
            strncpy(buffer, bootflags[diskinfo.bootflag],10);
            sizeout=strlen(buffer);
         }
         else
         {
            sizeout=snprintf(buffer,10,"%d",diskinfo.bootflag);
         }
      }
      else return -ENOATTR;
   }
   else
   {
      entry = lookup_name(path);
      if (entry == -1) return -ENOENT;

      // Now check for the xattr we support:
      // user.bbc.load
      // user.bbc.exec
      if (strcmp(name,"user.bbc.load")==0)
      {
         sizeout=snprintf(buffer,10,"0x%x",directory[entry].load);
      }
      else if (strcmp(name,"user.bbc.exec")==0)
      {
         sizeout=snprintf(buffer,10,"0x%x",directory[entry].exec);
      }
      else return -ENOATTR;
   }

   if (size>0)
   {
      if (sizeout>size) return -ERANGE;
      strncpy(value, buffer, sizeout);
   }
   return sizeout;
}

int add_listxattr(char *buffer, char *member)
{
      strcpy(buffer, member);
      return strlen(buffer)+1;
}

static int bbc_listxattr(const char *path, char *list,
                         size_t size)
{
   int entry=-1;
   char buffer[256],*ptr=buffer;
   int sizeout=0;
   // First check if we're talking about the root file
   if (strcmp(path,"/")==0)
   {
      ptr+=add_listxattr(ptr, "user.bbc.title");
      ptr+=add_listxattr(ptr, "user.bbc.cycle");
      ptr+=add_listxattr(ptr, "user.bbc.bootflag");
   }
   else
   {
      entry = lookup_name(path);
      if (entry == -1) return -ENOENT;

      ptr+=add_listxattr(ptr, "user.bbc.load");
      ptr+=add_listxattr(ptr, "user.bbc.exec");
   }
   sizeout=ptr-buffer;
   if (size>0)
   {
      if (sizeout>size) return -ERANGE;
      memcpy(list, buffer, sizeout);
   }
   return sizeout;
}

static struct fuse_operations bbc_oper =
{
   .getattr  = bbc_getattr,
   .readdir  = bbc_readdir,
   .open     = bbc_open,
   .release  = bbc_release,
   .read     = bbc_read,
   .getxattr = bbc_getxattr,
   .listxattr = bbc_listxattr,
};

int main(int argc, char **argv)
{
   // First parameter is the file - so we can use this ourselves
   // ere we pass to fuse.
   int success;

   inhandle=fopen( argv[1], "rb" );
   if (inhandle == 0)
   {
      fprintf(stderr,"Failed to open disc image %s\n", argv[1]) ;
      return 1;
   }

   // Whilst we're here we may as well cache the directory
   success=bbc_list_directory( directory, inhandle );
   if ( success == -1 )
   {
      fprintf(stderr, "Failed to read image's directory\n");
      fclose( inhandle );
      return 1;
   }
   num_files=success;

   // Now we've got the cache let's spawn fuse
   return fuse_main(argc-1, argv+1, &bbc_oper, NULL);
}
