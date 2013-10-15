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

/*************************
*
* $Id: ddlfuncs.cpp 9210 2013-01-21 14:10:42Z rdempsey $
*
*************************/

#include "ddlfuncs.h"
#include "checkerr.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace std;

#ifndef OCI_ORACLE
# include <oci.h>
#endif
#ifndef ODCI_ORACLE
# include <odci.h>
#endif

#include "bytestream.h"
#include "messagequeue.h"
using namespace messageqcpp;

#include "ddlpkg.h"
#include "sqlparser.h"
using namespace ddlpackage;

#include "createtableprocessor.h"
#include "createindexprocessor.h"
#include "altertableprocessor.h"
#include "dropindexprocessor.h"
#include "droptableprocessor.h"
using namespace ddlpackageprocessor;

#include "calpontsystemcatalog.h"
using namespace execplan;

#include "messagelog.h"
#include "messageobj.h"
#include "messageids.h"
using namespace logging;

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;

namespace {

MessageLog* ml = 0;
//OCISvcCtx *svchp;
void pause_()
{
	struct timespec req;
	struct timespec rem;

	req.tv_sec = 1;
	req.tv_nsec = 0;

	rem.tv_sec = 0;
	rem.tv_nsec = 0;

again:
	if (nanosleep(&req, &rem) != 0)
		if (rem.tv_sec > 0 || rem.tv_nsec > 0)
		{
			req = rem;
			goto again;
		}
}

const string checkConstraint(const string& str, string::size_type pos)
{
    string chk = "";
    
    // increment num when get '(' and decrement when get ')'
    // to find the mathing ')' when num = 0
    int num = 1;
    for (; pos < str.length(); )
    {
        if (str[pos] == '(')
            num++;
        else if (str[pos] == ')')
            num--;
        if (num == 0) 
        {
            pos++;
            return chk;
        }          
        chk.push_back(str[pos++]);
    }
    
    string msg = "No ')' found in";
    msg.append(str);    
    throw invalid_argument ( msg );
    return chk;
}

}

