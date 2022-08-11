#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include "d64.h"

char infile[256], outfile[256];
int verbose, noglob, inter;

void syntax(command)
char *command;
{
   printf("Syntax: %s -f <d64image> <d64file> [<d64file>...] <outfile>\n", \
      command);
   printf("Flags are:\n");
   printf("\t-h|-?      - this message\n");
   printf("\t-i         - interactive - prompt on filename\n");
   printf("\t-n         - noglob\n");
   printf("\t-v         - verboseg\n");
   exit(0);
}

int parse_arguments(argc,argv)
int argc;
char *argv[];
{
   int carg=1,c=0,carg2;

   infile[0]='\0';

   while (carg<argc)
   {
      c=0;
      while (argv[carg][0]='-')
      {
         carg2=carg;
         while (c<strlen(argv[carg])-1)
         {
            switch(argv[carg][++c])
            {
               case 'h' :
               case '?' :
                  syntax(argv[0]);

               case '-' :
                  break;

               case 'n' :
                  noglob=1;
                  break;

               case 'v' :
                  verbose=1;
                  break;

               case 'i' :
                  inter=1;
                  break;

               case 'f' :
                  strncpy(infile,argv[++carg2],255);
                  break;

               default :
                  printf("%s: Invalid flag: -%c\n",argv[0],argv[carg][c]);
                  syntax(argv[0]);
            }
            if (argv[carg][++c]=='-') break;
            carg++;
         }
      }
      if ( argv[carg][0] != '-' ) break;
   }

   if (infile[0]=='\0')
   {
      printf("%s: No input file specified\n",argv[0]);
      syntax(argv[0]);
   }
 
   return carg;
}
 
int main( argc, argv)
int argc;
char **argv;
{
   unsigned char *dblock;
   int success,i,j,dirsize,next;
   FILE *inhandle,*outhandle;
   directory_type *directory;
   char permissions[12];

   strncpy( infile, argv[1], 255);
   next=2;

   dblock=(char *) calloc( 1, 260 );
   directory=(directory_type *) calloc( 144, sizeof( directory_type ) );

   inhandle=fopen( infile, "r" );
   if ( inhandle == 0 )
   {
      printf( "Failed to open file %s\n", argv[1] );
      return 1;
   }

   outhandle=fopen( argv[3], "w" );
   if ( outhandle == 0 )
   {
      printf( "Failed to open file %s\n", argv[3] );
      return 1;
   }

   dirsize=d64_list_directory( directory, inhandle );
   if ( dirsize == -1 )
   {
      printf( "Failed to read directory\n" );
      fclose( inhandle );
      fclose( outhandle );
      return 1;
   }

   j=2;
   {
      i=0;
      while( i < dirsize )
      {
         success=fnmatch( argv[j], directory[i].name, 0);
         if (success == 0) break;
         i++;
      }
      
      if ( i == dirsize+1 )
      {
         printf( "Failed to find file %s\n",argv[2] );
         fclose( inhandle );
         fclose( outhandle );
         return 1;
      }
   
      d64_copy_file( inhandle, outhandle, directory[i] );
   }
   fclose( inhandle );
   fclose( outhandle );
   return 0;
}
