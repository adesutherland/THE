/***********************************************************************/
/* DIRECTRY.C - Directory routines                                     */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *
 *    The Free Software Foundation, Inc.
 *    675 Mass Ave,
 *    Cambridge, MA 02139 USA.
 *
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling, mark@rexx.org  http://www.rexx.org/
 */


#include "the.h"
#include "proto.h"

#include "directry.h"
#include "thematch.h"

#define NUM_DIRTYPE 5
static ATTR_TYPE curr_dirtype =
       (F_DI | F_AR | F_RO | F_HI | F_SY);
static ATTR_TYPE all_dirtype =
       (F_DI | F_AR | F_RO | F_HI | F_SY);
static CHARTYPE _THE_FAR *dirtype[NUM_DIRTYPE] =
       {(CHARTYPE *)"normal",
       (CHARTYPE *)"readonly",
       (CHARTYPE *)"system",
       (CHARTYPE *)"hidden",
       (CHARTYPE *)"directory"};
static ATTR_TYPE att[NUM_DIRTYPE] =
       {0,F_RO,F_SY,F_HI,F_DI};

/*********************************************************************/
CHARTYPE *make_full(CHARTYPE *path, CHARTYPE *file)
/*********************************************************************/
{
   static CHARTYPE _THE_FAR filebuf[BUFSIZ];
   short pathlen=strlen((DEFCHAR *)path);

   if (pathlen+1+strlen((DEFCHAR *)file)+1 > BUFSIZ)
      return(NULL);
   if (!strcmp((DEFCHAR *)path, "") || !strcmp((DEFCHAR *)path, "."))
   {
      (void) strcpy((DEFCHAR *)filebuf, (DEFCHAR *)file);
      return(filebuf);
   }
   (void) strcpy((DEFCHAR *)filebuf, (DEFCHAR *)path);
   if (*(path+(pathlen - 1)) != ISLASH && *file != ISLASH)
      (void) strcat((DEFCHAR *)filebuf, (DEFCHAR *)ISTR_SLASH);
   (void) strcat((DEFCHAR *)filebuf, (DEFCHAR *)file);
   return(filebuf);
}
/*********************************************************************/
#if !defined(WIN32)
short getfiles(CHARTYPE *path,CHARTYPE *files,struct dirfile **dpfirst,
                                    struct dirfile **dplast)
/*********************************************************************/
{
   DIR *dirp=NULL;
   struct stat sp;
   struct dirent *direntp=NULL;
   struct dirfile *dp=NULL;
   CHARTYPE *full_name=NULL;
   short entries = 10;
   struct tm *timp=NULL;

   dirp = opendir((DEFCHAR *)path);
   if (dirp == NULL)
      return(10);

   dp = *dpfirst = (struct dirfile *)(*the_malloc)(entries * sizeof(struct dirfile));
   if (dp == NULL)
      return(RC_OUT_OF_MEMORY);
   memset(dp, 0, entries*sizeof(struct dirfile) );
   memset(*dpfirst, 0, entries*sizeof(struct dirfile) );
   *dplast = *dpfirst + entries;

   for (direntp = readdir(dirp);direntp != NULL;direntp = readdir(dirp))
   {
      if (thematch( (DEFCHAR *)files, (DEFCHAR *)direntp->d_name,0) == 0)
      {
         if ((full_name = make_full(path,(CHARTYPE *)direntp->d_name)) == NULL)
            return(RC_OUT_OF_MEMORY);
#if defined(HAVE_LSTAT)
         if (lstat((DEFCHAR *)full_name,&sp) != 0)
#else
         if (stat((DEFCHAR *)full_name,&sp) != 0)
#endif
            continue;
         dp->fname_length = strlen(direntp->d_name)+1;
         if ((dp->fname = (CHARTYPE *)(*the_malloc)(dp->fname_length*sizeof(CHARTYPE))) == NULL)
            return(RC_OUT_OF_MEMORY);
         strcpy((DEFCHAR *)dp->fname,direntp->d_name);
         dp->fattr = sp.st_mode;
#if defined(_HPUX_SOURCE)
         dp->facl  = sp.st_acl;
#else
         dp->facl  = 0;
#endif
         timp = localtime(&(sp.st_mtime));
         dp->f_hh = HH_MASK(timp);
         dp->f_mi = MI_MASK(timp);
         dp->f_ss = SS_MASK(timp);
         dp->f_dd = DD_MASK(timp);
         dp->f_mm = MM_MASK(timp);
         dp->f_yy = YY_MASK(timp);
         dp->fsize = sp.st_size;
         dp->lname = NULL;
         /*
          * If we have the lstat() function, and the current file
          * is a symbolic link, go and get the filename the symbolic
          * link points to...
          */
#if defined(S_ISLNK) && defined(HAVE_READLINK)
         if (S_ISLNK(dp->fattr))
         {
            char buf[MAX_FILE_NAME+1];
            int rc=0;

            rc = readlink((DEFCHAR *)full_name,buf,sizeof(buf));
            if (rc != (-1))
            {
               if ((dp->lname = (CHARTYPE *)(*the_malloc)((rc+1)*sizeof(CHARTYPE))) == NULL)
                  return(RC_OUT_OF_MEMORY);
               memcpy((DEFCHAR *)dp->lname,buf,rc);
               dp->lname[rc] = '\0';
            }
         }
#endif
         dp++;
         if (dp == *dplast)
         {
            *dpfirst = (struct dirfile *)(*the_realloc)((CHARTYPE *)*dpfirst,
                        2 * entries * sizeof (struct dirfile));
            if (*dpfirst == NULL)
               return(RC_OUT_OF_MEMORY);
            dp = *dpfirst + entries;
            *dplast = dp + entries;
            entries *= 2;
         }
      }
   }
   closedir(dirp);
   *dplast = dp;
   return(0);
}
#else
/*********************************************************************/
short getfiles(CHARTYPE *path,CHARTYPE *files,struct dirfile **dpfirst,
                                    struct dirfile **dplast)
