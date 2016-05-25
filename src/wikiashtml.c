/*
 *      wikiashtml.c
 *
 *      Copyright 2010 jp <inphilly@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */
/************************************************************************/
/*  Here we translate the page in html and 'print' it.
 * The main function for this job is wiki_print_data_as_html()
 * HttpResponse *res contains the vars passed to the server.
 * char *raw_page_data contains the text to translate.
 * int autorized is TRUE if logged.
 * char *page is the page name.
 */

#include "didi.h"

/* local variable */
int pic_width=0, pic_height=0, pic_border=0;
int expand_collapse_num=0;
int target_wwwlink=0;

typedef struct {
  HttpResponse *res;
  char *page;
  char *page_data;  /* accumulates non marked up text */
  string_t line;
  string_t htmlbuf;
  struct {
    unsigned pre_on;
    unsigned form_on;
    unsigned bold_on;
    unsigned code_on;
    unsigned table_on;
    unsigned color_on;
    unsigned bgcolor_on;
    unsigned italic_on;
    unsigned underline_on;
    unsigned highlight_on;
    unsigned strikethrough_on;
    unsigned int form_cnt;
  };
} wiki_ctx_t;

typedef struct {
  const char *start;
  const char *end;
} element_t;

static element_t element_bold = { "<b>", "</b>" };
static element_t element_strikethrough = { "<del>", "</del>" };
static element_t element_underline = { "<u>", "</u>" };
static element_t element_highlight = { "<span style=\"background: #ffff00\">", "</span>" };
static element_t element_code = { "<code>", "</code>" };

static char *
get_line_from_string(char **lines, int *line_len)
{
  int   i;
  char *z = *lines;

  if( z[0] == '\0' ) return NULL;

  for (i=0; z[i]; i++)
  {
      if (z[i] == '\n')
      {
        if (i > 0 && z[i-1]=='\r')
        { z[i-1] = '\0'; }
        else
        { z[i] = '\0'; }
      i++;
      break;
     }
  }

  /* advance lines on */
  *lines      = &z[i];
  *line_len -= i;

  return z;
}

