/**
@example bufr_bundle.c
@english
bundle a file containing 1 or several BUFR messages with subsets which match specified search key(s).
This generates multiple OUTPUT-n.bufr files

@endenglish
@francais
@endfrancais
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bufr_api.h"
#include "bufr_io.h"
#include <locale.h>
#include "bufr_i18n.h"
#include "gettext.h"
#include "bufr_util.h"

#define   EXIT_ERROR    5

static  char *str_ltableb = NULL;
static  char *str_ltabled = NULL;
static  char *str_ibufr   = NULL;
static  char *str_obufr   = NULL;

static  BufrDescValue srchkey[100];
static  int           nb_key=0;
static  int           use_compress=0;
static  BufrSection1  section1;

static int  read_cmdline( int argc, const char *argv[] );
static void abort_usage(const char *pgrmname);
static int  bundle_file (BufrDescValue *dvalues, int nbdv);
static int  match_search_pattern( DataSubset *subset, BufrDescValue *dvalues, int nb );
static int  resolve_search_values( BufrDescValue *dvalues, int nb, BUFR_Tables *tbls );


/*
 * nom: abort_usage
 *
 * auteur:  Vanh Souvanlasy
 *
 * fonction: informer la facon d'utiliser le programme
 *
 * parametres:  
 *        pgrmname  : nom du programme
 */
static void abort_usage(const char *pgrmname)
{
   fprintf( stderr, _("BUFR Bundle Version %s\n"), BUFR_API_VERSION );
   fprintf( stderr, _("Copyright Her Majesty The Queen in Right of Canada, Environment Canada, 2009\n") );
   fprintf( stderr, _("Licence LGPLv3\n\n") );
   fprintf( stderr, _("Usage: %s\n"), pgrmname );
   fprintf( stderr, _("          [-inbufr     <filename>]    input BUFR file to decode (default=stdin)\n") );
   fprintf( stderr, _("          [-outbufr    <filename>]    outut BUFR file to encode (default=stdout)\n") );
   fprintf( stderr, _("          [-category value] set header only section1 category as bundle\n") );
   fprintf( stderr, _("          [-orig_centre value]              set origin center as bundle\n") );
   fprintf( stderr, _("          [-master_table_version value]     set master_table_version as bundle\n") );
   fprintf( stderr, _("          \n") );
   fprintf( stderr, _("          [-ltableb    <filename>]    local table B to use for decoding\n") );
   fprintf( stderr, _("          [-ltabled    <filename>]    local table D to use for decoding\n") );
   fprintf( stderr, _("          [-compress] compress datasubsets if possible\n") );
   fprintf( stderr, _("          [-srchkey descriptor value] descriptor value pair(s) search key\n") );
   exit(EXIT_ERROR);
}

/*
 * nom: read_cmdline
 *
 * auteur:  Vanh Souvanlasy
 *
 * fonction: lire la ligne de commande pour extraire les options
 *
 * parametres:  
 *        argc, argv
 */
static int read_cmdline( int argc, const char *argv[] )
{
   int i;

   for ( i = 1 ; i < argc ; i++ ) 
     {
     if (strcmp(argv[i],"-ltableb")==0) 
        {
        ++i; if (i >= argc) abort_usage(argv[0]);
        str_ltableb = strdup(argv[i]);
        } 
     else if (strcmp(argv[i],"-ltabled")==0)
        {
        ++i; if (i >= argc) abort_usage(argv[0]);
        str_ltabled = strdup(argv[i]);
        } 
     else if (strcmp(argv[i],"-inbufr")==0) 
        {
        ++i; if (i >= argc) abort_usage(argv[0]);
        str_ibufr = strdup(argv[i]);
        }
     else if (strcmp(argv[i],"-outbufr")==0) 
        {
        ++i; if (i >= argc) abort_usage(argv[0]);
        str_obufr = strdup(argv[i]);
        } 
     else if (strcmp(argv[i],"-compress")==0)
       {
       use_compress = 1;
       }
     else if (strcmp(argv[i],"-category")==0)
       {
        ++i; if (i >= argc) abort_usage(argv[0]);
       section1.msg_type = atoi(argv[i]);
       }
     else if (strcmp(argv[i],"-orig_centre")==0)
       {
        ++i; if (i >= argc) abort_usage(argv[0]);
       section1.orig_centre = atoi(argv[i]);
       }
     else if (strcmp(argv[i],"-master_table_version")==0)
       {
        ++i; if (i >= argc) abort_usage(argv[0]);
       section1.master_table_version = atoi(argv[i]);
       }
     else if (strcmp(argv[i],"-srchkey")==0)
       {
       int desc;
       ++i; if (i >= argc) abort_usage(argv[0]);
       bufr_init_DescValue( &(srchkey[nb_key]) );
       desc = atoi( argv[i] );
       ++i; if (i >= argc) abort_usage(argv[0]);
       bufr_set_key_string( &(srchkey[nb_key]), desc, &(argv[i]), 1 );
       ++nb_key; 
       }
   }

   return 0;
}