/*********************************************************************/
{
   struct dirfile *dp=NULL;
   FSTR_TYPE ffblk;
   ATTR_TYPE attrs=curr_dirtype;
   DONE_TYPE done=0;
   intptr_t handle=0;
   struct tm *local=NULL;
   CHARTYPE *full_path=NULL;
   short entries = 10;

 if ((full_path = make_full(path,"*.*")) == NULL)
    return(RC_FILE_NOT_FOUND);

   ffblk.attrib = attrs;
   handle = _findfirst((DEFCHAR *)full_path,&ffblk);
   if (handle == (-1))
      done = (-1);
   if (done != 0)
      return(RC_FILE_NOT_FOUND);

   dp = *dpfirst = (struct dirfile *)(*the_malloc)(entries * sizeof (struct dirfile));
   if (dp == NULL)
      return(RC_OUT_OF_MEMORY);
   memset(*dpfirst, 0, entries*sizeof(struct dirfile) );
   *dplast = *dpfirst + entries;

   while(!done)
   {
      local = localtime(&ffblk.time_write);
      if (thematch((DEFCHAR *)files,(DEFCHAR *)ffblk.NAME_NAME,FNM_IGNORECASE) == 0)
      {
         dp->fname_length = strlen((DEFCHAR *)ffblk.NAME_NAME)+1;
         if ((dp->fname = (CHARTYPE *)(*the_malloc)(dp->fname_length*sizeof(CHARTYPE))) == NULL)
            return(RC_OUT_OF_MEMORY);
         strcpy((DEFCHAR *)dp->fname,(DEFCHAR *)ffblk.NAME_NAME);
         dp->fattr = ffblk.ATTR_NAME;
         dp->f_hh = HH_MASK(ffblk.TIME_NAME);
         dp->f_mi = MI_MASK(ffblk.TIME_NAME);
         dp->f_ss = SS_MASK(ffblk.TIME_NAME);
         dp->f_dd = DD_MASK(ffblk.DATE_NAME);
         dp->f_mm = MM_MASK(ffblk.DATE_NAME);
         dp->f_yy = YY_MASK(ffblk.DATE_NAME);
         dp->fsize = ffblk.SIZE_NAME;
         dp->lname = NULL;
         dp->facl  = 0;
         dp++;
         if (dp == *dplast)
         {
            *dpfirst = (struct dirfile *)(*the_realloc)((CHARTYPE *)*dpfirst,
                       2 * entries * sizeof (struct dirfile));
            if (*dpfirst == NULL)
               return(RC_FILE_NOT_FOUND);
            dp = *dpfirst + entries;
            *dplast = dp + entries;
            entries *= 2;
         }
      }
      done = _findnext(handle,&ffblk);
   }
   *dplast = dp;
   _findclose(handle);
   return(RC_OK);
}
#endif

/*********************************************************************/
int date_compare(struct dirfile *first,struct dirfile *next)
/*********************************************************************/
{
   if (first->f_yy > next->f_yy)
      return(1);
   else if (first->f_yy < next->f_yy)
      return(-1);
   if (first->f_mm > next->f_mm)
      return(1);
   else if (first->f_mm < next->f_mm)
      return(-1);
   if (first->f_dd > next->f_dd)
      return(1);
   else if (first->f_dd < next->f_dd)
      return(-1);
   return(0);
}
/*********************************************************************/
int time_compare(struct dirfile *first,struct dirfile *next)
/*********************************************************************/
{
   if (first->f_hh > next->f_hh)
      return(1);
   else if (first->f_hh < next->f_hh)
      return(-1);
   if (first->f_mi > next->f_mi)
      return(1);
   else if (first->f_mi < next->f_mi)
      return(-1);
   if (first->f_ss > next->f_ss)
      return(1);
   else if (first->f_ss < next->f_ss)
      return(-1);
   return(0);
}

