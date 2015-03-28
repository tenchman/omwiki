/*
 *  CiWiki is a fort of DidiWiki - a small lightweight wiki engine. 
 *  DidiWiki Copyright 2004 Matthew Allum <mallum@o-hand.com>
 *  CiWiki Copyright 2010 Jean-Pierre Redonnet <inphilly@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "didi.h"
#include "wikitext.h"
#include "wikilogin.h"
#include "wikichanges.h"
#include "wikiashtml.h"
#include "wikientries.h"

/* local variable */
static int errorcnt=0;
static char* CssData = STYLESHEET;

/* external variable */
extern int lgindex;

/* read wiki page */
static char *
file_read(char *filename)
{
  struct stat st;
  FILE*       fp;
  char*       str;
  int         len;

  /* Get the file size. */
  if (stat(filename, &st)) 
    return NULL;

  if (!(fp = fopen(filename, "rb"))) 
    return NULL;
  
  str = (char *)malloc(sizeof(char)*(st.st_size + 1));
  len = fread(str, 1, st.st_size, fp);
  if (len >= 0) str[len] = '\0';
  
  fclose(fp);

  return str;
}

/* save wiki page */
static int
file_write(char *filename, char *data)
{
  FILE*       fp;
  int         bytes_written = 0;
  int         len           = strlen(data); /* clip off extra '\0' */
  char        filename_prev[strlen(filename)+10];
  char        *str_ptr;

  /* replace each space between 2 words with an underline */
  while ( (str_ptr=strchr(filename,' ')) )
    *str_ptr='_';

  if ( strstr(filename,".prev.1") )
  {
    /* previous page restored as current page */
    strcpy(filename_prev,filename);
    *strstr(filename,".prev.1")='\0'; //remove ext
    rename(filename,filename_prev);
  }
  else
  {
    /* backup the previous page */
    strcpy(filename_prev,filename);
    strcat(filename_prev,".prev.1");
    rename(filename,filename_prev);
  }
  
  if (!(fp = fopen(filename, "wb"))) 
    return -1;
 
  while ( len > 0 )
    {
      bytes_written = fwrite(data, sizeof(char), len, fp);
      len = len - bytes_written;
      data = data + bytes_written;
    }

  fclose(fp);

  return 1;
}


int
wiki_redirect(HttpResponse *res, char *location)
{
  char *location_enc = util_httpize(location);
  
  int   header_len = strlen(location_enc) + 14; 
  char *header = alloca(sizeof(char)*header_len);

  snprintf(header, header_len, "Location: %s\r\n", location_enc);
  free(location_enc);
  
  http_response_append_header(res, header);
  http_response_printf(res, "<html>\n<p>Redirect to %s</p>\n</html>\n", 
               location);
  http_response_set_status(res, 302, "Moved Temporarily");
  http_response_send(res);

  exit(0);
}

void
wiki_page_forbiden(HttpResponse *res)
{
  http_response_printf(res, 
    "<html>\n<p>Permission denied - You need to login.</p>"
    "<a href='javascript:javascript:history.go(-1)'>Return to the previous page.</a>\n"
    "</html>\n");
  http_response_set_status(res, 403, "Forbidden</");
  http_response_send(res);

  exit(0);
}

int
wiki_show_page(HttpResponse *res, char *wikitext, char *page, int autorized)
{
  int private;
  char *html_clean_wikitext = NULL;

  http_response_printf_alloc_buffer(res, strlen(wikitext)*2);

  wiki_show_header(res, page, TRUE, autorized);

  html_clean_wikitext = util_htmlize(wikitext, strlen(wikitext));

  private=wiki_print_data_as_html(res, html_clean_wikitext, autorized, page);      

  wiki_show_footer(res);  

  http_response_send(res);

  exit(private);
}

void
wiki_show_edit_page(HttpResponse *res, char *wikitext, char *page,
                    int preview, int autorized)
{

  wiki_show_header(res, page, FALSE, autorized);
  http_response_printf(res, "%s", SHORTHELP);
  
  if (wikitext == NULL) wikitext = "";
  http_response_printf(res, EDITFORM, page, wikitext);

  if (preview)
  {
    char *html_clean_wikitext;

    http_response_printf(res, EDITPREVIEW);
    http_response_printf_alloc_buffer(res, strlen(wikitext)*2);

    html_clean_wikitext = util_htmlize(wikitext, strlen(wikitext));

    wiki_print_data_as_html(res, html_clean_wikitext, autorized, page);
  }

  wiki_show_footer(res);

  http_response_send(res);
  exit(0);
}

