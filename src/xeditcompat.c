/* XEDIT macro controls use the shared text, view and logical cursor model. */
#include <the.h>
#include <proto.h>
#include <query.h>

extern CHARTYPE query_num1[50],query_num2[50];

LENGTHTYPE xedit_truncated_length(FILE_DETAILS *file,const CHARTYPE *text,LENGTHTYPE len)
{
   if (!file->truncation_column) return len;
#ifdef USE_UTF8
   return textpos_from_cell(text,len,file->truncation_column,TEXT_SNAP_BACKWARD).byte_offset;
#else
   return min(len,file->truncation_column);
#endif
}

short extract_trunc(short count,short item,CHARTYPE *args,CHARTYPE type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
{
   sprintf((char *)query_num1,"%d",CURRENT_FILE->truncation_column ? CURRENT_FILE->truncation_column : max_line_length);
   item_values[1].value=query_num1;
   item_values[1].len=strlen((char *)query_num1);
   return 1;
}

short Mask(CHARTYPE *params)
{
   CHARTYPE *word[3],strip[2]={STRIP_BOTH,STRIP_NONE};
   short count=param_split(params,word,2,WORD_DELIMS,TEMP_PARAM,strip,FALSE);
   size_t len;
   if (count<1) { display_error(3,(CHARTYPE *)"",FALSE); return RC_INVALID_OPERAND; }
   if (equal((CHARTYPE *)"immed",word[0],1))
   {
      len=strlen((char *)word[1]);
      if (len>=sizeof(CURRENT_FILE->input_mask) || len>(size_t)max_line_length)
      { display_error(6,word[1],FALSE); return RC_INVALID_OPERAND; }
      memcpy(CURRENT_FILE->input_mask,word[1],len+1);
      CURRENT_FILE->input_mask_len=(LENGTHTYPE)len;
      return RC_OK;
   }
   if ((equal((CHARTYPE *)"define",word[0],1) || equal((CHARTYPE *)"modify",word[0],1)) && count==1 && !batch_only)
   {
      static const char prefix[]="set mask immed ";
      size_t i,width;
      if (equal((CHARTYPE *)"modify",word[0],1))
      {
         len=CURRENT_FILE->input_mask_len;
         if (len+sizeof(prefix)>(size_t)max_line_length)
            return RC_INVALID_OPERAND;
         memcpy(cmd_rec+sizeof(prefix)-1,CURRENT_FILE->input_mask,len);
      }
      else
      {
         width=CURRENT_SCREEN.cols[WINDOW_COMMAND];
         len=width>sizeof(prefix) ? width-sizeof(prefix) : 1;
         for (i=0;i<len;i++)
            cmd_rec[sizeof(prefix)-1+i]=(i+1)%10==0 ? '0'+((i+1)/10)%10 : (i+1)%5==0 ? '+' : '.';
      }
      memcpy(cmd_rec,prefix,sizeof(prefix)-1);
      cmd_rec_len=sizeof(prefix)-1+len;
      cmd_rec[cmd_rec_len]=0;
      return THEcursor_cmdline(current_screen,CURRENT_VIEW,sizeof(prefix));
   }
   display_error(1,word[0],FALSE);
   return RC_INVALID_OPERAND;
}

CHARTYPE *xedit_mask_line(FILE_DETAILS *file,const CHARTYPE *text,LENGTHTYPE len,LENGTHTYPE *result_len)
{
   size_t offset=(size_t)min(len,file->input_mask_len),padding=0,total;
   CHARTYPE *result;
#ifdef USE_UTF8
   int cells=textpos_from_byte(text,len,len).cell_column;
   TextPos position=textpos_from_cell(file->input_mask,file->input_mask_len,cells,TEXT_SNAP_BACKWARD);
   if (position.cell_column<cells && position.byte_offset<(size_t)file->input_mask_len)
      position=textpos_cluster_at_boundary(file->input_mask,file->input_mask_len,position).end;
   offset=position.byte_offset;
   if (position.cell_column>cells)
      padding=(size_t)(position.cell_column-cells);
#endif
   total=(size_t)len+padding+(size_t)file->input_mask_len-offset;
   if (total>(size_t)max_line_length) return NULL;
   result=(*the_malloc)(total+1);
   if (!result) return NULL;
   memcpy(result,text,len);
   memset(result+len,' ',padding);
   memcpy(result+len+padding,file->input_mask+offset,file->input_mask_len-offset);
   result[total]=0;
   *result_len=(LENGTHTYPE)total;
   return result;
}

short extract_mask(short count,short item,CHARTYPE *args,CHARTYPE type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
{
   item_values[1].value=CURRENT_FILE->input_mask;
   item_values[1].len=CURRENT_FILE->input_mask_len;
   return 1;
}

short extract_flscreen(short count,short item,CHARTYPE *args,CHARTYPE type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
{
   LINETYPE first=0,last=0;
   short row;
   if (batch_only) return 0;
   build_screen(current_screen);
   for (row=0;row<CURRENT_SCREEN.rows[WINDOW_FILEAREA];row++)
   {
      SHOW_LINE *line=&CURRENT_SCREEN.sl[row];
      if (line->line_type==LINE_LINE && line->line_number>0 && line->line_number<=CURRENT_FILE->number_lines)
      {
         if (!first) first=line->line_number;
         last=line->line_number;
      }
   }
   sprintf((char *)query_num1,"%ld",first);
   sprintf((char *)query_num2,"%ld",last);
   item_values[1].value=query_num1; item_values[1].len=strlen((char *)query_num1);
   item_values[2].value=query_num2; item_values[2].len=strlen((char *)query_num2);
   return 2;
}

static unsigned int cursor_depth;
static int winning_priority=-1;
static VIEW_DETAILS *winning_view;
static FILE_DETAILS *winning_file;
static LINE *winning_line;
static LogicalCursor winning_cursor;

void xedit_cursor_begin(void)
{
   if (!cursor_depth++) { winning_priority=-1; winning_view=NULL; winning_line=NULL; }
}

bool xedit_cursor_accept(int priority)
{
   if (!cursor_depth) { winning_priority=-1; winning_view=NULL; }
   if (compatible_feel!=COMPAT_XEDIT) return TRUE;
   return priority<0 || priority>winning_priority;
}

void xedit_cursor_record(int priority)
{
   if (compatible_feel!=COMPAT_XEDIT || !CURRENT_VIEW) return;
   winning_priority=priority<0 ? 256 : priority;
   winning_view=CURRENT_VIEW;
   winning_file=CURRENT_FILE;
   winning_cursor=CURRENT_VIEW->logical_cursor.current;
   winning_line=lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,
                         winning_cursor.line_number,CURRENT_FILE->number_lines);
}

int xedit_cursor_priority(void) { return winning_priority>255 ? -1 : winning_priority; }

void xedit_cursor_end(void)
{
   LINE *line;
   LINETYPE number;
   if (!cursor_depth || --cursor_depth) return;
   if (!number_of_files || CURRENT_VIEW!=winning_view || CURRENT_FILE!=winning_file || !winning_cursor.valid) return;
   if (winning_cursor.zone==LOGICAL_CURSOR_ZONE_COMMAND)
      (void)THEcursor_cmdline(current_screen,CURRENT_VIEW,winning_cursor.text.cell_column+1);
   else
   {
      for (line=CURRENT_FILE->first_line,number=0;line;line=line->next,number++)
         if (line==winning_line)
         {
            (void)THEcursor_goto(number,winning_cursor.text.cell_column+1);
            if (winning_cursor.zone==LOGICAL_CURSOR_ZONE_PREFIX)
               (void)Sos_prefix((CHARTYPE *)"");
            break;
         }
   }
}