/*
 * nom: cleanup
 *
 * auteur:  Vanh Souvanlasy
 *
 * fonction: nettoyer les allocations du programme
 *
 * parametres:  
 */
static void cleanup(void)
   {
   int i;

   if (str_ltableb != NULL) free(str_ltableb);
   if (str_ltabled != NULL) free(str_ltabled);
   if (str_ibufr!= NULL) free(str_ibufr);
   if (str_obufr!= NULL) free(str_obufr);

   for ( i = 0; i < nb_key ; i++ )
      bufr_vfree_DescValue( &(srchkey[i]) );
   }

/*
 * nom: init_sect1_keys
 *
 * auteur:  Vanh Souvanlasy
 *
 * fonction: initialize section 1 keys as unused 
 *
 * parametres:  
 */
static void  init_sect1_keys( BufrSection1 *s1 )
   {
   s1->bufr_master_table    = -1;
   s1->orig_centre          = -1;
   s1->orig_sub_centre      = -1;
   s1->upd_seq_no           = -1;
   s1->msg_type             = -1;
   s1->msg_inter_subtype    = -1;
   s1->msg_local_subtype    = -1;
   s1->master_table_version = -1;
   s1->local_table_version  = -1;
   s1->year                 = -1;
   s1->month                = -1;
   s1->day                  = -1;
   s1->hour                 = -1;
   s1->minute               = -1;
   s1->second               = -1;
   }

int main(int argc, const char *argv[])
   {
/*
 * use section 1 as keys for bundleing from header only
 */
   init_sect1_keys( &section1 );

   read_cmdline( argc, argv );

   //Setup for internationalization
   bufr_begin_api();
   setlocale (LC_ALL, "");
   bindtextdomain ("bufr_codec", LOCALEDIR);
   textdomain ("bufr_codec");

   if (argc == 1)
      abort_usage( argv[0] );

   bundle_file( srchkey, nb_key );
   cleanup();
   }

static int bundle_file (BufrDescValue *dvalues, int nbdv)
   {
   BUFR_Dataset  *dts, *dts2;
   int           sscount;
   FILE          *fp, *fpO;
   char           buf[256];
   BUFR_Message  *msg;
   int            rtrn;
   int            count;
   int            pos;
   char           filename[512];
   char           prefix[512], *str;
   BUFR_Tables   *file_tables=NULL;
   BUFR_Tables   *useTables=NULL;
   LinkedList    *tables_list=NULL;
   int            tablenos[2];
/*
   BUFR_Template *tmplt=NULL;
   BUFR_Template *tmplt2=NULL;
*/
   DataSubset    *subset;
   int            i;
/*
 * load CMC Table B and D, currently version 14
 */
   file_tables = bufr_create_tables();
   bufr_load_cmc_tables( file_tables );

/*
 * load all tables into list
 */
   tablenos[0] = 13;
   tables_list = bufr_load_tables_list( getenv("BUFR_TABLES"), tablenos, 1 );
/* 
 * add version 14 to list 
 */
   lst_addfirst( tables_list, lst_newnode( file_tables ) );
/*
 * load local tables if any to list of tables
 */
   bufr_tables_list_addlocal( tables_list, str_ltableb, str_ltabled );
/*
 * open a file for reading
 */
   if (str_ibufr == NULL)
      fp = stdin;
   else
      fp = fopen( str_ibufr, "rb" );
   if (fp == NULL)
      {
      fprintf( stderr, "Error: can't open file \"%s\"\n", str_ibufr );
      exit(-1);
      }

   if (str_obufr == NULL)
      fpO = stdout;
   else
      fpO = fopen( str_obufr, "wb" );
   if (fpO == NULL)
      {
      fprintf( stderr, "Error: can't open file \"%s\"\n", str_obufr );
      exit(-1);
      }

   resolve_search_values( dvalues, nbdv, file_tables );
/*
 * read all messages from the input file
 */
   count = 0;
   dts2 = NULL;
/*
 * go through every report in the file and merge those that fit the pattern into a single bundle
 */
   while ( (rtrn = bufr_read_message( fp, &msg )) > 0 )
      {
      ++count;

/* 
 * apply report header filter if any 
 */
      if ((section1.msg_type < 0)   ||(section1.msg_type == msg->s1.msg_type) &&
         (section1.master_table_version < 0)||(section1.master_table_version == msg->s1.master_table_version)&&
         (section1.orig_centre < 0)||(section1.orig_centre == msg->s1.orig_centre))
         {
/*
 * fallback on default Tables first
 */
         useTables = file_tables;
/* 
 * try to find another if not compatible
 */
         if (useTables->master.version != msg->s1.master_table_version)
            useTables = bufr_use_tables_list( tables_list, msg->s1.master_table_version );
   
         if (useTables != NULL)
            dts = bufr_decode_message( msg, useTables );
         else 
            dts = NULL;
   
         if (dts != NULL)
            {
            if (dts2 == NULL)
               {
               dts2 = bufr_create_dataset( dts->tmplte );
/*
 * transfer section 1 and flag from origin message
 */
               bufr_copy_sect1( &(dts2->s1), &(msg->s1) );
               dts2->data_flag |= msg->s3.flag;
               }
/*
 * only report with same template can be bundled together
 */
            if (bufr_compare_template( dts->tmplte, dts2->tmplte ) == 0)
               {
               sscount = bufr_count_datasubset( dts );
               for (i = 0; i < sscount ; i++)
                  {
                  subset = bufr_get_datasubset( dts, i );
/* 
 * apply descriptor=value  filter if any, otherwise accept everything
 */
                  if (match_search_pattern( subset, dvalues, nbdv ))
                     {
                     pos = bufr_create_datasubset( dts2 );
                     bufr_merge_dataset( dts2, pos, dts, i, 1 );
                     }
                  }
               }
            bufr_free_dataset( dts );
            }
         }
/*
 * discard the message
 */
      bufr_free_message( msg );
      }

/*
 * write the content of cumulated dataset into a file
 */
   if (dts2) 
      {
      if (bufr_count_datasubset( dts2 ) > 0)
         {
         BUFR_Message  *msg2 = bufr_encode_message ( dts2, use_compress );
         bufr_write_message( fpO, msg2 ); 
         bufr_free_message( msg2 );
         }
      bufr_free_dataset( dts2 );
      }
/*
 * close all file and cleanup
 */
   if (str_ibufr != NULL)
      fclose( fp );

   if (str_obufr != NULL)
      fclose( fpO );

   bufr_free_tables_list( tables_list );
   }

