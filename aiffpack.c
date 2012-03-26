/*
** Copyright (C) 2005 Shea Ako
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
BUILD INTRUCTIONS:

1) Build and install libsndsfile (http://www.mega-nerd.com/libsndfile).

2) gcc -o aiffpack aiffpack.c -Wall -O2 -lsndfile
   or for a statically linked binary
   gcc -o aiffpack aiffpack.c -Wall -O2 /path/to/libsndfile.a
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>

#define _AIFFPACK_VERSION "0.12"
#define _AIFFPACK_OPTIONS "hvb:fdwB:"
#define _MAX_CHANNELS 256L
#define _DEFAULT_BLOCKSIZE_SAMPLES (sf_count_t)2048
#define _MIN_BLOCKSIZE_SAMPLES (sf_count_t)1
#define _COMMAND_BUFFER_SIZE 256

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

const char aiffpack_helpstring[]={"\n\
aiffpack is a utility for creating multichannel AIFF (or optionally WAV) sound files\n\
from a set of input sound files of varying formats and resolutions. The sample data\n\
type and bit resolution of the output file can also be specified. The length of the\n\
output file is the length of the longest input file. Shorter input files will create\n\
tracks that are padded at the end with silence. The order of the input files specified\n\
determines the order of the tracks in the output file. The first track of the output\n\
file is the first track of the first input file and the last track of the output file\n\
is the last track of the last input file.\n\
\n\
aiffpack version %s is Copyright (C) 2005, Shea Ako and is  made possible by\n\
%s Copyright (C) 1999-2004 Erik de Castro Lopo.\n\
\n\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or\n\
(at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with this program; if not, write to the Free Software\n\
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n\
"};

typedef struct
{
  char *path;
  SNDFILE *file;
  SF_INFO info;
  char *buffer;
  int bytes_per_frame; /* all input file sample data is read in as output file format so bytes_per_frame=info.channels * output_file_bytes_per_sample */
  sf_count_t index;
} aiffpack_file;

/* generic read frame and write frame function pointer types */
typedef sf_count_t  (*read_frames_ptr)  (SNDFILE *sndfile, void *ptr, sf_count_t frames);
typedef sf_count_t  (*write_frames_ptr)  (SNDFILE *sndfile, void *ptr, sf_count_t frames);


/* globals */
char command_buffer [_COMMAND_BUFFER_SIZE]; /* buffer for return info from sf_command() calls */

/* function prototypes */
void print_usage();
void init_aiffpack_file(aiffpack_file *x);
void print_aiffpack_file_info(aiffpack_file *x);
void print_help();

