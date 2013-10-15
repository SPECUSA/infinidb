/* Copyright (C) 2013 Calpont Corp.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation;
   version 2.1 of the License.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA. */

/***********************************************************************
*   $Id: createtable.cpp 9210 2013-01-21 14:10:42Z rdempsey $
*
*
***********************************************************************/

#include <iostream>

#define DDLPKG_DLLEXPORT
#include "ddlpkg.h"
#undef DDLPKG_DLLEXPORT

namespace ddlpackage {

	using namespace std;
	
	CreateTableStatement::CreateTableStatement() :
		fTableDef(0)
	{
	}
	
	CreateTableStatement::CreateTableStatement(TableDef* tableDef) :
		fTableDef(tableDef)
	{
	}

		
	CreateTableStatement::~CreateTableStatement()
	{
		if (fTableDef)
		{
			delete fTableDef;
		}
	}

	/** \brief Put to ostream. */
	ostream& CreateTableStatement::put(ostream& os) const
	{
		os << "CreateTable "
		   << *fTableDef;
		return os;
	}
}
