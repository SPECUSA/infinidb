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
*   $Id: returnedcolumn.h 9679 2013-07-11 22:32:03Z zzhu $
*
*
***********************************************************************/
/** @file */

#ifndef RETURNEDCOLUMN_H
#define RETURNEDCOLUMN_H
#include <string>
#include <iosfwd>
#include <vector>
#include <cmath>
#include <boost/shared_ptr.hpp>

#include "treenode.h"
#include "calpontsystemcatalog.h"

namespace messageqcpp {
	class ByteStream;
}

namespace rowgroup {
	class Row;
}

/**
 * @brief Namespace
 */
namespace execplan {

// Join info bit mask
//const uint64_t JOIN_OUTER = 0x0001;
const uint64_t JOIN_SEMI = 0x0002;
const uint64_t JOIN_ANTI = 0x0004;
const uint64_t JOIN_SCALAR = 0x0008;
const uint64_t JOIN_NULL_MATCH = 0x0010;
const uint64_t JOIN_CORRELATED = 0x0020;
const uint64_t JOIN_NULLMATCH_CANDIDATE = 0x0040;
const uint64_t JOIN_OUTER_SELECT = 0x0080;

// column source bit mask
const uint64_t FROM_SUB = 0x0002;
const uint64_t SELECT_SUB = 0x0004;
const uint64_t CORRELATED_JOIN = 0x0008;

/**
 * @brief fwd ref
 */
class SimpleColumn;
class AggregateColumn;
class WindowFunctionColumn;
class ReturnedColumn;

typedef boost::shared_ptr<ReturnedColumn> SRCP;

/**
 * @brief class ReturnedColumn
 */
class ReturnedColumn : public TreeNode {

public:

	/**
	 * Constructors
	 */
	ReturnedColumn();
	ReturnedColumn(const std::string& sql);
	ReturnedColumn(const u_int32_t sessionID, const bool returnAll = false);
	ReturnedColumn(const ReturnedColumn& rhs, const u_int32_t sessionID = 0);

	/**
	 * Destructors
	 */
	virtual ~ReturnedColumn();

	/**
	 * Accessor Methods
	 */
	virtual const std::string data() const;
	virtual void data(const std::string data) { fData = data; }

	virtual const bool returnAll() const {return fReturnAll;}
	virtual void returnAll(const bool returnAll) { fReturnAll = returnAll; }

	const u_int32_t sessionID() const {return fSessionID;}
	void sessionID(const u_int32_t sessionID) { fSessionID = sessionID; }

	inline const uint32_t sequence() const {return fSequence;}
	inline void sequence(const uint32_t sequence) {fSequence = sequence;}

	inline const std::string& alias() const { return fAlias; }
	inline void alias(const std::string& alias) {  fAlias = alias; }

	virtual uint64_t cardinality() const { return fCardinality; }
	virtual void cardinality( const uint64_t cardinality) { fCardinality = cardinality; }

	virtual bool distinct() const { return fDistinct;}
	virtual void distinct(const bool distinct) { fDistinct=distinct; }

	const uint expressionId() const { return fExpressionId; }
	void expressionId(const uint expressionId) { fExpressionId = expressionId; }

	virtual uint64_t joinInfo() const { return fJoinInfo; }
	virtual void joinInfo(const uint64_t joinInfo) { fJoinInfo = joinInfo; }

	virtual const bool asc() const { return fAsc; }
	virtual void asc(const bool asc) { fAsc = asc; }

	virtual const bool nullsFirst() const { return fNullsFirst; }
	virtual void nullsFirst(const bool nullsFirst) { fNullsFirst = nullsFirst; }

	virtual uint64_t orderPos() const { return fOrderPos; }
	virtual void orderPos(const uint64_t orderPos) { fOrderPos = orderPos; }

	virtual uint64_t colSource() const { return fColSource; }
	virtual void colSource(const uint64_t colSource) { fColSource = colSource; }
	
	virtual uint64_t colPosition() const { return fColPosition; }
	virtual void colPosition(const uint64_t colPosition) { fColPosition = colPosition; }

	/**
	 * Operations
	 */
	virtual ReturnedColumn* clone() const = 0;