void
wiki_show_delete_confirm_page(HttpResponse *res, char *page)
{
  wiki_show_header(res, "Delete Page", FALSE, 0);

  http_response_printf(res, DELETEFORM, page, page);

  wiki_show_footer(res);

  http_response_send(res);

  exit(0);
}

void
wiki_show_create_page(HttpResponse *res)
{
  wiki_show_header(res, "Create New Page", FALSE, 0);
  http_response_printf(res, "%s", CREATEFORM);
  wiki_show_footer(res);

  http_response_send(res);
  exit(0);
}

/* compare files dates */
static int 
changes_compar(const struct dirent **d1, const struct dirent **d2)
{
    struct stat st1, st2;

    stat((*d1)->d_name, &st1);

    stat((*d2)->d_name, &st2);

    if (st1.st_mtime > st2.st_mtime)
      return 1;
    else
      return -1;
}



WikiPageList**
wiki_get_pages(int  *n_pages, char *expr)
{
  WikiPageList  **pages;
  struct dirent **namelist;
  int             n, i = 0;
  struct stat     st;
  struct passwd  *pwd;
  struct group   *grp;


  n = scandir(".", &namelist, 0, (void *)changes_compar);
  
  pages = malloc(sizeof(WikiPageList*)*n);

  while(n--) 
  {
    if ((namelist[n]->d_name)[0] == '.' 
    || !strcmp(namelist[n]->d_name, "styles.css"))
      goto cleanup;

    /* are we looking for an expression in the wiki? */
    if (expr != NULL) 
    {           
      /* Super Simple Search */
      char *data = NULL;
      /* load the page */
      if ((data = file_read(namelist[n]->d_name)) != NULL)
        if (strcasestr(data, expr) == NULL)
          if (strcmp(namelist[n]->d_name, expr) != 0) 
            goto cleanup; 
    }

    stat(namelist[n]->d_name, &st);

    /* ignore anything but regular readable files */
    if (S_ISREG(st.st_mode) && access(namelist[n]->d_name, R_OK) == 0)
    {
      pwd=getpwuid(st.st_uid);
      grp=getgrgid(st.st_gid);
        
      pages[i]        = malloc(sizeof(WikiPageList));
      pages[i]->name  = strdup(namelist[n]->d_name);
      pages[i]->mtime = st.st_mtime;
      pages[i]->user_name =  strdup(pwd->pw_name);
      pages[i]->grp_name =  strdup(grp->gr_name);
      i++;
    }

    cleanup:
      free(namelist[n]);
  } //end while

  *n_pages = i;

  free(namelist);

  if (i==0) return NULL;

  return pages;
}


void
wiki_show_login_page(HttpResponse *res)
{
  wiki_show_header(res, "Login Page", FALSE, 0);
  http_response_printf(res, "%s", LOGINFORM);
  wiki_show_footer(res);

  http_response_send(res);
  exit(0);
}

void
wiki_show_newaccount_page(HttpResponse *res)
{
  wiki_show_header(res, "Login Page", FALSE, 0);
  http_response_printf(res, "%s", NEWLOGINFORM);
  wiki_show_footer(res);

  http_response_send(res);
  exit(0);
}

void
wiki_show_loginfo_page(HttpResponse *res, char *ipsrc)
{
  wiki_show_header(res, "Log Info Page", FALSE, 0);
  
  http_response_printf(res, LOGINFO, ipsrc, wikilogin_username(ipsrc));
  wiki_show_footer(res);

  http_response_send(res);
  exit(0);
}