int
main(int argc, char **argv)
{
  int input_file_count;
  sf_count_t output_file_total_frames;
  int output_file_bytes_per_sample;
  sf_count_t blocksize_frames;
  aiffpack_file *input_files;
  aiffpack_file output_file;
  read_frames_ptr read_frames;
  write_frames_ptr write_frames;
  int verbose;

  int c,file_count,i,j,rval=0;
  sf_count_t count, number_frames_read, dst;

  /* init */
  input_file_count=0;
  input_files=0;
  init_aiffpack_file(&output_file);
  output_file_total_frames=0;
  read_frames=0;
  write_frames=0;
  output_file.info.format=SF_FORMAT_AIFF|SF_FORMAT_PCM_16; /* default output format 16 bit PCM AIFF */
  output_file_bytes_per_sample=sizeof(int); /* default PCM */
  blocksize_frames=_DEFAULT_BLOCKSIZE_SAMPLES;
  verbose=0;

  if(argc<2)
    {
    print_usage();
    return -1;
    }

  /* help some versions of getopt to work with my code */
  setenv("POSIXLY_CORRECT","YES",0);
  
  /* count files, check for -h */
  for(file_count=0,opterr=0;optind<argc;)
    {
      while((c=getopt(argc, argv, _AIFFPACK_OPTIONS))!=EOF)
	{
	  if(c=='h')
	    {
	      print_help();
	      return 0;
	    }
	}
      if(optind<argc) file_count++;
      optind++;
    }

  input_file_count=file_count-1;

  if(file_count<2)
    {
      fprintf(stderr,"error: not enough files specified\n");
      print_usage();
      return -1;
    }

  /* allocate memory for input_files */
  input_files=(aiffpack_file*)malloc((input_file_count)*sizeof(aiffpack_file));
  if(!input_files)
    {
      fprintf(stderr,"error: out of memory");
      return -1;
    }

  /* init input_files[] */
  for(i=0;i<input_file_count;i++)
    init_aiffpack_file(&input_files[i]);

  /* parse options and filenames */
  for(i=0,optind=1,opterr=1;optind<argc;optind++)
    {
      while((c=getopt(argc, argv, _AIFFPACK_OPTIONS))!=EOF)
	{
	  switch(c)
	    {
	    case 'v':
	      verbose=1;
	      break;
	     
	    case 'f':
	      output_file.info.format=(output_file.info.format&(~SF_FORMAT_SUBMASK))|SF_FORMAT_FLOAT;
	      output_file_bytes_per_sample=sizeof(float);
	      break;

	    case 'd':
	      output_file.info.format=(output_file.info.format&(~SF_FORMAT_SUBMASK))|SF_FORMAT_DOUBLE;
	      output_file_bytes_per_sample=sizeof(double);
	      break;
	      
	    case 'b':
	      output_file_bytes_per_sample=sizeof(int); /* all pcm formats are read and stored internally as int */
	      output_file.info.format=(output_file.info.format&(~SF_FORMAT_SUBMASK))|(min(max(SF_FORMAT_PCM_S8,(atoi(optarg))),SF_FORMAT_PCM_32));
	      break;

	    case 'w':
	      output_file.info.format=(output_file.info.format&(~SF_FORMAT_TYPEMASK))|SF_FORMAT_WAV;
	      break;

	    case 'B':
	      blocksize_frames=max(_MIN_BLOCKSIZE_SAMPLES,(sf_count_t)atoi(optarg));
	      break;

	    case '?':
	      print_usage();
	      return -1;
	      break;
	    }
	}

      if(optind<argc)
	{
	  if(i==input_file_count)
	    {
	      output_file.path=argv[optind];
	    }
	  else
	    {
	      input_files[i].path=argv[optind];
	      i++;
	    }
	}
    }

  /* print version */
  if(verbose)
    {
      sf_command (NULL, SFC_GET_LIB_VERSION, command_buffer, sizeof (command_buffer));
      printf("\naiffpack version %s using %s\n\n",_AIFFPACK_VERSION,command_buffer);
    }

  /* open files */
  for(i=0;i<input_file_count;i++)
    {
      if(!(input_files[i].file=sf_open(input_files[i].path,SFM_READ,&input_files[i].info)))
	{
	  fprintf(stderr,"%s (%s)\n",sf_strerror(input_files[i].file),input_files[i].path);
	  rval=-1;
	  goto cleanup_and_exit;
	}
    }
  
  if(verbose)
    {
      printf("INPUT:\n");
      for(i=0;i<input_file_count;i++)
	print_aiffpack_file_info(&input_files[i]);
      printf("\n");
    }

  /* total up channels */
  for(i=0;i<input_file_count;i++)
    output_file.info.channels+=input_files[i].info.channels;

  /* bounds check on output channels */
  if(output_file.info.channels>_MAX_CHANNELS)
    {
      fprintf(stderr,"error: maximum number of output file channels is %ld",_MAX_CHANNELS);
      rval=-1;
      goto cleanup_and_exit;
    }

  /* confirm that all input files are at same sample rate */
  output_file.info.samplerate=input_files[0].info.samplerate;
  for(i=1;i<input_file_count;i++)
    if(output_file.info.samplerate!=input_files[i].info.samplerate)
      {
	fprintf(stderr,"error: all input files must be at same sample rate");
	rval=-1;
	goto cleanup_and_exit;
      }

  /* confirm output file format is legal */
  if(!(sf_format_check(&output_file.info)))
  {
    fprintf(stderr,"error: illegal output format\n");
    rval=-1;
    goto cleanup_and_exit;
  }

  /* find longest file */
  for(i=0;i<input_file_count;i++)
      output_file_total_frames=max(output_file_total_frames,input_files[i].info.frames);

  if(verbose)
    {
      printf("OUTPUT:\n");
      output_file.info.frames=output_file_total_frames;
      print_aiffpack_file_info(&output_file);
      printf("\n");
      output_file.info.frames=0;
    }
 
  /* assign read_frames and write_frames functions */
  switch(output_file.info.format&SF_FORMAT_SUBMASK)
    {
      case SF_FORMAT_FLOAT:
	read_frames=(read_frames_ptr)sf_readf_float;
	write_frames=(write_frames_ptr)sf_writef_float;
	break;

    case SF_FORMAT_DOUBLE:
      read_frames=(read_frames_ptr)sf_readf_double;
      write_frames=(write_frames_ptr)sf_writef_double;
      break;

    default:
      read_frames=(read_frames_ptr)sf_readf_int;
      write_frames=(write_frames_ptr)sf_writef_int;
      break;
    }

  /* assign bytes_per_frame members */
  for(i=0;i<input_file_count;i++)
    input_files[i].bytes_per_frame=input_files[i].info.channels * output_file_bytes_per_sample;
  
  output_file.bytes_per_frame=output_file.info.channels * output_file_bytes_per_sample;

  /* allocate buffers */
  for(i=0;i<input_file_count;i++)
    {
      if(!(input_files[i].buffer=(void*)malloc(input_files[i].bytes_per_frame * blocksize_frames)))
	{
	  fprintf(stderr,"error: out of memory\n");
	  rval=-1;
	  goto cleanup_and_exit;
	}
    }
  if(!(output_file.buffer=(void*)malloc(output_file.bytes_per_frame * blocksize_frames)))
    {
      fprintf(stderr,"error: out of memory\n");
      rval=-1;
      goto cleanup_and_exit;
    }

  /* open output file */
  if(!(output_file.file=sf_open(output_file.path,SFM_WRITE,&output_file.info)))
    {
      fprintf(stderr,"%s (%s)\n",sf_strerror(output_file.file),output_file.path);
      rval=-1;
      goto cleanup_and_exit;
    }

  /* create and write output file */

  /* status info */
  printf("creating %i channel file %s [      ",output_file.info.channels,output_file.path);

  for(count=0;count<output_file_total_frames;count+=blocksize_frames)
    {
      /* status info */
      printf("\b\b\b\b\b%02i%% ]",(int)(100.0*(double)count/(double)output_file_total_frames));
      fflush(stdout);

      /* read in next block from each input file */
      for(i=0;i<input_file_count;i++)
	{
	  number_frames_read=read_frames(input_files[i].file,input_files[i].buffer,blocksize_frames);
	  
	  if(sf_error(input_files[i].file))
	    {
	      fprintf(stderr,"\n%s (%s)\n",sf_strerror(input_files[i].file),input_files[i].path);
	      rval=-1;
	      goto cleanup_and_exit;
	    }

	  /* zero pad rest of buffer if the end of the file has been reached */
	  if(number_frames_read<blocksize_frames)
	    memset(input_files[i].buffer+(number_frames_read * input_files[i].bytes_per_frame) ,0, ((blocksize_frames-number_frames_read) * input_files[i].bytes_per_frame)  );	  

	  input_files[i].index=0; /* reset index (while we're looping through the input files */
	}

      /* fill output file buffer with interleaved frames from input buffers */
      for(j=0,dst=0;j<blocksize_frames;j++)
	{
	  // copy 1 frame from each input file buffer to output file buffer 
	  for(i=0;i<input_file_count;i++)
	    {
	      memcpy(&(output_file.buffer[dst]),&(input_files[i].buffer[input_files[i].index]),input_files[i].bytes_per_frame);
	      input_files[i].index+=input_files[i].bytes_per_frame;

	      dst+=input_files[i].bytes_per_frame;
	    }
	}
      
      /* write output file buffer to file */
      write_frames(output_file.file,output_file.buffer,blocksize_frames);

      if(sf_error(output_file.file))
	{
	  fprintf(stderr,"\n%s (%s)\n",sf_strerror(output_file.file),output_file.path);
	  rval=-1;
	  goto cleanup_and_exit;
	}
    }

  /* the output file is now a multiple of blocksize_frames (probably a bit longer that output_file_total_frames). truncate it so that it is the proper size. */
  sf_command(output_file.file,SFC_FILE_TRUNCATE,&output_file_total_frames,sizeof(output_file_total_frames));

  /* status info */
  printf("\b\b\b\b\b100%% ]\n");


 cleanup_and_exit:
  /* close files */  
  for(i=0;i<input_file_count;i++)
    {
    if(input_files[i].file) sf_close(input_files[i].file);
    if(input_files[i].buffer) free(input_files[i].buffer);
    }
  free(input_files);

  if(output_file.file) sf_close(output_file.file);
  if(output_file.buffer) free(output_file.buffer);

  return rval;
}