/*********************************************************************/
int date_comp( const void *in_first, const void *in_next )
/*********************************************************************/
{
   int rc=0;
   struct dirfile *first=(struct dirfile *)in_first,*next=(struct dirfile *)in_next;

   rc = date_compare( first, next );
   if (rc == 0)
      rc = time_compare( first, next );
   if (rc == 0)
#if defined(WIN32)
      rc = my_stricmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#else
      rc = strcmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#endif
   if (rc == 0)
      return(0);
   if (DIRORDERx == DIRSORT_DESC)
      rc = (rc > 0) ? (-1) : 1;
   return(rc);
}

/*********************************************************************/
int time_comp( const void *in_first, const void *in_next )
/*********************************************************************/
{
   int rc=0;
   struct dirfile *first=(struct dirfile *)in_first,*next=(struct dirfile *)in_next;

   rc = time_compare(first,next);
   if (rc == 0)
#if defined(WIN32)
      rc = my_stricmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#else
      rc = strcmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#endif
   if (rc == 0)
      return(0);
   if (DIRORDERx == DIRSORT_DESC)
      rc = (rc > 0) ? (-1) : 1;
   return(rc);
}

/*********************************************************************/
int dir_comp( const void *in_first, const void *in_next )
/*********************************************************************/
{
   int first_dir=0;
   int next_dir=0;
   int rc=0;
   struct dirfile *first=(struct dirfile *)in_first,*next=(struct dirfile *)in_next;

   first_dir=is_a_dir_dir(first->fattr);
   next_dir=is_a_dir_dir(next->fattr);
   if (first_dir && !next_dir)
      rc = (-1);
   if (!first_dir && next_dir)
      rc = 1;
   if (rc == 0)
#if defined(WIN32)
      rc = my_stricmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#else
      rc = strcmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#endif
   if (rc == 0)
      return(0);
   if (DIRORDERx == DIRSORT_DESC)
      rc = (rc > 0) ? (-1) : 1;
   return(rc);
}

/*********************************************************************/
int size_comp( const void *in_first, const void *in_next )
/*********************************************************************/
{
   int rc=0;
   struct dirfile *first=(struct dirfile *)in_first,*next=(struct dirfile *)in_next;

   if (first->fsize > next->fsize)
      rc = 1;
   else
   {
      if (first->fsize < next->fsize)
         rc = -1;
   }
   if (rc == 0)
#if defined(WIN32)
      rc = my_stricmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#else
      rc = strcmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#endif
   if (rc == 0)
      return(0);
   if (DIRORDERx == DIRSORT_DESC)
      rc = (rc > 0) ? (-1) : 1;
   return(rc);
}