	/**
	 * The serialize interface
	 */
	virtual void serialize(messageqcpp::ByteStream&) const;
	virtual void unserialize(messageqcpp::ByteStream&);

	virtual const std::string toString() const;

	/** @brief Do a deep, strict (as opposed to semantic) equivalence test
	 *
	 * Do a deep, strict (as opposed to semantic) equivalence test.
	 * @return true iff every member of t is a duplicate copy of every member of this; false otherwise
		 */
	virtual bool operator==(const TreeNode* t) const;

	/** @brief Do a deep, strict (as opposed to semantic) equivalence test
	 *
	 * Do a deep, strict (as opposed to semantic) equivalence test.
	 * @return true iff every member of t is a duplicate copy of every member of this; false otherwise
	 * @warning this member function is NOT virtual (why?)
	 */
	bool operator==(const ReturnedColumn& t) const;

	/** @brief Do a deep, strict (as opposed to semantic) equivalence test
	 *
	 * Do a deep, strict (as opposed to semantic) equivalence test.
	 * @return false iff every member of t is a duplicate copy of every member of this; true otherwise
	 */
	virtual bool operator!=(const TreeNode* t) const;

	/** @brief Do a deep, strict (as opposed to semantic) equivalence test
	 *
	 * Do a deep, strict (as opposed to semantic) equivalence test.
	 * @return false iff every member of t is a duplicate copy of every member of this; true otherwise
	 * @warning this member function is NOT virtual (why?)
	 */
	bool operator!=(const ReturnedColumn& t) const;

	/** @brief check if this column is the same as the argument
	 *
	 * @note Why can't operator==() be used?
	 */
	virtual bool sameColumn(const ReturnedColumn* rc) const
	{return (fData.compare(rc->data()) == 0);}

	// get all simple columns involved in this expression                                           
	const std::vector<SimpleColumn*>& simpleColumnList() const
	{ return fSimpleColumnList; }

	// get all aggregate column list in this expression
	const std::vector<AggregateColumn*>& aggColumnList() const
	{ return fAggColumnList; }
	
	// get all window function column list in this expression
	const std::vector<WindowFunctionColumn*>& windowfunctionColumnList() const
	{ return fWindowFunctionColumnList; }
	
	/* @brief if this column is or contains aggregate column
	 *
	 * @note after this function call fAggColumnList is populated
	 */
	virtual bool hasAggregate() = 0;
	
	/* @brief if this column is or contains window function column
	 *
	 * @note after this function call fWindowFunctionColumnList is populated
	 */
	virtual bool hasWindowFunc() = 0;

protected:
	// return all flag set if the other column is outer join column (+)
	bool fReturnAll;
	uint32_t fSessionID;
	uint32_t fSequence;                 /// column sequence on the SELECT mapped to the correlated joins
	uint64_t fCardinality;
	std::string fAlias;                 /// column alias
	bool fDistinct;
	uint64_t fJoinInfo;
	bool fAsc;                          /// for order by column
	bool fNullsFirst;                   /// for window function
	uint64_t fOrderPos;                 /// for order by and group by column
	uint64_t fColSource;                /// from which subquery
	uint64_t fColPosition;              /// the column position in the source subquery 
	std::vector<SimpleColumn*> fSimpleColumnList;
	std::vector<AggregateColumn*> fAggColumnList;
	std::vector<WindowFunctionColumn*> fWindowFunctionColumnList;

private:
	std::string fData;

/******************************************************************
 *                   F&E framework                                *
 ******************************************************************/
public:
	const uint inputIndex() const { return fInputIndex; }
	void inputIndex ( const uint inputIndex ) { fInputIndex = inputIndex; }
	const uint outputIndex() const { return fOutputIndex; }
	void outputIndex ( const uint outputIndex ) { fOutputIndex = outputIndex; }

protected:
	std::string fErrMsg;   /// error occured in evaluation
	uint32_t fInputIndex;  /// index to the input rowgroup
	uint32_t fOutputIndex; /// index to the output rowgroup
	uint fExpressionId;    /// unique id for this expression
};

std::ostream& operator<<(std::ostream& os, const ReturnedColumn& rhs);

}
#endif //RETURNEDCOLUMN_H