void
print_usage()
{
  fprintf (stderr,"Usage: aiffpack [-h] [-v] [-b <bytes> | -f | -d] [-w] [-B <blocksize>] input_files... output_file\n");
  fprintf (stderr,"Options:\n");
  fprintf (stderr,"-h              help\n");
  fprintf (stderr,"-v              verbose\n");
  fprintf (stderr,"-b <bytes>      output PCM bytes per sample (-b 2 or 16 bit PCM is default)\n");
  fprintf (stderr,"-f              32 bit floating point output\n");
  fprintf (stderr,"-d              64 bit floating point output\n");
  fprintf (stderr,"-w              Microsoft WAV format output (AIFF is default)\n");
  fprintf (stderr,"-B <blocksize>  processing blocksize in samples (default %lli)\n",_DEFAULT_BLOCKSIZE_SAMPLES);
}

void
init_aiffpack_file(aiffpack_file *x)
{
  if(x)
    {
      x->path=0;
      x->file=0;
      x->info.frames=0;
      x->info.samplerate=0;
      x->info.channels=0;
      x->info.format=0;
      x->info.sections=0;
      x->info.seekable=0;
      x->buffer=0;
      x->bytes_per_frame=0;
      x->index=0;
    }
}

void
print_aiffpack_file_info(aiffpack_file *x)
{
  static SF_FORMAT_INFO format_info;

  printf("%s\n",x->path);
  printf("    {\n");
  
  format_info.format=x->info.format;
  sf_command (0, SFC_GET_FORMAT_INFO, &format_info, sizeof (format_info)) ;
  printf("    format: %s\n",format_info.name);

  printf("    sample rate: %i\n",x->info.samplerate);
  printf("    channels: %i\n",x->info.channels);
  printf("    length: %f seconds (%lli samples)\n",(double)x->info.frames/(double)x->info.samplerate, x->info.frames);
  printf("    sample data: ");
  switch(x->info.format&SF_FORMAT_SUBMASK)
    {
      case SF_FORMAT_PCM_S8:
	printf("Signed 8 bit data\n");
      break;
      
      case SF_FORMAT_PCM_16:
	printf("Signed 16 bit data\n");
      break;
      
      case SF_FORMAT_PCM_24:
	printf("Signed 24 bit data\n");
      break;
      
      case SF_FORMAT_PCM_32:
	printf("Signed 32 bit data\n");
      break;
      
      case SF_FORMAT_PCM_U8:
	printf("Unsigned 8 bit data\n");
      break;
      
      case SF_FORMAT_FLOAT:
	printf("32 bit float data\n");
      break;
      
      case SF_FORMAT_DOUBLE:
	printf("64 bit float data\n");
      break;
      
      case SF_FORMAT_ULAW:
	printf("U-Law encoded\n");
      break;
      
      case SF_FORMAT_ALAW:
	printf("A-Law encoded\n");
      break;
      
      case SF_FORMAT_IMA_ADPCM:
	printf("IMA ADPCM\n");
      break;
      
      case SF_FORMAT_MS_ADPCM:
	printf("Microsoft ADPCM\n");
      break;
      
      case SF_FORMAT_GSM610:
	printf("GSM 6.10 encoding\n");
      break;
      
      case SF_FORMAT_VOX_ADPCM:
	printf("Oki Dialogic ADPCM encoding\n");
      break;
      
      case SF_FORMAT_G721_32:
	printf("32kbs G721 ADPCM encoding\n");
      break;
      
      case SF_FORMAT_G723_24:
	printf("24kbs G723 ADPCM encoding\n");
      break;
      
      case SF_FORMAT_G723_40:
	printf("40kbs G723 ADPCM encoding\n");
      break;
      
      case SF_FORMAT_DWVW_12:
	printf("12 bit Delta Width Variable Word encoding\n");
      break;
      
      case SF_FORMAT_DWVW_16:
	printf("16 bit Delta Width Variable Word encoding\n");
      break;
      
      case SF_FORMAT_DWVW_24:
	printf("24 bit Delta Width Variable Word encoding\n");
      break;
      
      case SF_FORMAT_DWVW_N:
	printf("N bit Delta Width Variable Word encoding\n");
      break;
      
      default:
        printf("unknown\n");
      break;
    }

  printf("    }\n\n");
}

void
print_help()
{
  sf_command (NULL, SFC_GET_LIB_VERSION, command_buffer, sizeof (command_buffer));
  print_usage();
  fprintf(stderr,aiffpack_helpstring,_AIFFPACK_VERSION,command_buffer);
}