/*********************************************************************/
int name_comp( const void *in_first, const void *in_next )
/*********************************************************************/
{
   int rc=0;
   struct dirfile *first=(struct dirfile *)in_first,*next=(struct dirfile *)in_next;

#if defined(WIN32)
   rc = my_stricmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#else
   rc = strcmp((DEFCHAR *)first->fname,(DEFCHAR *)next->fname);
#endif
   if (rc == 0)
      return(0);
   if (DIRORDERx == DIRSORT_DESC)
      rc = (rc > 0) ? (-1) : 1;
   return(rc);
}
/*********************************************************************/
CHARTYPE *file_date(struct dirfile *date,CHARTYPE *str_date)
/*********************************************************************/
{
   static CHARTYPE _THE_FAR *mon[12] =
   { (CHARTYPE *)"Jan",(CHARTYPE *)"Feb",(CHARTYPE *)"Mar",(CHARTYPE *)"Apr",(CHARTYPE *)"May",(CHARTYPE *)"Jun",(CHARTYPE *)"Jul",(CHARTYPE *)"Aug",(CHARTYPE *)"Sep",(CHARTYPE *)"Oct",(CHARTYPE *)"Nov",(CHARTYPE *)"Dec"};
   sprintf((DEFCHAR *)str_date,"%2d-%3.3s-%4.4d",date->f_dd,mon[date->f_mm],date->f_yy);
   return(str_date);
}
/*********************************************************************/
CHARTYPE *file_time(struct dirfile *time,CHARTYPE *str_time)
/*********************************************************************/
{
   sprintf((DEFCHAR *)str_time,"%2d:%2.2d",time->f_hh,time->f_mi);
   return(str_time);
}
/*********************************************************************/
CHARTYPE *file_attrs(ATTR_TYPE attrs,CHARTYPE *str_attr,int facl)
/*********************************************************************/
{
#if !defined(WIN32)
#if 0
   ATTR_TYPE ftype=(attrs & S_IFMT);
#else
   ATTR_TYPE ftype=attrs;
#endif

   str_attr[11] = '\0';
   str_attr[10] = ' ';
   if (facl)
      str_attr[10] = '+';
   str_attr[0] = '-';
#ifdef S_ISDIR
   if ( S_ISDIR( ftype) )
      str_attr[0] = 'd';
#endif
#ifdef S_ISCHR
   if ( S_ISCHR( ftype) )
      str_attr[0] = 'c';
#endif
#ifdef S_ISBLK
   if ( S_ISBLK( ftype) )
      str_attr[0] = 'b';
#endif
#ifdef S_ISFIFO
   if ( S_ISFIFO( ftype) )
      str_attr[0] = 'p';
#endif
#ifdef S_ISLNK
   if ( S_ISLNK( ftype) )
      str_attr[0] = 'l';
#endif
#ifdef S_ISSOCK
   if ( S_ISSOCK( ftype) )
      str_attr[0] = 's';
#endif
   str_attr[1] = (attrs & S_IRUSR) ? 'r' : '-';
   str_attr[2] = (attrs & S_IWUSR) ? 'w' : '-';
   str_attr[3] = (attrs & S_IXUSR) ? 'x' : '-';
#ifdef S_ISUID
   str_attr[3] = (attrs & S_ISUID) ? 's' : str_attr[3];
#endif
   str_attr[4] = (attrs & S_IRGRP) ? 'r' : '-';
   str_attr[5] = (attrs & S_IWGRP) ? 'w' : '-';
   str_attr[6] = (attrs & S_IXGRP) ? 'x' : '-';
#ifdef S_ISGID
   str_attr[6] = (attrs & S_ISGID) ? 's' : str_attr[6];
#endif
   str_attr[7] = (attrs & S_IROTH) ? 'r' : '-';
   str_attr[8] = (attrs & S_IWOTH) ? 'w' : '-';
   str_attr[9] = (attrs & S_IXOTH) ? 'x' : '-';
#else
   strcpy(str_attr,".... ");
   if ((attrs & F_RO) == F_RO)
      str_attr[0] = 'r';
   if ((attrs & F_AR) == F_AR)
      str_attr[1] = 'a';
   if ((attrs & F_SY) == F_SY)
      str_attr[2] = 's';
   if ((attrs & F_HI) == F_HI)
      str_attr[3] = 'h';
   str_attr[4] = '\0';
   if ((attrs & F_DI) == F_DI)
      strcat(str_attr,"(dir)  ");
   else
      strcat(str_attr,"       ");
#endif
   return(str_attr);
}
/*********************************************************************/
short set_dirtype(CHARTYPE *params)
/***********************************************************************/
{
   CHARTYPE *p=NULL;
   register short i=0;
   ATTR_TYPE attribs=0;
   bool found=FALSE;

   if (strcmp((DEFCHAR *)params,"") == 0)      /* error if no paramaters */
   {
      display_error(3,(CHARTYPE *)"",FALSE);
      return(RC_INVALID_OPERAND);
   }
   if (strcmp((DEFCHAR *)params,"*") == 0)                 /* set to ALL */
   {
      curr_dirtype = (F_RO | F_HI | F_SY | F_DI | F_AR);
      return(RC_OK);
   }
   p = (CHARTYPE *)strtok((DEFCHAR *)params," ");
   while(p != NULL)
   {
      found = FALSE;
      for (i=0;i<NUM_DIRTYPE;i++)
      {
         if (equal(dirtype[i],p,1))
         {
            found = TRUE;
            attribs |= att[i];
         }
      }
      if (!found)
      {
         display_error(1,(CHARTYPE *)p,FALSE);
         return(RC_INVALID_OPERAND);
      }
      p = (CHARTYPE *)strtok(NULL," ");
   }
   curr_dirtype = attribs;
   return(RC_OK);
}

/*********************************************************************/
CHARTYPE *get_dirtype(CHARTYPE *buf)
/***********************************************************************/
{
   if (curr_dirtype == all_dirtype)                 /* all masks enabled */
   {
      strcpy((DEFCHAR*)buf,"*");
   }
   else
   {
      strcpy((DEFCHAR*)buf,"normal");
      if (curr_dirtype & F_DI)
         strcat((DEFCHAR*)buf," directory");
      if (curr_dirtype & F_RO)
         strcat((DEFCHAR*)buf," readonly");
      if (curr_dirtype & F_HI)
         strcat((DEFCHAR*)buf," hidden");
      if (curr_dirtype & F_SY)
         strcat((DEFCHAR*)buf," system");
   }
   return(buf);
}
