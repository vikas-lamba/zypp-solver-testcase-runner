/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixMessageImpl.h
 *
*/
#ifndef ZYPP_SOLVER_TEMPORARY_HELIXMESSAGEIMPL_H
#define ZYPP_SOLVER_TEMPORARY_HELIXMESSAGEIMPL_H

#include "zypp/detail/MessageImpl.h"
#include "HelixParser.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : HelixMessageImpl
//
/** Class representing a package
*/
class HelixMessageImpl : public detail::MessageImplIf
{
public:

	class HelixParser;
	/** Default ctor
	*/
	HelixMessageImpl( Repository source_r, const zypp::HelixParser & data );

	TranslatedText text () const;
	std::string type () const;
	virtual ByteCount size() const;
	/** */
 	virtual Repository repository() const;
        /** */
        virtual Vendor vendor() const;

protected:
	Repository _source;
	TranslatedText _text;
	std::string _type;
	ByteCount _size_installed;
        Vendor _vendor;    


 };
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOLVER_TEMPORARY_HELIXMESSAGEIMPL_H
