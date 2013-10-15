/* Copyright (C) 2013 Calpont Corp.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA. */

#ifndef FEMSGHANDLER_H_
#define FEMSGHANDLER_H_

#include "joblist.h"
#include "inetstreamsocket.h"

class FEMsgHandler
{
public:
	FEMsgHandler();
	FEMsgHandler(boost::shared_ptr<joblist::JobList>, messageqcpp::IOSocket *);
	virtual ~FEMsgHandler();

	void start();
	void stop();
	void setJobList(boost::shared_ptr<joblist::JobList>);
	void setSocket(messageqcpp::IOSocket *);
	bool aborted();

	void threadFcn();

private:
	bool die, running, sawData;
	messageqcpp::IOSocket *sock;
	boost::shared_ptr<joblist::JobList> jl;
	boost::thread thr;
	boost::mutex mutex;
};

#endif /* FEMSGHANDLER_H_ */