void
wiki_show_index_page(HttpResponse *res, char *dir)
/* list pages, files */
{
  struct dirent **namelist;
  int     n;
  int     count_files;
  int     numvar;
  
  count_files=0;
  numvar=1;;
  wiki_show_header(res, "Index", FALSE, 0);
  if (!dir) 
    dir=strdup(".");
  n = scandir(dir, &namelist, 0, (void *)changes_compar);
  
  //prepare an collapsible box
  http_response_printf(res, "<div id=""wrapper""><p><a onclick=""expandcollapse('myvar%i');"" title=""Expand or collapse"">Index %i</a></p><div id=""myvar%i"">\n",numvar,numvar,numvar); 
  http_response_printf(res, "<ul>\n");
  
  while(n--)
  {
    if ( namelist[n]->d_type == DT_REG )
    {
      //exclude hidden and style
      if ((namelist[n]->d_name)[0] == '.'
          || !strcmp(namelist[n]->d_name, "styles.css"))
        goto cleanup;
      //print link to page and page name (previous pages are not printed)
      if ( !strstr(namelist[n]->d_name,".prev.") ) {
		//box is full so create a new one
		if (count_files >=lgindex) {
			count_files=0;
			http_response_printf(res, "</ul>\n");
			http_response_printf(res, "</div></div>\n");
			if (numvar%4 == 0) http_response_printf(res, "<BR>\n");
			numvar++;
			http_response_printf(res, "<div id=""wrapper""><p><a onclick=""expandcollapse('myvar%i');"" title=""Expand or collapse"">Index %i</a></p><div id=""myvar%i"" style=""display:none"">\n",numvar,numvar,numvar);
			http_response_printf(res, "<ul>\n");
		}
        http_response_printf(res, "<li><a href='%s'>%s</a></li>\n", namelist[n]->d_name, namelist[n]->d_name); 
        count_files++;
      }
        
      cleanup:
      free(namelist[n]);
    }
  } //end while
  http_response_printf(res, "</div></div>\n");
  http_response_printf(res, "</ul>\n");
  free(namelist);
  wiki_show_footer(res);
  http_response_send(res);
  exit(0);
}

void
wiki_show_search_results_page(HttpResponse *res, char *expr)
{
  WikiPageList **pages = NULL;
  int            n_pages, i;

  if (expr == NULL || strlen(expr) == 0)
  {
    wiki_show_header(res, "Search", FALSE, 0);
    http_response_printf(res, "No Search Terms supplied");
    wiki_show_footer(res);
    http_response_send(res);
    exit(0);
  }

  pages = wiki_get_pages(&n_pages, expr);
  
  /* if only one page is found, redirect to it */
  if (n_pages == 1) 
    wiki_redirect(res, pages[0]->name);
  
  if (pages)
    {
    for (i=0; i<n_pages; i++)
      if (!strcmp(pages[i]->name, expr)) /* redirect on page name match */
        wiki_redirect(res, pages[i]->name);

    wiki_show_header(res, "Search", FALSE, 0);

    for (i=0; i<n_pages; i++)
    {
      http_response_printf(res, "<a href='%s'>%s</a><br />\n", 
                   pages[i]->name, 
                   pages[i]->name);
    }
  }
  else 
  {
    wiki_show_header(res, "Search", FALSE, 0);
    http_response_printf(res, "No matches");
  }

  wiki_show_footer(res);
  http_response_send(res);

  exit(0);
}

void 
wiki_show_template(HttpResponse *res, char *template_data)
{
  /* 4 templates - header.html, footer.html, 
                   header-noedit.html, footer-noedit.html

     Vars;

     $title      - page title. 
     $include()  - ?
     $pages 

  */

}

void
wiki_show_header(HttpResponse *res, char *page_title, int want_edit, int autorized)
{
  http_response_printf(res, 
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
    "<html xmlns='http://www.w3.org/1999/xhtml'>\n"
    "<head>\n"
    "<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />\n" 
    "<link rel='SHORTCUT ICON' href='favicon.ico' />\n"
    "<link media='all' href='styles.css' rel='stylesheet' type='text/css' />\n"
    "<title>%s</title>\n"
    "<script type=\"text/javascript\">\n"
    "<!--\n"
    "function expandcollapse(obj) {\n"
    "var el = document.getElementById(obj);\n"
    "if ( el.style.display != \"none\" ) {\n"
		"el.style.display = 'none';\n"
    "}\n"
    "else {\n"
		"el.style.display = '';\n"
    "}\n"
    "}\n"
    "//-->\n"
    "</script>\n"
    "</head>\n"
    "<body>\n", page_title
    );
  
  if (want_edit)
    http_response_printf(res, EDITHEADER, page_title, page_title, 
                      page_title, autorized ? "You are logged in":"login");
  else
    http_response_printf(res, PAGEHEADER, page_title, "" );
}

void
wiki_show_footer(HttpResponse *res)
{
  http_response_printf(res, "%s", PAGEFOOTER);

  http_response_printf(res, 
     "</body>\n"
     "</html>\n"
     );
}

