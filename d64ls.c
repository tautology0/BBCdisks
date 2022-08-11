#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d64.h"

char perm_string[4];

char *parse_permissions(int permission)
{
   strcpy( perm_string, "---" );
   /* first owner */
   if ( (permission & 4) != 0 ) perm_string[0]='r';
   if ( (permission & 2) != 0 ) perm_string[1]='w';
   if ( (permission & 1) != 0 ) perm_string[2]='x';

   return perm_string;
}

int main( int argc, char **argv)
{
   unsigned char *dblock;
   int success,i;
   FILE *inhandle;
   directory_type *directory;
   char permissions[12];

   dblock=(unsigned char *) calloc( 1, 260 );
   directory=(directory_type *) calloc( 144, sizeof( directory_type ) );

   inhandle=fopen( argv[1], "r" );
   if ( inhandle == 0 )
   {
      printf( "Failed to open file %s\n", argv[1] );
      return 1;
   }

   success=d64_list_directory( directory, inhandle );
   if ( success == -1 )
   {
      printf( "Failed to read directory\n" );
      fclose( inhandle );
      return 1;
   }

   for ( i=0; i < success; i++ )
   {
      if ( directory[i].flags != 0 )
      {
         /* break up the access rights */ 
         strcpy(permissions,"-");
         strcat(permissions,parse_permissions(directory[i].owner_permissions));
         strcat(permissions,parse_permissions(directory[i].group_permissions));
         strcat(permissions,parse_permissions(directory[i].world_permissions));

         printf( "%s   0 %-9d%-9d%8d %s\n", permissions, directory[i].owner, \
            directory[i].group, directory[i].size*254, directory[i].name);
      }
   }

   return 0;
}