static char*
check_for_link(char *line, int *skip_chars)
{
  char *start =  line;
  char *p     =  line;
  char *q;
  char *url   =  NULL;
  char *title =  NULL;
  char *result = NULL;
  int   found = 0;
  int  w,h;
  int  wwwlink;
  char border_pic_str[32]="",width_pic_str[32]="",height_pic_str[32]="";

  if (*p == '[')
  /* [link] or [link title] or [image link] */
    {
      /* XXX TODO XXX
       * Allow links like [the Main page] ( covert to the_main_page )
       */
      url = start+1; *p = '\0'; p++;
      while (  *p != ']' && *p != '\0' && !isspace(*p) ) p++;

    if (isspace(*p)) /* there is a title or image */
    {
      *p = '\0';
      title = ++p;
      while (  *p != ']' && *p != '\0' ) /* search closing braket */
        p++;
    }
      *p = '\0';
      p++;
    }
    else if ( !strncasecmp(p, "file=", 5) )
    /* file to dowload */
    {
      q=p;
      start=p+5;
      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;
      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
       "<a href='%s'>%s</a>",url,url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "youtube=", 8) )
    /* youtube video embedded */
    {
      q=p;
      start=p+8;
      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;
      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object width=\"480\" height=\"385\"><param name=\"movie\" value=\"%s\">"
      "</param><param name=\"allowFullScreen\" value=\"true\"></param><param name=\"allowscriptaccess\" "
      "value=\"always\"></param><embed src=\"%s\" type=\"application/x-shockwave-flash\" "
      "allowscriptaccess=\"always\" allowfullscreen=\"true\" width=\"480\" height=\"385\">"
      "</embed></object>",
      url,url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "dailymotion=", 12) )
    /* dailymotion video embedded */
    {
      q=p;
      start=p+12;
      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;
      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object width=\"480\" height=\"275\"><param name=\"movie\" value=\"%s\">"
      "</param><param name=\"allowFullScreen\" value=\"true\"></param><param name=\"allowScriptAccess\" "
      "value=\"always\"></param><embed src=\"%s\" width=\"480\" height=\"275\" allowfullscreen=\"true\" "
      "allowscriptaccess=\"always\"></embed></object>",
      url,url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "vimeo=", 6) )
    /* vimeo video embedded */
    {
      q=p;
      start=p+6;
      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;
      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object width=\"400\" height=\"225\"><param name=\"allowfullscreen\" value=\"true\" />"
      "<param name=\"allowscriptaccess\" value=\"always\" /><param name=\"movie\" "
      "value=\"%s&amp;server=vimeo.com&amp;show_title=1&amp;show_byline=1&amp;show_portrait=0&amp;color=&amp;fullscreen=1\" />"
      "<embed src=\"%s&amp;server=vimeo.com&amp;show_title=1&amp;show_byline=1&amp;show_portrait=0&amp;color=&amp;fullscreen=1\" "
      "type=\"application/x-shockwave-flash\" allowfullscreen=\"true\" allowscriptaccess=\"always\" width=\"400\" height=\"225\">"
      "</embed></object>",
      url,url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "veoh=", 5) )
    /* veoh video embedded */
    {
      q=p;
      start=p+5;
      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;

      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object width=\"410\" height=\"341\" id=\"veohFlashPlayer\" name=\"veohFlashPlayer\">"
      "<param name=\"movie\" value=\"%s&player=videodetailsembedded&videoAutoPlay=0&id=anonymous\">"
      "</param><param name=\"allowFullScreen\" value=\"true\">"
      "</param><param name=\"allowscriptaccess\" value=\"always\">"
      "</param><embed src=\"%s&player=videodetailsembedded&videoAutoPlay=0&id=anonymous\" "
      "type=\"application/x-shockwave-flash\" allowscriptaccess=\"always\" allowfullscreen=\"true\" "
      "width=\"410\" height=\"341\" id=\"veohFlashPlayerEmbed\" name=\"veohFlashPlayerEmbed\">"
      "</embed></object>",
      url, url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "flash=", 6) )
    /* generic flash video embedded */
    {
      q=p;
      start=p+6;
      if ( pic_width > 100) w=pic_width;
        else w=320;
      if ( pic_height > 100) h=pic_height;
        else h=240;

      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;

      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object type=\"application/x-shockwave-flash\" data=\"%s\" width=\"%i\" height=\"%i\">"
      "<param name=\"movie\" value=\"%s\"></object>",
      url, w, h, url);
      *skip_chars = p - start;
      return result;
    }
    else if ( !strncasecmp(p, "swf=", 4) )
    /* swf flash animation embedded */
    {
      q=p;
      start=p+4;
      if ( pic_width > 100) w=pic_width;
        else w=320;
      if ( pic_height > 100) h=pic_height;
        else h=240;

      while ( *p != '\0' && !isspace(*p) && *p != '|' ) p++;

      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      start=q;
      *start = '\0';
      asprintf(&result,
      "<object type=\"application/x-shockwave-flash\" data=\"%s\" width=\"%i\" height=\"%i\">",
        url, w, h);
      *skip_chars = p - start;
      return result;
    }
    else if (!strncasecmp(p, "http://", 7)
       || !strncasecmp(p, "https://", 8)
       || !strncasecmp(p, "mailto://", 9)
       || !strncasecmp(p, "file://", 7))
  /* external link */
    {
      while ( *p != '\0' && !isspace(*p) ) p++;
      found = 1;
    }
  else if (isupper(*p))         /* Camel-case */
    {
      int num_upper_char = 1;
      p++;
      while ( *p != '\0' && isalnum(*p) )
      {
        if (isupper(*p))
        { found = 1; num_upper_char++; }
        p++;
      }

      if (num_upper_char == (p-start)) /* Dont make ALLCAPS links */
        return NULL;
    }

  if (found)  /* cant really set http/camel links in place */
  {
      url = malloc(sizeof(char) * ((p - start) + 2) );
      memset(url, 0, sizeof(char) * ((p - start) + 2));
      strncpy(url, start, p - start);
      *start = '\0';
  }

  if (url != NULL)
  {
      int len = strlen(url);

      *skip_chars = p - start;

      /* url is an image ? */
      if (!strncmp(url+len-4, ".gif", 4) || !strncmp(url+len-4, ".png", 4)
      || !strncmp(url+len-4, ".jpg", 4) || !strncmp(url+len-5, ".jpeg", 5))
      {
          if (pic_width)
            sprintf(width_pic_str," width=\"%i\"",pic_width);
          else
            width_pic_str[0]='\0';

          if (pic_height)
            sprintf(height_pic_str," height=\"%i\"",pic_height);
          else
            height_pic_str[0]='\0';

          if (pic_border)
            sprintf(border_pic_str," border=\"%i\"",pic_border);
          else
            border_pic_str[0]='\0';

          if (title) /*  case: [image link] */
            asprintf(&result, "<a href=\"%s\"><img src=\"%s\"%s%s%s></a>",
                 title, url, border_pic_str, width_pic_str, height_pic_str);
          else /*  case: http://link_to_image */
            asprintf(&result, "<img src=\"%s\"%s%s%s>",
                url, border_pic_str, width_pic_str, height_pic_str);
      }
      else /*  url or title does'nt link to an image */
      {
          char *extra_attr = "";

          wwwlink=0;
          if (!strncasecmp(url, "http://", 7) || !strncasecmp(url, "https://", 8))
          {
            extra_attr = " title='WWW link' class='externallink'";
            wwwlink=1;
          }

          if (target_wwwlink && wwwlink)
          {
            if (title)
              asprintf(&result,"<a %s href='%s' target='_blank'>%s</a>", extra_attr, url, title);
            else
              asprintf(&result, "<a %s href='%s' target='_blank'>%s</a>", extra_attr, url, url);
          }
          else
          {
            if (title)
              asprintf(&result,"<a %s href='%s'>%s</a>", extra_attr, url, title);
            else
              asprintf(&result, "<a %s href='%s'>%s</a>", extra_attr, url, url);
          }
      }

      return result;
  }

  return NULL;
}

