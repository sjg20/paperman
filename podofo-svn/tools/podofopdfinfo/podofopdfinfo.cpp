/***************************************************************************
 *   Copyright (C) 2005 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <iostream>
#include <PdfDefines.h>
#include <util/PdfMutex.h>
#include "pdfinfo.h"

#include <stdlib.h>

#ifdef _HAVE_CONFIG
#include <config.h>
#endif // _HAVE_CONFIG

void print_help()
{
  printf("Usage: podofopdfinfo [DCPON] [inputfile] \n\n");
  printf("       This tool displays information about the PDF file\n");
  printf("       according to format instruction (if not provided, displays all).\n");
  printf("       D displays Document Info.\n");
  printf("       C displays Classic Metadata.\n");
  printf("       P displays Page Info.\n");
  printf("       O displays Outlines.\n");
  printf("       N displays Names.\n");
}

struct Format {
	bool document; // D
	bool classic; // C
	bool pages; // P
	bool outlines; // O
	bool names; // N
	Format():document(true),classic(true),pages(true),outlines(true),names(true){}
};

Format ParseFormat(const std::string& fs)
{
	Format ret;
	
	if(fs.find('D') == std::string::npos)
		ret.document = false;
	
	if(fs.find('C') == std::string::npos)
		ret.classic = false;
	
	if(fs.find('P') == std::string::npos)
		ret.pages = false;
	
	if(fs.find('O') == std::string::npos)
		ret.outlines = false;
	
	if(fs.find('N') == std::string::npos)
		ret.names = false;
	
	return ret;
}

int main( int argc, char* argv[] )
{
#if 1
  PoDoFo::PdfError::EnableDebug( false );	// turn it off to better view the output from this app!
  PoDoFo::PdfError::EnableLogging( false );
#endif

#if 0
  printf("Multithreaded PoDoFo: %i\n", PoDoFo::Util::PdfMutex::IsPoDoFoMultiThread() );
#endif
			 
  if( (argc == 1) ||  (argc > 3) )
  {
    print_help();
    exit( -1 );
  }

  
  char*    pszInput;
  Format format;
  if(argc == 2)
  {
	pszInput  = argv[1];
  }
  else if(argc == 3)
  {
	  pszInput  = argv[2];
	  format = ParseFormat(std::string(argv[1]));
  }
  std::string fName( pszInput );

  try {
      PdfInfo	myInfo( fName );

      if(format.document)
      {
      std::cout << "Document Info" << std::endl;
      std::cout << "-------------" << std::endl;
      std::cout << "\tFile: " << fName << std::endl;
      myInfo.OutputDocumentInfo( std::cout );
      std::cout << std::endl;
      }
      
      if(format.classic)
      {
      std::cout << "Classic Metadata" << std::endl;
      std::cout << "----------------" << std::endl;
      myInfo.OutputInfoDict( std::cout );
      std::cout << std::endl;
      }
      
      if(format.pages)
      {
      std::cout << "Page Info" << std::endl;
      std::cout << "---------" << std::endl;
      myInfo.OutputPageInfo( std::cout );
      }
      
      if(format.outlines)
      {
      std::cout << "Outlines" << std::endl;
      std::cout << "--------" << std::endl;
      myInfo.OutputOutlines( std::cout );
      }
      
      if(format.names)
      {
      std::cout << "Names" << std::endl;
      std::cout << "-----" << std::endl;
      myInfo.OutputNames( std::cout );
      }

  } catch( PoDoFo::PdfError & e ) {
      fprintf( stderr, "Error: An error %i ocurred during uncompressing the pdf file.\n", e.GetError() );
      e.PrintErrorMsg();
      return e.GetError();

  }
  
//   std::cerr << "All information written sucessfully.\n" << std::endl << std::endl;

  return 0;
}

