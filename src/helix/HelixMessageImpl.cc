/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixMessageImpl.cc
 *
*/

#include "HelixMessageImpl.h"
#include "zypp/repo/RepositoryImpl.h"
#include "zypp/base/String.h"
#include "zypp/base/Logger.h"

using namespace std;
using namespace zypp::detail;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : HelixMessageImpl
//
///////////////////////////////////////////////////////////////////

/** Default ctor
*/
HelixMessageImpl::HelixMessageImpl (Repository source_r, const zypp::HelixParser & parsed)
    : _source (source_r)
    , _vendor(parsed.vendor)    
{
}

Repository
HelixMessageImpl::repository() const
{ return _source; }

TranslatedText
HelixMessageImpl::text () const
{ return _text; }

std::string
HelixMessageImpl::type () const
{ return _type; }

ByteCount
HelixMessageImpl::size() const
{ return _size_installed; }

Vendor HelixMessageImpl::vendor() const
{ 
   if ( _vendor == "")
      return "SUSE LINUX Products GmbH, Nuernberg, Germany";
   return _vendor;
}


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