static int
is_wiki_format_char_or_space(char c)
{
  if (isspace(c)) return 1;
  if (strchr(">|/*_-", c)) return 1;
  return 0;
}


void
wiki_parse_between_braces(char *s)
{
    char *str_ptr;

    if ( (str_ptr=strstr(s,"width=")) != NULL )
    {
      str_ptr+=6;
      pic_width=atoi(str_ptr);
    }
    /* else pic_width=0; */

    if ( (str_ptr=strstr(s,"height=")) != NULL )
    {
      str_ptr+=7;
      pic_height=atoi(str_ptr);
    }
    /* else pic_height=0; */

    if ( (str_ptr=strstr(s,"border=")) != NULL )
    {
      str_ptr+=7;
      pic_border=atoi(str_ptr);
    }
    /* else pic_border=0; */

    if ( strstr(s,"picture=default") != NULL )
    {
      pic_width=0;
      pic_height=0;
      pic_border=0;
    }

    if ( strstr(s,"wwwlink=new_tag") != NULL )
      target_wwwlink=1;
    else if ( strstr(s,"wwwlink=current_tag") != NULL )
      target_wwwlink=0;

}

static void
print_element(wiki_ctx_t *ctx, element_t *el, unsigned *flag)
{
  if (ctx->line.s != ctx->line.pos
      && !is_wiki_format_char_or_space(*(ctx->line.pos - 1))
      && !*flag) {
    ctx->line.pos++;
  } else if (isspace(*(ctx->line.pos + 1)) && !*flag) {
    ctx->line.pos++;
  } else {
    *ctx->line.pos = '\0';
    http_response_printf(ctx->res, "%s%s", util_htmlize(ctx->page_data, &ctx->htmlbuf),
	*flag ? el->end : el->start);
    *flag ^= 1;				  /* toggle flag */
    ctx->page_data = ctx->line.pos + 1;	  /* advance page_data to next line */
  }
}
/**
 * If you use several conflicting options like <(:)>, the last option wins.
 *
 * See: http://wikiwikiweb.de/HelpOnTables
 *
 * What we do so far:
 *
 *  <|x> rowspan=x
 *  <-x> colspan=x
 *  <^>  valign=top
 *  <v>  valign=bottom
 *  <)>  align=right
 *  <(>  align=left
 *  <:>  align=center
 *  <#XXXXXX> bgcolor="#XXXXXX"
 *  <XX%> width="XX%"
 *
 * and any combination of it (if it makes sense) see above.
**/
static int parse_table_attributes(const char *s, char *buf, size_t bufsize)
{
  const char *start = s;
  char *end, *align = NULL, *valign = NULL;
  int i, ret = 0, colspan = 0, rowspan = 0, bgcolor = 0, width = 0;;
  size_t n = 0;

  if ('<' != *s) {
    /* no attributes at all */
  } else {
    *buf = '\0';
    s++;
    while (*s) {
      switch (*s++) {
	case '>':
	  ret = s - start;
	  goto endofstyle;
	case ';':
	  /* separator, ignore */
	  break;
	case ':':
	  align = "center";
	  break;
	case '(':
	  align = "left";
	  break;
	case ')':
	  align = "right";
	  break;
	case '^':
	  valign = "top";
	  break;
	case 'v':
	  valign = "bottom";
	  break;
	case '-':
	  colspan = strtol(s, &end, 10);
	  s = end;
	  break;
	case '|':
	  rowspan = strtol(s, &end, 10);
	  s = end;
	  break;
	case '#':
	  bgcolor = strtol(s, &end, 16);
	  s = end;
	  break;
	case '1' ... '9':
	  if ((i = strtol(s - 1, &end, 10)) <= 100 && '%' == *end) {
	    width = i;
	    s = end + 1;
	    break;
	  }
	  /* fall through */
	default:
	  ;
      }
    }

endofstyle:

    if (NULL != align) {
      n += snprintf(buf + n, bufsize - n, "align=\"%s\" ", align);
    }
    if (NULL != valign) {
      n += snprintf(buf + n, bufsize - n, "valign=\"%s\" ", valign);
    }
    if (0 != colspan) {
      n += snprintf(buf + n, bufsize - n, "colspan=%d ", colspan);
    }
    if (0 != rowspan) {
      n += snprintf(buf + n, bufsize - n, "rowspan=%d ", rowspan);
    }
    if (0 != bgcolor) {
      n += snprintf(buf + n, bufsize - n, "bgcolor=\"#%06x\" ", bgcolor);
    }
    if (0 != width) {
      n += snprintf(buf + n, bufsize - n, "width=\"%d%%\" ", width);
    }
    /* cosmetic, remove trailing space */
    if (n && n <= bufsize) {
      buf[n - 1] = '\0';
    }
  }
  return ret;
}