void
wiki_handle_rest_call(HttpRequest  *req, 
              HttpResponse *res,
              char         *func)
{
  if (func != NULL && *func != '\0')
  {
    if (!strcmp(func, "page/get"))
    {
      char *page = http_request_param_get(req, "page");

      if (page == NULL)
        page = http_request_get_query_string(req);

      if (page && (access(page, R_OK) == 0)) 
      {
        http_response_printf(res, "%s", file_read(page));
        http_response_send(res);
        return;
      }  
    }
    else if (!strcmp(func, "page/set"))
    {
      char *wikitext = NULL, *page = NULL;
      if( ( (wikitext = http_request_param_get(req, "text")) != NULL)
          && ( (page = http_request_param_get(req, "page")) != NULL))
      {
        file_write(page, wikitext);
        /* log modified page name and IP address */
        syslog(LOG_LOCAL0|LOG_INFO, "page %s modified from %s", page ,http_request_get_ip_src(req));
        http_response_printf(res, "success");
        http_response_send(res);
        return;
      }
    }
    else if (!strcmp(func, "page/delete"))
    {
      char *page = http_request_param_get(req, "page");

      if (page == NULL)
        page = http_request_get_query_string(req);

      if (page && (unlink(page) > 0))
      {
        http_response_printf(res, "success");
        http_response_send(res);
        return;  
      }
    }
    else if (!strcmp(func, "page/exists"))
    {
      char *page = http_request_param_get(req, "page");

      if (page == NULL)
        page = http_request_get_query_string(req);

      if (page && (access(page, R_OK) == 0)) 
        {
          http_response_printf(res, "success");
          http_response_send(res);
          return;  
        }
    }
    else if (!strcmp(func, "pages") || !strcmp(func, "search"))
    {
      WikiPageList **pages = NULL;
      int            n_pages, i;
      char          *expr = http_request_param_get(req, "expr");

      if (expr == NULL)
        expr = http_request_get_query_string(req);
      
      pages = wiki_get_pages(&n_pages, expr);

      if (pages)
      {
        for (i=0; i<n_pages; i++)
        {
          struct tm   *pTm;
          char   datebuf[64];
          
          pTm = localtime(&pages[i]->mtime);
          strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M", pTm);
          http_response_printf(res, "%s\t%s\n", pages[i]->name, datebuf);
        }

        http_response_send(res);
        return;  
      }
    }
  }

  http_response_set_status(res, 500, "Error");
  http_response_printf(res, "<html><body>Failed</body></html>\n");
  http_response_send(res);

  return;  
}

/* wiki_handle_http_request is the Main function
 * It gets the request and then calls the functions to:
 *  - load page (wikitext)
 *  - convert wikitext in html
 *  - edit wikitext 
 *  - save, delete wikitext
 *  - list pages (index)
 *  - list changes
 *  - login 
 *  - get permission
 */
