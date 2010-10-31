/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
//
// C++ Implementation: delivermv
//
// Description: 
//
//
// Author: simon <sglass@kiwi>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//


#include <iostream>
#include <getopt.h>

#include <QApplication>
#include <QString>

#include "config.h"
#include "err.h"
#include "delivery.h"
#include "transfer.h"


static void usage (void)
   {
   std::cout << "delivermv - maxview wide area delivery agent\n\n";
   printf ("(C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net, v%s\n\n", CONFIG_version_str);
   printf ("Usage:  delivermv <opts>\n\n");
   printf ("   -h|--help       display this usage information\n");
   printf ("   -i|--info       display queue information\n");
   printf ("   -r|--root       add transfer root to list\n");
   printf ("   -s|--server     start a maxview server and wait for connections\n");
   printf ("\n");
   }


bool err_complain (err_info *err)
   {
   if (err)
      printf ("Error: %s\n", err->errstr);
   return err != NULL;
   }


static err_info *do_deliver (Delivery &del)
   {

   return NULL;
   }


static void do_collect (void)
   {
//   Transfer transfer;


   }


static err_info *do_info (Delivery &del)
   {
   CALL (del.scan ());
   del.showQueues ();
   return NULL;
   }


static err_info *do_server (Delivery &del)
   {
   del.server ();
   return NULL;
   }


int main (int argc, char *argv[])
   {
   static struct option long_options[] = {
     {"help", 0, 0, 'h'},
     {"deliver", 0, 0, 'd'},
     {"info", 0, 0, 'i'},
     {"collect", 0, 0, 'c'},
     {"root", 1, 0, 'r'},
     {"server", 0, 0, 's'},
     {0, 0, 0, 0}
   };
   int op_type = -1, c;
   QString index;
   bool bad = false;

   Delivery del;

   while (c = getopt_long (argc, argv,
      "h?discr:", long_options, NULL), c != -1)
      switch (c)
         {
         case 'h' :
         case '?' :
            bad = true;

         case 'c' :
            op_type = 'c';
            break;
   
         case 'd' :
            op_type = 'd';
            break;
   
         case 'i' :
            op_type = 'i';
            break;

         case 'r' :
            del.addRoot (optarg);
            break;
  
         case 's' :
            op_type = 's';
            break;
         }

   if (bad || op_type == -1)
      {
      usage ();
      return 1;
      }
   if (!del.rootCount ())
      printf ("Warning: no transfer roots defined\n");
   QApplication app (argc, argv, false);

   switch (op_type)
      {
      case 'd' :
         err_complain (do_deliver (del));
         break;

      case 'c' :
         do_collect ();
         break;
  
      case 'i' :
         err_complain (do_info (del));
         break;
  
      case 's' :
         err_complain (do_server (del));
         break;
      }
   }