void
prepare_toc(char **sectionlist,char *raw_page_data)
/* create a list of header, will be used by {{toc}} */
{
  int header = 0;
  int i = 0;
  char *p = raw_page_data;

  int lg = 1000;
  *sectionlist = malloc(lg); /* we can need to realloc more */
  while ( *p != '\0' )
  {
    /* header is '=' at the beginning of the line */
    if ( *p == '=' && ( p == raw_page_data || *(p-1) == '\n' || *(p-1) == '\r' ) )
    {
      /* get chars until eol */
      while (*p != '\0' && *p != '\n' && *p != '\r' )
      {
        if (i > lg-3)
        {
          lg+= 1000;
          *sectionlist = realloc(*sectionlist, lg);
        }

        (*sectionlist)[i++] = *p;
        p++;
      }
      (*sectionlist)[i++] = '\n';
      header++;
    }
    else p++;
  }
  (*sectionlist)[i] = '\0';
  return;
}

static void
print_toc(HttpResponse *res, char *sectionlist)
{
  int sectioncnt = 0;
  char *eol;

  http_response_printf(res, "<div id=\"toc\">\n");
  while ((eol = strchr(sectionlist,'\n'))) {
    int depth = 0;
    *eol = '\0';
    sectioncnt++;

    /* header level */
    while ('=' == *sectionlist) {
      sectionlist++;
      depth++;
    }
    /* skip first ! */
    if ('!' == *sectionlist)
      sectionlist++;

    http_response_printf(res,
	"<div style='padding-left: %dpx; padding-right: 15px'>"
	"<a href='#section%i'>%s</a></div>\n", depth * 15, sectioncnt, sectionlist);

    /* point to the next header */
    sectionlist = eol + 1;
  }
  http_response_printf(res, "</div>\n");
}

static void
print_entry(wiki_ctx_t *ctx, char *str_ptr)
{
  /* close form already opened */
  if (ctx->form_on)
    http_response_printf(ctx->res, "</form>\n");

  ctx->form_on = 1; /* so we know we will have to close <form> */
  ctx->form_cnt++;  /* for datafield */
  http_response_printf(ctx->res,
      "<form method=POST action='%s?entry' name='entryform'>\n", ctx->page);

  int size = 30;
  char date[80] = "";

  if (*(str_ptr)) {
    if ((strstr(str_ptr, "tiny")))
      size = 10;
    else if ((strstr(str_ptr, "small")))
      size = 20;
    else if ((strstr(str_ptr, "medium")))
      size = 40;
    else if ((strstr(str_ptr, "large")))
      size = 60;
    else if ((strstr(str_ptr, "huge")))
      size = 80;

    if ((strstr(str_ptr, "date")))
    {
      time_t now;
      struct tm * timeinfo;
      (void) time(&now);
      timeinfo = localtime(&now);
      strftime(date, 80, "%a %b %d %H:%M ", timeinfo);
    }
  }

  /* hidden datafield gives the {{data#}} to use */
  http_response_printf(ctx->res,
      "<p><input type='text' name='data' value='%s' size='%i' title='Entrer your text'>"
      "</p>"
      "<input type='hidden' name='datafield' value='%u' />"
      "<p><input type=submit name='add' value='Add' title='[alt-a]' accesskey='a'>"
      "</p>\n", date, size, ctx->form_cnt);
}

static char *
wiki_color_string(char c)
{
  switch (c) {
    case 'R':
      return "#FF0000";
    case 'G':
      return "#00FF00";
    case 'B':
      return "#0000FF";
    case 'Y':
      return "#FFFF00";
    case 'P':
      return "#FF00FF";
    case 'C':
      return "#00FFFF";
  }
  return "#000000";
}