static int resolve_search_values( BufrDescValue *dvalues, int nb, BUFR_Tables *tbls )
   {
   int i;
   BufrDescValue *dv;
   EntryTableB   *e;
   char          *str = NULL, *ptr, *tok;
   ArrayPtr       iarr, farr;
   ValueType      vtype;
   int            len;
   long           ival;
   float          fval;

   
   if (nb <= 0) return 0;

   iarr = arr_create( 100, sizeof(int), 100 );
   farr = arr_create( 100, sizeof(float), 100 );
/*
 * convert search key type based descriptor
 */
   for (i = 0; i < nb ; i++)
      {
      dv = &(dvalues[i]);
      e = bufr_fetch_tableB( tbls, dv->descriptor );
      if (e != NULL)
         {
         vtype = bufr_encoding_to_valtype( &(e->encoding) );
         switch( vtype )
            {
            case VALTYPE_STRING : /* already a string */
            break;
            case VALTYPE_INT64 :
            case VALTYPE_INT32  :
               ptr = str = strdup( bufr_value_get_string( dv->values[0], &len ) );
               arr_del ( iarr, arr_count( iarr ) );
               tok = strtok_r( NULL, "\t\n,=", &ptr );
               while ( tok )
                  {
                  ival = atol( tok );
                  arr_add( iarr, (char *)&ival );
                  tok = strtok_r( NULL, "\t\n,", &ptr );
                  }
               bufr_set_key_int32( dv, dv->descriptor, arr_get( iarr, 0 ), arr_count( iarr ) );
               
               free( str );
            break;
            case VALTYPE_FLT64  :
            case VALTYPE_FLT32  :
               ptr = str = strdup( bufr_value_get_string( dv->values[0], &len ) );
               arr_del ( farr, arr_count( farr ) );
               tok = strtok_r( NULL, "\t\n,=", &ptr );
               while ( tok )
                  {
                  fval = atof( tok );
                  arr_add( farr, (char *)&fval );
                  tok = strtok_r( NULL, "\t\n,", &ptr );
                  }
               bufr_set_key_flt32( dv, dv->descriptor, arr_get( farr, 0 ), arr_count( farr ) );
               free( str );
            break;
            default :
            break;
            }
         }
      }
   arr_free( &iarr );
   arr_free( &farr );

   return nb;
   }

static int match_search_pattern ( DataSubset *subset, BufrDescValue *dvalues, int nb )
   {
   int pos;

   if (nb <= 0) return 1;
   pos = bufr_subset_find_values ( subset, dvalues, nb, 0 );
   return ((pos >= 0) ? 1 : 0);
   }