void
wiki_handle_http_request(HttpRequest *req)
{
  HttpResponse *res      = http_response_new(req);
  char         *page     = http_request_get_path_info(req); 
  char         *command  = http_request_get_query_string(req); 
  char         *wikitext = NULL;
  int          return_code=0, private=1, autorized=0;
  char         *msg,*link;
  char         *listbox;
  char         *value;
  char         *str_ptr;
  char         *folder;

  util_dehttpize(page);// remove any encoding on the requested page name.

  if (!strcmp(page, "/"))
  {
    if (access("WikiHome", R_OK) != 0)
      wiki_redirect(res, "/WikiHome?create");
    page = "/WikiHome";
  }

  if (!strcmp(page, "/styles.css"))
  {
    /*  Return CSS page */
    http_response_set_content_type(res, "text/css");
    http_response_printf(res, "%s", CssData);
    http_response_send(res);
    exit(0);
  }

  if (!strcmp(page, "/favicon.ico"))
  {
    /*  Return favicon */
    http_response_set_content_type(res, "image/ico");
    http_response_set_data(res, FaviconData, FaviconDataLen);
    http_response_send(res);
    exit(0);
  }

  if ( (str_ptr=strrchr(page, '.')) ) 
  {
    /* check if it's an image extension */
    if ( !strcasecmp(str_ptr, ".png") || !strcasecmp(str_ptr, ".jpeg") ||
         !strcasecmp(str_ptr, ".jpg") || !strcasecmp(str_ptr, ".gif") ||
         !strcasecmp(str_ptr, ".pdf") )
    {
      http_response_send_smallfile(res, page+1, "image/ico", 1000000);
      //http_response_send_bigfile(res, page+1, "image/ico");
      exit(0);
    }
    /* check if it's a package or compressed file */
    else if ( !strcasecmp(str_ptr, ".gz") || !strcasecmp(str_ptr, ".bz2") ||
         !strcasecmp(str_ptr, ".lzma") || !strcasecmp(str_ptr, ".zip") ||
         !strcasecmp(str_ptr, ".tar") )
    {
      http_response_send_bigfile(res, page+1, "application/octet-stream");
      exit(0);
    }
  }
  
  page = page + 1;      /* skip slash */

  if (!strncmp(page, "api/", 4))
  {
    char *p;

    page += 4; 
    for (p=page; *p != '\0'; p++)
      if (*p=='?') { *p ='\0'; break; }
    
    wiki_handle_rest_call(req, res, page); 
    exit(0);
  }

  /* A little safety. issue a malformed request for any paths,
   * There shouldn't need to be any..
   */
  if (strchr(page, '/'))
  {
    http_response_set_status(res, 404, "Not Found");
    http_response_printf(res, "<html><body>404 Not Found</body></html>\n");
    http_response_send(res);
    exit(0);
  }
  
  if (!strcmp(page, "Upload")) 
  {
    FILE *pop;
    int status;
    char string[256];
    
    string[255]='\0';
    status=0;
    //upload doesn't work yet!
    if ( (pop = popen("/usr/local/bin/upload.exe", "w")) )
    {
      while (fgets(string, 255, pop) != NULL)
      {
        if (*string=='\0' || *string==EOF) break;
        http_response_printf(res, "%s\n",string);
        *string='\0';
      }
      status = pclose(pop);
      if (status==-1) printf("\nError in page==Upload\n");
    }
    else
      http_response_printf(res, 
        "<html><body></body><strong>Cannot upload.</strong></html>\n");
      
    http_response_send(res);
    
    exit(0);        
  }
  else if (!strcmp(page, "Login"))
  {
    if ( !strncmp(command, "rac=",4) )
    /* url: /Login?rac=rac=xxxxxxxxx&username
     * rac means return of access code */
    {
      char *username=NULL;
      if ( (username=wikirac_isvalid( 
            command+4, 
            http_request_get_ip_src(req))) )
      {
        /* code > 0, so permission is granted */
        wikilogin_setpermission(
          http_request_get_ip_src(req),
          username );
        /* store in log file */
        syslog(LOG_LOCAL0|LOG_INFO, "Login successful from %s",
              http_request_get_ip_src(req));
        /* go the the home page */
        page="/WikiHome";
        wiki_redirect(res, "WikiHome");
      }
      else
      {
        /* Failure: Return a msg to the user */
        msg=strdup("Invalid access code.");
        link=strdup("href='WikiHome'>Home Page");
        http_response_printf(res, "<html><body><strong>%s</strong><br><br>"
                                  "<a %s</a>"
                                  "</body></html>\n",msg,link);
        http_response_send(res);
        /* store msg in log file */
        syslog(LOG_LOCAL0|LOG_INFO, "Login error %s from %s",
              msg ,http_request_get_ip_src(req));
        free(msg);
        exit(0);        
      }
    }
    else if ( http_request_param_get(req, "logoff") != NULL )
    /* the form contains the input name:logoff. */
    {
      wikilogin_logoff(http_request_get_ip_src(req));
      /* go the the home page */
      page="/WikiHome";
      wiki_redirect(res, "WikiHome");
      exit(0); 
    }
    else if ( http_request_param_get(req, "chgpwd") != NULL )
    /* the form contains the input name:chgpwd. */
    {
      if ( wikilogin_username(http_request_get_ip_src(req)) != NULL ) 
      {  
		int status = wikilogin_chgpwd(
          http_request_get_ip_src(req),
          http_request_param_get(req, "password"),
          http_request_param_get(req, "newpassword") );
          
        if (status > 0)
        {    
          /* store in log file */
          syslog(LOG_LOCAL0|LOG_INFO, "PWD change from %s status:%i",
              http_request_get_ip_src(req), status);    
              
          /* Confirm and go the the home page */
          msg=strdup("You can log in with your new password.");
          link=strdup("href='WikiHome'>Home Page");
          http_response_printf(res, "<html><body><strong>%s</strong><br><br>"
                                    "<a %s</a>"
                                    "</body></html>\n",msg,link);
          http_response_send(res);
          /* store msg in log file */
          syslog(LOG_LOCAL0|LOG_INFO, "Password changed from %s",
			http_request_get_ip_src(req));
          free(msg);
          exit(0); 
	    } 
	    else
	    {
		  /* Failure: Return a msg to the user */    
          switch(status) {  
		  case -1  : 
            msg=strdup("Password is too short. 8 chars or more required.");
            link=strdup("href='javascript:javascript:history.go(-1)'>Back");
            break;       
          case -2  : 
            msg=strdup("Password with invalid char");
            link=strdup("href='javascript:javascript:history.go(-1)'>Back");
            break;  
          case -3  : 
            msg=strdup("Password is too long. 24 chars or less required");
            link=strdup("href='javascript:javascript:history.go(-1)'>Back");
            break; 	
          case -10 : 
            msg=strdup("Login refused: Wrong password");
            link=strdup("href='javascript:javascript:history.go(-1)'>Back");
            break;       
          case -20 : 
            msg=strdup("Login refused: Wrong name");
            link=strdup("href='javascript:javascript:history.go(-1)'>Back");
            break;       
          case -100: 
            msg=strdup("Internal error. We are sorry. Please report the problem.");
            link=strdup("href='WikiHome'>Home Page");
            break;              
          default : 
            msg=strdup("Unknow error.");
            link=strdup("href='WikiHome'>Home Page");
          }
            
          http_response_printf(res, "<html><body><strong>%s</strong><br><br>"
                                    "<a %s</a>"
                                    "</body></html>\n",msg,link);
          http_response_send(res);
          /* store msg in log file */
          syslog(LOG_LOCAL0|LOG_INFO, "Change password error %s from %s",
              msg ,http_request_get_ip_src(req));
          free(msg);
          exit(0); 
	    }
      }
      else
      {
       /* Failure: Return a msg to the user */   
        msg=strdup("You cannot change your password (autologged).");
        link=strdup("href='WikiHome'>Home Page");
        http_response_printf(res, "<html><body><strong>%s</strong><br><br>"
                                  "<a %s</a>"
                                  "</body></html>\n",msg,link);
        http_response_send(res);
        free(msg);
        exit(0);       
      }
    }
    else if ( http_request_param_get(req, "username") != NULL )
     /* the form contains an username, it's a login page */
    {
      /* log is valid ? */
      if ( (return_code=wikilogin_isvalid(
            http_request_param_get(req, "username"),
            http_request_param_get(req, "password"),
            http_request_param_get(req, "email"),
            http_request_param_get(req, "code"),
            http_request_param_get(req, "newaccount"),
            http_request_get_ip_src(req) ) ) > 0 )
      {
        /* code > 0, so permission is granted */
        wikilogin_setpermission(
          http_request_get_ip_src(req),
          http_request_param_get(req, "username") );
        /* store in log file */
        syslog(LOG_LOCAL0|LOG_INFO, "Login successful from %s",
              http_request_get_ip_src(req));
        /* go the the home page */
        page="/WikiHome";
        wiki_redirect(res, "WikiHome");
      }
      else
      /* code < 1 reports error */
      {
        switch(return_code) {
        case 0  : 
          msg=strdup("Permission denied");
          link=strdup("href='WikiHome'>Home Page");
          break;
        case -1  : 
          msg=strdup("Name or password is too short. 8 chars or more required.");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;       
        case -2  : 
          msg=strdup("Name or password with invalid char");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;  
        case -3  : 
          msg=strdup("Name or password is too long. 24 chars or less required");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;         
        case -10 : 
          msg=strdup("Login refused: Wrong name/password");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;       
        case -20 : 
          msg=strdup("Login refused: Wrong name/password");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;       
        case -30 : 
          msg=strdup("Username already exist. Choose another one.");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;
        case -35 : 
          msg=strdup("Email non valid.");
          link=strdup("href='javascript:javascript:history.go(-1)'>Back");
          break;  
        case -40 : 
          msg=strdup("You need an access code. You should receive it by email.");
          link=strdup("href='WikiHome'>Home Page");
          break;  
        case -100: 
          msg=strdup("Internal error. We are sorry. Please report the problem.");
          link=strdup("href='WikiHome'>Home Page");
          break;              
        default : 
          msg=strdup("Unknow error.");
          link=strdup("href='WikiHome'>Home Page");
        }
        /* Return a msg to the user */
        http_response_printf(res, "<html><body><strong>%s</strong><br><br>"
                                  "<a %s</a><br></body></html>\n",msg,link);
        http_response_send(res);
        /* store msg in log file */
        syslog(LOG_LOCAL0|LOG_INFO, "Login error %s from %s",
              msg ,http_request_get_ip_src(req));
        free(msg);
        exit(0);        
      }
    }
    else
    /* will serve a page */
    {
      char *ipsrc = strdup(http_request_get_ip_src(req));
      if ( wikilogin_getpermission( ipsrc ) )
        /* serve the logoff & chg pwd page */
        wiki_show_loginfo_page( res, ipsrc );
      else
        /* serve the login page */
        wiki_show_login_page( res );
    }
  }
  else if (!strcmp(page, "NewAccount"))
  {
    /* serve the login page */
    wiki_show_newaccount_page(res);
  }
  else if (!strcmp(page, "Changes"))
  {
	//grant permission to show changes (to protect {privacy}} data)
    if ( (autorized=wikilogin_getpermission(http_request_get_ip_src(req))) )
    {
      if ( (page = http_request_param_get(req, "diff1")) )
        wiki_show_diff_between_pages(res, page, 1);
      else if ( (page = http_request_param_get(req, "diff2")) )
        wiki_show_diff_between_pages(res, page, 2);  
      else
        wiki_show_changes_page(res);
      autorized=0; //return to a secure mode
    }
    else
        wiki_page_forbiden(res); //err msg and exit
  }
  else if (!strcmp(page, "ChangesRss"))
  {
    wiki_show_changes_page_rss(res);
  }
  else if (!strcmp(page, "Index"))
  {
    if ( (folder=http_request_param_get(req, "Folder")) )
      wiki_show_index_page(res,folder);
    else
      wiki_show_index_page(res,".");
  }
  else if (!strcmp(page, "Search"))
  {
    wiki_show_search_results_page(res, http_request_param_get(req, "expr"));
  }
  else if (!strcmp(page, "Create"))
  /* unlogged user cannot create a page */
  {
    if ( (autorized=wikilogin_getpermission(http_request_get_ip_src(req))) )
    {
      if ( (wikitext = http_request_param_get(req, "title")) != NULL)
      {
        /* create page and redirect */
        wiki_redirect(res, http_request_param_get(req, "title"));
      }
      else
      {
         /* serve the create page form  */
        wiki_show_create_page(res);
      }
    }
    else
      wiki_page_forbiden(res); //err msg and exit
  }
  else
  {
    /* grant permission */
    autorized=wikilogin_getpermission(http_request_get_ip_src(req));
      
    if (access(page, R_OK) == 0) 
    /* page exists */
    {
      wikitext = file_read(page);
      /* log read page name and IP address */
      syslog(LOG_LOCAL0|LOG_INFO, "page %s viewed from %s", page, http_request_get_ip_src(req));
    } 
    else private=0; //page doesn't exit yet,so no privacy that allows to save the new page

    /* Add entry */
    if ( !strcmp(command, "entry") )
    {
      if ( http_request_param_get(req, "add") )
      {
        value = http_request_param_get(req, "datafield"); //need the field of data
        char *newdata = http_request_param_get(req, "data"); //data to add 
        wikitext = wiki_add_entry(page, newdata, value); //insert in the page
        file_write(page, wikitext);
      }
      else if ( http_request_param_get(req, "delete") )
      {
        listbox = http_request_checkbox_get(req);
        wikitext = wiki_delete_entry(page, listbox);
        file_write(page, wikitext);
      }
      else if ( http_request_param_get(req, "save") )
      {
        listbox = http_request_checkbox_get(req);
        wikitext = wiki_set_checkbox(page, listbox);
        file_write(page, wikitext);
      }
      
    }

    /* Permission is required */
    if ( !strcmp(command, "delete") )
    {
      if (autorized)
      {
        if (http_request_param_get(req, "confirm"))
        {
          unlink(page);
          wiki_redirect(res, "WikiHome");
        }
        else if (http_request_param_get(req, "cancel"))
        {
          wiki_redirect(res, page);
        }
        else
        {
          wiki_show_delete_confirm_page(res, page);
        }
      }
      else
        wiki_page_forbiden(res); //err msg and exit
    }
    /* permission can be required */
    else if ( !strcmp(command, "edit") || !strcmp(command, "create") )
    {
      if (autorized || !private)
      {
        char *newtext = http_request_param_get(req, "wikitext");

        if (http_request_param_get(req, "save") && newtext)
        {
          file_write(page, newtext);
          /* log modified page name and IP address */
          syslog(LOG_LOCAL0|LOG_INFO, "page %s modified from %s", page ,http_request_get_ip_src(req));
          wiki_redirect(res, page);
        }
        else if (http_request_param_get(req, "cancel"))
        {
          wiki_redirect(res, page);
        }

        if (http_request_param_get(req, "preview"))
        {
          wiki_show_edit_page(res, newtext, page, TRUE, TRUE);
        }
        else
        {
          wiki_show_edit_page(res, wikitext, page, FALSE, TRUE);
        }
      }
      else
        wiki_page_forbiden(res); //err msg and exit
    }
    else 
    /* there's no command */
    {
      if (wikitext)
      /* page exist, so just show the page */
      { 
        /* private is used to prevent edit/delete */
        private=wiki_show_page(res, wikitext, page, autorized);
      }
      else
      /* page doesn't exist, create it! */
      {
        if ( autorized )
        {
          char buf[1024];
          snprintf(buf, sizeof(buf), "%s?create", page);
          wiki_redirect(res, buf);
        }
        else
          wiki_page_forbiden(res); //err msg and exit
      }
    }
  } 
}