int
wiki_print_data_as_html(
HttpResponse *res, char *raw_page_data, int autorized, char *page)
{
  char *q = NULL, *link = NULL; /* temporary scratch stuff */
  int   i, j, k, skip_chars;
  char  color_str[64];
  char label[80];
  char *str_ptr;
  char  *sectionlist = NULL;
  int section      = 0;
  /* flags, mainly for open tag states */
  int open_para    = 0;

  char color_k,color_prev='\0';
  char bgcolor_k,bgcolor_prev='\0';
  int private; /* flag 1 if access denied */

  wiki_ctx_t ctx;

  memset(&ctx, 0, sizeof(ctx));
  ctx.res = res;
  ctx.page = page;
  ctx.page_data = raw_page_data;

#define ULIST 0
#define OLIST 1
#define NUM_LIST_TYPES 2

  struct { char ident; int  depth; char *tag; } listtypes[] = {
    { '*', 0, "ul" },
    { '#', 0, "ol" }
  };


  prepare_toc(&sectionlist, raw_page_data);

  q = ctx.page_data;  /* ctx.page_data accumulates non marked up text, q is just a pointer
       * to the end of the current line - used by below func.
       */

  private=0;
  while ( (ctx.line.s = get_line_from_string(&q, &ctx.line.len)) && !private)
  {
    int   header_level = 0;
    int   blockquote_flag = 0;
    int   skip_to_content = 0;
    char  attrbuf[1024];

    ctx.line.pos = ctx.line.s;

    /*
     *  process any initial wiki chars at line beginning
     */

    if (ctx.pre_on && !isspace(*ctx.line.pos) && *ctx.line.pos != '\0')
    {
      /* close any preformatting if already on*/
      http_response_printf(res, "</pre>\n") ;
      ctx.pre_on = 0;
    }

    /* Handle ordered & unordered list, code is a bit mental.. */
    for (i=0; i<NUM_LIST_TYPES; i++)
    {

      /* extra checks avoid bolding */
      if ( *ctx.line.pos == listtypes[i].ident
           && ( *(ctx.line.pos+1) == listtypes[i].ident || isspace(*(ctx.line.pos+1)) ) )
      {
        int item_depth = 0;

        if (listtypes[!i].depth)
        {
          for (j=0; j<listtypes[!i].depth; j++)
            http_response_printf(res, "</%s>\n", listtypes[!i].tag);
          listtypes[!i].depth = 0;
        }

        while ( *ctx.line.pos == listtypes[i].ident ) { ctx.line.pos++; item_depth++; }

        if (item_depth < listtypes[i].depth)
        {
          for (j = 0; j < (listtypes[i].depth - item_depth); j++)
            http_response_printf(res, "</%s>\n", listtypes[i].tag);
        }
        else
        {
          for (j = 0; j < (item_depth - listtypes[i].depth); j++)
            http_response_printf(res, "<%s>\n", listtypes[i].tag);
        }

        http_response_printf(res, "<li>");
        listtypes[i].depth = item_depth;
        skip_to_content = 1;
      }
      else if (listtypes[i].depth && !listtypes[!i].depth)
      {
       /* close current list */
        for (j=0; j<listtypes[i].depth; j++)
          http_response_printf(res, "</%s>\n", listtypes[i].tag);

        listtypes[i].depth = 0;
      }
    }

    if (skip_to_content)
      goto line_content; /* skip parsing any more initial chars */

    /* Tables */

    if (*ctx.line.pos == '|')
    {
      int n;
      if (ctx.table_on==0)
        http_response_printf(res, "<table class='wikitable'>\n");

      http_response_printf(res, "<tr><td");
      if (0 != (n = parse_table_attributes(ctx.line.pos + 1, attrbuf, sizeof(attrbuf)))) {
	http_response_printf(res, " %s>", attrbuf);
	ctx.line.pos += n;
      } else {
	http_response_printf(res, ">");
      }
      ctx.line.pos++;
      ctx.table_on = 1;
      goto line_content;
    }
    else
    {
      if(ctx.table_on)
      {
        http_response_printf(res, "</table>\n");
        ctx.table_on = 0;
      }
    }

    /* pre formated  */

    if ( (isspace(*ctx.line.pos) || *ctx.line.pos == '\0'))
    {
      int n_spaces = 0;

      while ( isspace(*ctx.line.pos) ) { ctx.line.pos++; n_spaces++; }

      if (*ctx.line.pos == '\0')  /* empty line - para */
      {
        if (ctx.pre_on)
        {
          continue;
        }
        else if (open_para)
        {
          http_response_printf(res, "\n</p><p>\n") ;
        }
        else
        {
          http_response_printf(res, "\n<p>\n") ;
          open_para = 1;
        }
      }
      else /* starts with space so Pre formatted, see above for close */
      {
        if (!ctx.pre_on)
        http_response_printf(res, "<pre>\n") ;
        ctx.pre_on = 1;
        ctx.line.pos = ctx.line.pos - ( n_spaces - 1 ); /* rewind so extra spaces
                                                 they matter to pre */
        http_response_printf(res, "%s\n", util_htmlize(ctx.line.pos, &ctx.htmlbuf));
        continue;
      }
    }
    else if ( *ctx.line.pos == '=' ) /* header */
    {
      section++;
      while (*ctx.line.pos == '=')
        { header_level++; ctx.line.pos++; }
      http_response_printf(res, "<h%d id='section%i'>", header_level, section);
      ctx.page_data = ctx.line.pos;
    }
    else if ( *ctx.line.pos == '-' && *(ctx.line.pos+1) == '-' ) /* rule */
    {
      http_response_printf(res, "<hr/>\n");
      while ( *ctx.line.pos == '-' ) ctx.line.pos++;
    }
    else if ( *ctx.line.pos == '\'' ) /* quote */
    {
      blockquote_flag=1;
      ctx.line.pos++;
      http_response_printf(res, "<blockquote>\n");
    }

    line_content:

    /*
     * now process rest of the line
     */

    ctx.page_data = ctx.line.pos;

    while ( *ctx.line.pos != '\0' )
    {
      /* ignore link */
      if ( *ctx.line.pos == '!' && !isspace(*(ctx.line.pos+1)))
      {                   /* escape next word - skip it */
        *ctx.line.pos = '\0';
        http_response_printf(res, "%s", util_htmlize(ctx.page_data, &ctx.htmlbuf));
        ctx.page_data = ++ctx.line.pos;

        while (*ctx.line.pos != '\0' && !isspace(*ctx.line.pos)) ctx.line.pos++;
        if (*ctx.line.pos == '\0')
          continue;
      }
      /* search for link inside the line */
      else if ((link = check_for_link(ctx.line.pos, &skip_chars)) != NULL)
      {
        http_response_printf(res, "%s", util_htmlize(ctx.page_data, &ctx.htmlbuf));
        http_response_printf(res, "%s", link);

        ctx.line.pos += skip_chars;
        ctx.page_data = ctx.line.pos;

        continue;
      }
      /* TODO: Below is getting bloated and messy, need rewriting more
       *       compactly ( and efficently ).
       */
      else if  (*ctx.line.pos == '{' && *(ctx.line.pos+1) == '{')
      /* Proceed double braces */
      {
        if (ctx.line.s != ctx.line.pos
          && !is_wiki_format_char_or_space(*(ctx.line.pos-1)))
        { ctx.line.pos=ctx.line.pos+2; continue; }
        /* search closing braces */
        k=2;
        while (*(ctx.line.pos+k) != '}')
        {
          if ( *(ctx.line.pos+k) == '\0' )
            { ctx.line.pos=ctx.line.pos+2; continue; }
          k++;
        }
        k++;
        if (*(ctx.line.pos+k) == '}')
          *(ctx.line.pos+k)='\0'; /* terminate the line */
        else
          { ctx.line.pos=ctx.line.pos+2; continue; }

        /* Parse tags between double braces */
        /*  need to be rewritten */

        /* search image/video size */
        wiki_parse_between_braces(ctx.line.pos+2);
        /* control access */
        if ( (strstr(ctx.line.pos+2,"private")) )
        {
          if ( !autorized )
          {
            http_response_printf(res, "<p>--- Sorry, the access below is denied ----</p>\n");
            *ctx.page_data = '\0';
            private=1; /* will stop getting line */
            break; /* terminate the while loop */
          }
        }
        /* Expand */
        if ( (str_ptr=strstr(ctx.line.pos+2,"expand")) )
        {
          if ( *(str_ptr-1) == '-' ) /* terminate expand */
          {
            http_response_printf(res,"</div></div>\n");
          }
          else
          {
            /* search label */
            if ( *(str_ptr+6) == '=' )
            {
              i=0;
              str_ptr+=7;
              while ( (*str_ptr != '}' && *str_ptr != '\0') && i < 80)
              {
                label[i]=*str_ptr;
                str_ptr++;
                i++;
              }
              label[i]='\0';
            }
            else
              strcpy(label,"Click here!");

            expand_collapse_num++;
            http_response_printf(res,
              "<div id=\"wrapper\">"
              "<p>"
              "<a onclick=\"expandcollapse('myvar%i');\" "
              "title=\"Expand or collapse\">%s</a>"
              "</p>"
              "<div id=\"myvar%i\" style=\"display:none\">",
              expand_collapse_num,label,expand_collapse_num);
          }
        }
        /* Collapse */
        else if ( (str_ptr=strstr(ctx.line.pos+2,"collapse")) )
        {
          if ( *(str_ptr-1) == '-' ) /* terminate expand */
          {
            http_response_printf(res,"</div></div>\n");
          }
          else
          {
            /* search label */
            if ( *(str_ptr+8) == '=' )
            {
              i=0;
              str_ptr+=9;
              while ( (*str_ptr != '}' && *str_ptr != '\0') && i < 80)
              {
                label[i]=*str_ptr;
                str_ptr++;
                i++;
              }
              label[i]='\0';
            }
            else
              strcpy(label,"Click here!");

            expand_collapse_num++;
            http_response_printf(res,
              "<div id=\"wrapper\">"
              "<p>"
              "<a onclick=\"expandcollapse('myvar%i');\" "
              "title=\"Expand or collapse\">%s</a>"
              "</p>"
              "<div id=\"myvar%i\">",
              expand_collapse_num,label,expand_collapse_num);
          }
        }
        /* Upload a file */
        if ( (strstr(ctx.line.pos+2,"upload")) )
        {
          http_response_printf(res,
            "<p><FORM ACTION='Upload' METHOD='post' ENCTYPE='multipart/form-data'>"
            "<input type=file><br>"
            "Note about this file: <input type=text name=note><br>"
            "<P><INPUT TYPE=submit VALUE=validate></P>"
            "</form></p>\n");
        }

        /* table of contents */
        if (strstr(ctx.line.pos+2, "toc"))
        {
	  print_toc(res, sectionlist);
        }
	/* entry  */
        if ( (str_ptr=strstr(ctx.line.pos+2,"entry")) )
        {
	  print_entry(&ctx, str_ptr + 5);
        }
        /* Simple checkbox */
        if ( (str_ptr=strstr(ctx.line.pos+2,"checkbox")) )
        {
	  int num = 0, state = 0;
          if (*(str_ptr+8) == '=')
            num=atol(str_ptr+9);
          if ( (str_ptr=strchr(str_ptr+9,';')) )
            state=atoi(str_ptr+1);
          /* little trick: checkbox unchecked is not posted
           * the input hidden returns the value 0
           * we have the values: 0 , 1 when checked
           * and 0 when unchecked. browser must operate sequentially!
           */
          http_response_printf(res,
            "<input type='hidden' name='checkbox%i' value='0' />"
            "<input type='checkbox' name='checkbox%i' value='1' %s /> ",
            num,num,state ? "checked='checked'":"");
        }
        /* Save checkboxes state */
        if ( (strstr(ctx.line.pos+2,"save")) )
        {
          http_response_printf(res,
            "<input type=submit name='save' value='Save' title='[alt-s]' accesskey='s'>\n");
        }
        /* Delete field */
        if ( (strstr(ctx.line.pos+2,"delete")) )
        {
          http_response_printf(res,
            "<input type=submit name='delete' value='Delete' title='[alt-d]' accesskey='d'>\n");
        }
        ctx.line.pos += k; /* point to '\0' (the last closing brace) */
        ctx.page_data = ctx.line.pos + 1; /* point just after '\0' */
      } /* end of the double braces */

      /* single braces */
      else if (*ctx.line.pos == '{' && strchr("RGBYPCD",*(ctx.line.pos+1)) && *(ctx.line.pos+2) == '}')
      {
        /* Text color */
        if (ctx.line.s != ctx.line.pos
          && !is_wiki_format_char_or_space(*(ctx.line.pos-1))
          && !ctx.color_on)
        { ctx.line.pos++; continue; }

        if ((isspace(*(ctx.line.pos+3)) && !ctx.color_on))
        { ctx.line.pos++; continue; }

        /* color */
        *ctx.line.pos = '\0';
        color_k = *(ctx.line.pos+1);
        snprintf(color_str, sizeof(color_str), "<FONT COLOR=\"%s\">", wiki_color_string(color_k));
        if (color_prev && color_k != color_prev) {
          ctx.color_on = 0; /* reset flag */
          http_response_printf(res, "%s","</FONT>\n");
        }
          color_prev = color_k;

        http_response_printf(res, "%s%s\n", ctx.page_data, ctx.color_on ? "</FONT>" : color_str);
        ctx.color_on ^= 1; /* switch flag */
        ctx.page_data = ctx.line.pos+3;
      }
       else if (*ctx.line.pos == '(' && strchr("RGBYPCD",*(ctx.line.pos+1)) && *(ctx.line.pos+2) ==')' )
      {
        /* bgcolor */
        if (ctx.line.s != ctx.line.pos
          && !is_wiki_format_char_or_space(*(ctx.line.pos-1))
          && !ctx.bgcolor_on)
        { ctx.line.pos++; continue; }

        if ((isspace(*(ctx.line.pos+3)) && !ctx.bgcolor_on))
        { ctx.line.pos++; continue; }

        /* color */
        *ctx.line.pos = '\0';
        bgcolor_k = *(ctx.line.pos+1);
        snprintf(color_str, sizeof(color_str), "<SPAN STYLE=\"background: %s\">", wiki_color_string(bgcolor_k));
        if (bgcolor_prev && bgcolor_k != bgcolor_prev) {
          ctx.bgcolor_on = 0; /* reset flag */
          http_response_printf(res, "%s","</SPAN>\n");
        }
        bgcolor_prev = bgcolor_k;

        http_response_printf(res, "%s%s\n", util_htmlize(ctx.page_data, &ctx.htmlbuf), ctx.bgcolor_on ? "</SPAN>" : color_str);
        ctx.bgcolor_on ^= 1; /* switch flag */
        ctx.page_data = ctx.line.pos+3;

      }
      else if (*ctx.line.pos == '`')
      {
        print_element(&ctx, &element_code, &ctx.code_on);
      }
      else if (*ctx.line.pos == '+')
      {
        print_element(&ctx, &element_highlight, &ctx.highlight_on);
      }
      else if (*ctx.line.pos == '*')
      {
        print_element(&ctx, &element_bold, &ctx.bold_on);
      }
      else if (*ctx.line.pos == '_' )
      {
        print_element(&ctx, &element_underline, &ctx.underline_on);
      }
      else if (*ctx.line.pos == '-')
      {
        print_element(&ctx, &element_strikethrough, &ctx.strikethrough_on);
      }
      else if (*ctx.line.pos == '/' )
      {
        if (ctx.line.s != ctx.line.pos
	  && !is_wiki_format_char_or_space(*(ctx.line.pos-1))
          && !ctx.italic_on)
	{ 
	  ctx.line.pos++;
	  continue;
	}

	if (isspace(*(ctx.line.pos+1)) && !ctx.italic_on) {
	  ctx.line.pos++;
	  continue;
	}

	/* crude path detection */
	if (ctx.line.s != ctx.line.pos && isspace(*(ctx.line.pos-1)) && !ctx.italic_on) {
	  char *tmp   = ctx.line.pos+1;
	  int slashes = 0;

	  /* Hack to escape out file paths */
	  while (*tmp != '\0' && !isspace(*tmp))
	  {
	    if (*tmp == '/') slashes++;
	    tmp++;
	  }

	  if (slashes > 1 || (slashes == 1 && *(tmp-1) != '/')) {
	    ctx.line.pos = tmp;
	    continue;
	  }
	} 

        if (*(ctx.line.pos+1) == '/')
	  ctx.line.pos++;     /* escape out common '//' - eg urls */
        else
        {
          /* italic */
          *ctx.line.pos = '\0';
	  http_response_printf(res, "%s%s\n", util_htmlize(ctx.page_data, &ctx.htmlbuf), ctx.italic_on ? "</i>" : "<i>");
	  ctx.italic_on ^= 1; /* reset flag */
	  ctx.page_data = ctx.line.pos+1;
        }
      }
      else if (*ctx.line.pos == '|' && ctx.table_on) /* table column */
      {
	int n;
        *ctx.line.pos = '\0';
	http_response_printf(res, "%s", util_htmlize(ctx.page_data, &ctx.htmlbuf));
	http_response_printf(res, "</td><td");
	if (0 != (n = parse_table_attributes(ctx.line.pos + 1, attrbuf, sizeof(attrbuf)))) {
	  http_response_printf(res, " %s>", attrbuf);
	  ctx.line.pos += n;
	} else {
	  http_response_printf(res, ">\n");
	}
	ctx.page_data = ctx.line.pos+1;
      }

      ctx.line.pos++;

    } /* while loop, next word */

    if (*ctx.page_data != '\0')           /* accumulated text left over */
      http_response_printf(res, "%s", util_htmlize(ctx.page_data, &ctx.htmlbuf));

      /* close any html tags that could be still open */

    if (listtypes[ULIST].depth)
      http_response_printf(res, "</li>");

    if (listtypes[OLIST].depth)
      http_response_printf(res, "</li>");

    if (ctx.table_on)
      http_response_printf(res, "</td></tr>\n");

    if (header_level)
      http_response_printf(res, "</h%d>\n", header_level);
    else if (blockquote_flag)
      http_response_printf(res, "</blockquote>\n");
    else
      http_response_printf(res, "\n");
    } /* next line */

  /*** clean up anything thats still open ***/

  if (ctx.pre_on)
    http_response_printf(res, "</pre>\n");

  /* close any open lists */
  for (i=0; i<listtypes[ULIST].depth; i++)
    http_response_printf(res, "</ul>\n");

  for (i=0; i<listtypes[OLIST].depth; i++)
    http_response_printf(res, "</ol>\n");

  /* close any open paras */
  if (open_para)
    http_response_printf(res, "</p>\n");

  /* close table */
  if (ctx.table_on)
    http_response_printf(res, "</table>\n");
  /* close form */
  if (ctx.form_on)
    http_response_printf(res, "</form>\n");

  free(ctx.htmlbuf.s);
  free(sectionlist);
  return private;
}