extern "C" int Process_ddl(OCIExtProcContext* extProcCtx,
	OCINumber* sessionIDn,
	short sessionIDn_ind,
	char* sOwner, short sOwner_ind,
	char* ddltext, short ddltext_ind,
	ColValueSet** colValList, short* colValList_ind)
{
	char errbuf[1024];
	Message::Args args;
	Message ddlStmtMsg(M0013);
	Message commError(M0056);
	Message DDLProcError(M0057);
    
	if (sessionIDn_ind == OCI_IND_NULL ||
		sOwner_ind == OCI_IND_NULL 
		) return -1;

    string ddlStatement(ddltext);
    const string owner(sOwner);
    
    // Remove Oracle generated keyword. Also indicate this is a primary key creation
    if (ddlStatement.find("NOPARALLEL") != string::npos)
    {
        ddlStatement.erase(ddlStatement.find("NOPARALLEL"), 10);
        
        // remove quotes in the sql statement in case it's generated by Oracle
        // Need to watch for delimited identifier
        for (unsigned int i = 0; i < ddlStatement.length(); i++)
        {
            if (ddlStatement.at(i) == 0x22) // ''''
                ddlStatement.erase(i, 1);
        }
    }
        
    // replace shadow user name with owner name. Assume all upper case
    string ddl = ddlStatement;
    to_upper(ddl);
    string shadowUser (sOwner);
    shadowUser = "S_" + shadowUser;
    string::size_type pos = (string::size_type)ddl.find(shadowUser, 0);
    
    while (pos != string::npos)
    {
        ddlStatement.replace(pos, shadowUser.length(), owner);
        ddl.replace(pos, shadowUser.length(), owner);
        pos = ddl.find(shadowUser, pos);
    }            
    
	Handles_t handles;
	ub4 sessionID;

	if (GetHandles(extProcCtx, &handles)) return -2;
	

	if (OCINumberToInt(handles.errhp, sessionIDn, sizeof(sessionID), OCI_NUMBER_UNSIGNED,
		(dvoid *)&sessionID) != OCI_SUCCESS)
		return -3;

	if (ml == 0) ml = new MessageLog(LoggingID(24, sessionID, 0, 1));

	if (ddlStatement.length() == 0)
	{
		strcpy(errbuf, "Error retrieving SQL statement");
		OCIExtProcRaiseExcpWithMsg(extProcCtx, 9998, (text*)errbuf, strlen(errbuf));
		return -4;
	}

    // trick the parser by changing '()' for in to '{}'. need to fix parser later
    ddl = ddlStatement;
    to_upper(ddl);
    string::size_type inPos;
    if ((inPos = ddl.find(" IN(")) != string::npos || (inPos = ddl.find(" IN ")) != string::npos)
    {
        inPos = ddl.find("(", inPos);
        ddlStatement.replace(inPos, 1, "{");
        inPos = ddl.find(")", inPos);
        ddlStatement.replace(inPos, 1, "}");
    }
     
#if 0
    /* @brief handle IN operator in check constraint
     * it's not currently handled by the ddl parser.  
     * frontend converts check (col in(a, b, c)) to check(col=a or col=b or col=c)
     */
    ddl = ddlStatement;
    to_upper(ddl);

    if ((ddl.find(" IN(") != string::npos || ddl.find(" IN ") != string::npos) &&
        (ddl.find(" CHECK(") != string::npos || ddl.find(" CHECK ") != string::npos))
    {
        // add space after in if there's no
        string convertedChkConst;
        string chkConstraint;
        string::size_type chkPos;
        
        if ((chkPos = ddl.find(" IN("))!= string::npos)
            ddlStatement.insert (chkPos+3, " ");
        chkPos = ddl.find(" CHECK(");
        if (chkPos == string::npos) chkPos = ddl.find(" CHECK ");
        ddl = ddlStatement; 
        chkPos = ddl.find("(", chkPos) + 1;
        chkConstraint = checkConstraint(ddl, chkPos);
        string::size_type pos1 = 0;
        string::size_type pos2 = 0;
        string::size_type pos3 = 0;    
        
        // get chkTok in ()
        string chkTok;
        pos1 = chkConstraint.find("(");
        chkTok = checkConstraint(chkConstraint, pos1+1);
        pos1 = 0;
        
        // try to replace the comma in '' in order not to confuse the tokenizer
        while (pos2 < chkTok.length() && pos1 != string::npos)
        {
            pos1 = chkTok.find_first_of("'", pos2);
            pos2 = chkTok.find_first_of("'", pos1+1);
            pos3 = chkTok.find_first_of(",", pos1);
            if (pos3 < pos2)
                chkTok.replace(pos3, 1, "^");
            pos2++;
        }
        
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
        boost::char_separator<char> sep(" ");
        tokenizer tokens(chkConstraint, sep);
        tokenizer::iterator tok_iter = tokens.begin();
        
        string col = (*tok_iter++);
        bool notin = false;
        string op = "=";
        string logicalOp = " OR ";
        
        if (strcasecmp((*tok_iter).c_str(),"IN") != 0)
        {
            if (strcasecmp((*tok_iter++).c_str(), "NOT") == 0 && 
                strcasecmp((*tok_iter).c_str(), "IN") == 0 )
                notin = true;
            else
            {                
                ml->logWarningMessage(ddlStmtMsg);
    		    strcpy(errbuf, "Error pre-parsing DDL statement for check constraint");
    		    OCIExtProcRaiseExcpWithMsg(extProcCtx, 9999, (text*)errbuf, strlen(errbuf));
    		    return -5;
    		}
        }
        
        if (notin)
        {
            op = "!=";
            logicalOp = " AND ";
        }
        
        boost::char_separator<char> sep1(",");
        tokenizer tokens1(chkTok, sep1);
        tok_iter = tokens1.begin();
        convertedChkConst = col + op + (*tok_iter++);
        
        for (; tok_iter != tokens1.end(); ++tok_iter)
        {
            convertedChkConst = convertedChkConst + logicalOp;
            convertedChkConst = convertedChkConst + col + op + (*tok_iter);
        }
        
        // recover '^' to ','
        for (string::size_type i = 0; i < convertedChkConst.length(); i++)
            if (convertedChkConst[i] == '^') convertedChkConst[i] = ',';
        
        ddlStatement.replace(chkPos, chkConstraint.length(), convertedChkConst);
    }
#endif
    //@bug 399. Replace drop primary key with drop constraint.
	ddl = ddlStatement;
    to_upper(ddl);
    //Strip off leading space
    string::size_type startPlace = ddl.find_first_not_of(" ");
    ddl = ddl.substr( startPlace, ddl.length()-startPlace);
    //Check whether it is alter table statement
    string::size_type blankspace = ddl.find(" ",0);
    string sqlCommand = ddl.substr(0, blankspace);
    if(strcasecmp(sqlCommand.c_str(), "ALTER") == 0)
    {
    	startPlace = ddl.find(" DROP ");
    	if ( startPlace < string::npos )
    	{
    		startPlace = ddl.find( "PRIMARY" );
    		if ( startPlace < string::npos )
    		{
    			string::size_type stopPlace = ddl.find( "KEY" );
    			std::string constraintName;
    			std::string tableName;
    			//Find table name
    			typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    			boost::char_separator<char> sep( " ;"); 
    			tokenizer tokens(ddl, sep);
    			std::vector<std::string> dataList;
    			tokenizer::iterator tok_iter = tokens.begin();
    			++tok_iter;
    			++tok_iter;
    			tableName = *tok_iter;
    			string::size_type dotPlace;
    			CalpontSystemCatalog::TableName qualifiedTblName;
    			dotPlace = tableName.find( ".");
    			if ( dotPlace < string::npos )
    			{
    				qualifiedTblName.schema = tableName.substr(0, dotPlace);
    				qualifiedTblName.table = tableName.substr(dotPlace,tableName.length()-dotPlace-1 );
    			}
    			else
    			{
    				qualifiedTblName.schema = owner;
    				qualifiedTblName.table = tableName;
    			}
    			CalpontSystemCatalog *csc = CalpontSystemCatalog::makeCalpontSystemCatalog(sessionID);
    			csc->identity( CalpontSystemCatalog::FE );
    			constraintName = csc->primaryKeyName( qualifiedTblName );
    			
    			constraintName = " constraint " + constraintName;
    			ddlStatement.replace(startPlace, stopPlace - startPlace + 3, constraintName);   			
    		}
    	}    	
    	
    }       
	args.reset();
	args.add(ddlStatement);
	ddlStmtMsg.format(args);
	ml->logDebugMessage(ddlStmtMsg);

    SqlParser parser;
    parser.setDefaultSchema(owner);

    parser.Parse(ddlStatement.c_str());
    if (!parser.Good())
	{
		ml->logWarningMessage(ddlStmtMsg);
		strcpy(errbuf, "Error parsing DDL statement");
		OCIExtProcRaiseExcpWithMsg(extProcCtx, 9999, (text*)errbuf, strlen(errbuf));
		return -5;
	}    
    
    const ParseTree &ptree = parser.GetParseTree();
    SqlStatement &stmt = *ptree.fList[0];
    stmt.fSessionID = sessionID;
    stmt.fSql = ddlStatement;
    stmt.fOwner = shadowUser;
    
    //Check if the statement is alter table add column,. If it is, fill in the column information
    if ( typeid ( stmt ) == typeid ( AlterTableStatement ) )
    {
    	AlterTableStatement &alterTableStmt = *(dynamic_cast<AlterTableStatement*> ( &stmt ) );
    	ddlpackage::AlterTableActionList actionList = alterTableStmt.fActions;
    	AlterTableActionList::const_iterator action_iterator = actionList.begin();
    	sword retcode = 0;
    	
    	while ( action_iterator != actionList.end() )
    	{
        	std::string s(typeid(*(*action_iterator)).name());

        	if ( s.find("AtaAddColumn") != string::npos )
        	{
        		colValue aColValue;  /* current collection element */ 
    			colValue_ind  aColValueInd; /*current element indicator */  				
      			//bug 827:change AtaAddColumn to AtaAddColumns	
      			//Bug 1192
      			ddlpackage::ColumnDef* columnDefPtr = 0;
      			ddlpackage::AtaAddColumn *addColumn = dynamic_cast<AtaAddColumn*> (*action_iterator);
      			if ( addColumn )
      			{
      				columnDefPtr = addColumn->fColumnDef;
      			}
      			else
      			{
      				ddlpackage::AtaAddColumns& addColumns = *(dynamic_cast<AtaAddColumns*> (*action_iterator));
      				columnDefPtr = addColumns.fColumns[0];
      			}
      				
            	int dType = columnDefPtr->fType->fType;           	
            	ub2 dataLen = columnDefPtr->fName.length();
            	
            	/* Initialize the element indicator struct */
    			aColValueInd._atomic=OCI_IND_NOTNULL;
    			aColValueInd.column_name=OCI_IND_NOTNULL;
    			aColValueInd.data_type=OCI_IND_NOTNULL;
    			aColValueInd.data_length=OCI_IND_NOTNULL;
    			aColValueInd.data_precision=OCI_IND_NOTNULL;

				/*Assign column_name */
				aColValue.column_name = NULL;
 				retcode = OCIStringAssignText( handles.envhp,handles.errhp,(text*)(columnDefPtr->fName.c_str()), dataLen, &(aColValue.column_name) );
 				      	
            	/*Assign data_type */
            	aColValue.data_type = NULL;
            	//@Bug 1423. Added the handlig of timestamp data type.
            	string typeStr;
            	if ( dType == DDL_DATETIME )
            		typeStr = "timestamp";
            	else 
            		typeStr = DDLDatatypeString[dType];
            	dataLen = typeStr.length();	
            	retcode = OCIStringAssignText( handles.envhp,handles.errhp,(text*)typeStr.c_str(), dataLen, &(aColValue.data_type) );   
            	/*Assign data_length */               	            	
            	retcode = OCINumberFromInt( handles.errhp, &(columnDefPtr->fType->fLength), sizeof(columnDefPtr->fType->fLength),OCI_NUMBER_SIGNED, &(aColValue.data_length) );
            	
            	/* Assign data_precision */      
            	if ( columnDefPtr->fType->fPrecision < 0 )
            	{
            		columnDefPtr->fType->fPrecision = 0;
            	}   	
            	retcode = OCINumberFromInt( handles.errhp, &(columnDefPtr->fType->fPrecision), sizeof(columnDefPtr->fType->fPrecision),OCI_NUMBER_SIGNED, &( aColValue.data_precision ) );
            	
            	/* Assign data_scale */ 
            	if ( columnDefPtr->fType->fScale < 0 )
            	{
            		columnDefPtr->fType->fScale = 0;
            	}
            	retcode = OCINumberFromInt( handles.errhp, &(columnDefPtr->fType->fScale), sizeof(columnDefPtr->fType->fScale),OCI_NUMBER_SIGNED, &( aColValue.data_scale ) );    
            	
            	/* append row to output collection */
    			retcode = OCICollAppend(handles.envhp, handles.errhp,&aColValue, &aColValueInd, *colValList);   
    			
    			/* set collection indicator to not null */
    			*colValList_ind=OCI_IND_NOTNULL;
    			
    			action_iterator++;
                   
        	}
        	else
        	{
        		action_iterator++;
        	 	continue;
        	}
		}
		
    }
    
	ByteStream bytestream;
	bytestream << sessionID;
	stmt.serialize(bytestream);
	MessageQueueClient mq("DDLProc");
	ByteStream::byte b;
	std::string errorMsg;
	try
	{
		mq.write(bytestream);
		bytestream = mq.read();
		bytestream >> b;
		bytestream >> errorMsg;
	}
	catch (runtime_error& rex)
	{
		args.reset();
		args.add(rex.what());
		commError.format(args);
		ml->logSeriousMessage(commError);
		sprintf(errbuf, "Error communicating with Engine Controller: %s", rex.what());
		OCIExtProcRaiseExcpWithMsg(extProcCtx, 9999, (text*)errbuf, strlen(errbuf));
		return -6;
	}
	catch (...)
	{
		args.reset();
		args.add("caught unknown exception");
		commError.format(args);
		ml->logSeriousMessage(commError);
		strcpy(errbuf, "Error communicating with Engine Controller");
		OCIExtProcRaiseExcpWithMsg(extProcCtx, 9999, (text*)errbuf, strlen(errbuf));
		return -7;
	}

	if (b != 0)
	{
		args.reset();
		args.add(ddlStatement);
		args.add(errorMsg);
		DDLProcError.format(args);
		ml->logCriticalMessage(DDLProcError);
		sprintf(errbuf, "%s", errorMsg.c_str());
		OCIExtProcRaiseExcpWithMsg(extProcCtx, 29400, (text*)errbuf, strlen(errbuf));
		return -8;
	}

	return 0;
}