/* 1st function called only for the startup:
 * create the folders, create the help and home pages,
 * erase the previous login granted */
int
wiki_init(char *didiwiki_home, int restore_WikiHelp)
{
  char datadir[512] = { 0 };
  struct stat st;  

  /* look where the pages are/will be located */
  if (didiwiki_home)
  {
    /* datadir was passed as arg in the cmd line */
    snprintf(datadir, 512, "%s", didiwiki_home);
  }
  else 
  {
    if (getenv("DIDIWIKIHOME"))
    {
      /* datadir wad passed in the environment */
      snprintf(datadir, 512, "%s", getenv("DIDIWIKIHOME"));
    }
    else
    {
      /* default datadir */
      if (getenv("HOME") == NULL)
      {
        fprintf(stderr, "Unable to get home directory, is HOME set?\n");
        exit(1);
      }
      snprintf(datadir, 512, "%s/.didiwiki", getenv("HOME"));
    }
  }
  
  /* Check if datadir exists and create if not */
  if (stat(datadir, &st) != 0 )
  {
    if (mkdir(datadir, 0755) == -1)
    {
      fprintf(stderr, "Unable to create '%s', giving up.\n", datadir);
      exit(1);
    }
  }
  /* go to datadir */
  if ( chdir(datadir) )
    errorcnt++;
  
  /* Check if datadir/images exists and create if not */
  if (stat(PICSFOLDER, &st) != 0 )
  {
    if (mkdir(PICSFOLDER, 0755) == -1)
    {
      fprintf(stderr, "Unable to create '%s', giving up.\n",PICSFOLDER);
      exit(1);
    }
  }
  
  /* Check if datadir/permissions exists and create if not */
  if (stat(ACCESSFOLDER, &st) != 0 )
  {
    if (mkdir(ACCESSFOLDER, 0755) == -1)
    {
      fprintf(stderr, "Unable to create '%s', giving up.\n",ACCESSFOLDER);
      exit(1);
    }
  }
  
  /* Check if datadir/files exists and create if not */
  if (stat(FILESFOLDER, &st) != 0 )
  {
    if (mkdir(FILESFOLDER, 0755) == -1)
    {
      fprintf(stderr, "Unable to create '%s', giving up.\n",FILESFOLDER);
      exit(1);
    }
  }

  /* Write Default Help + Home page if it doesn't exist 
   * or rewrite Default Help */
  if ( access("WikiHelp", R_OK) != 0 || restore_WikiHelp )
    file_write("WikiHelp", HELPTEXT);

  if (access("WikiHome", R_OK) != 0 || restore_WikiHelp ) 
    file_write("WikiHome", HOMETEXT);

  /* Read in optional CSS data */
  if (access("styles.css", R_OK) == 0) 
    CssData = file_read("styles.css");
    
  /* Delete previous permission list */
  remove(ACCESSFOLDER"/.session.txt");
  
  return 1;
}

