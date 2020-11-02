/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesLongMessageForm.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "../cmCacheManager.h"
#include "../cmSystemTools.h"
#include "../cmake.h"
#include "../cmVersion.h"
#include "cmCursesLongMessageForm.h"
#include "cmCursesMainForm.h"

inline int ctrl(int z)
{
    return (z&037);
} 

cmCursesLongMessageForm::cmCursesLongMessageForm(std::vector<std::string> 
                                                 const& messages, const char* 
                                                 title)
{
  // Append all messages into on big string
  std::vector<std::string>::const_iterator it;
  for(it=messages.begin(); it != messages.end(); it++)
    {
    this->Messages += (*it);
    // Add one blank line after each message
    this->Messages += "\n\n";
    }
  this->Title = title;
  this->Fields[0] = 0;
  this->Fields[1] = 0;
}

cmCursesLongMessageForm::~cmCursesLongMessageForm()
{
  if (this->Fields[0])
    {
    free_field(this->Fields[0]);
    }
}


void cmCursesLongMessageForm::UpdateStatusBar()
{
  int x,y;
  getmaxyx(stdscr, y, x);

  char bar[cmCursesMainForm::MAX_WIDTH];
  int size = strlen(this->Title.c_str());
  if ( size >= cmCursesMainForm::MAX_WIDTH )
    {
    size = cmCursesMainForm::MAX_WIDTH-1;
    }
  strncpy(bar, this->Title.c_str(), size);
  for(int i=size-1; i<cmCursesMainForm::MAX_WIDTH; i++) bar[i] = ' ';

  int width;
  if (x < cmCursesMainForm::MAX_WIDTH )
    {
    width = x;
    }
  else
    {
    width = cmCursesMainForm::MAX_WIDTH;
    }

  bar[width] = '\0';

  char version[cmCursesMainForm::MAX_WIDTH];
  char vertmp[128];
  sprintf(vertmp,"CMake Version %d.%d - %s", cmVersion::GetMajorVersion(),
          cmVersion::GetMinorVersion(),cmVersion::GetReleaseVersion().c_str());
  int sideSpace = (width-strlen(vertmp));
  for(int i=0; i<sideSpace; i++) { version[i] = ' '; }
  sprintf(version+sideSpace, "%s", vertmp);
  version[width] = '\0';

  curses_move(y-4,0);
  attron(A_STANDOUT);
  printw(bar);
  attroff(A_STANDOUT);  
  curses_move(y-3,0);
  printw(version);
  pos_form_cursor(this->Form);
}

void cmCursesLongMessageForm::PrintKeys()
{
  int x,y;
  getmaxyx(stdscr, y, x);
  if ( x < cmCursesMainForm::MIN_WIDTH  || 
       y < cmCursesMainForm::MIN_HEIGHT )
    {
    return;
    }
  char firstLine[512];
  sprintf(firstLine,  "Press [e] to exit help");

  curses_move(y-2,0);
  printw(firstLine);
  pos_form_cursor(this->Form);
  
}

void cmCursesLongMessageForm::Render(int, int, int, int)
{
  int x,y;
  getmaxyx(stdscr, y, x);

  if (this->Form)
    {
    unpost_form(this->Form);
    free_form(this->Form);
    this->Form = 0;
    }

  const char* msg = this->Messages.c_str();

  curses_clear();

  if (this->Fields[0])
    {
    free_field(this->Fields[0]);
    this->Fields[0] = 0;
    }

  this->Fields[0] = new_field(y-6, x-2, 1, 1, 0, 0);

  field_opts_off(this->Fields[0],  O_STATIC);

  this->Form = new_form(this->Fields);
  post_form(this->Form);

  int i=0;
  form_driver(this->Form, REQ_BEG_FIELD);
  while(msg[i] != '\0' && i < 60000)
    {
    if (msg[i] == '\n' && msg[i+1] != '\0')
      {
      form_driver(this->Form, REQ_NEW_LINE);
      }
    else
      {
      form_driver(this->Form, msg[i]);
      }
    i++;
    }
  form_driver(this->Form, REQ_BEG_FIELD);

  this->UpdateStatusBar();
  this->PrintKeys();
  touchwin(stdscr); 
  refresh();

}

void cmCursesLongMessageForm::HandleInput()
{
  if (!this->Form)
    {
    return;
    }

  char debugMessage[128];

  for(;;)
    {
    int key = getch();

    sprintf(debugMessage, "Message widget handling input, key: %d", key);
    cmCursesForm::LogMessage(debugMessage);

    // quit
    if ( key == 'o' || key == 'e' )
      {
      break;
      }
    else if ( key == KEY_DOWN || key == ctrl('n') )
      {
      form_driver(this->Form, REQ_SCR_FLINE);
      }
    else if ( key == KEY_UP  || key == ctrl('p') )
      {
      form_driver(this->Form, REQ_SCR_BLINE);
      }
    else if ( key == KEY_NPAGE || key == ctrl('d') )
      {
      form_driver(this->Form, REQ_SCR_FPAGE);
      }
    else if ( key == KEY_PPAGE || key == ctrl('u') )
      {
      form_driver(this->Form, REQ_SCR_BPAGE);
      }

    this->UpdateStatusBar();
    this->PrintKeys();
    touchwin(stdscr); 
    wrefresh(stdscr); 
    }

}
