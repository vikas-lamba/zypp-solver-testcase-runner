/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * zypptestomatic.cc
 *
 * Copyright (C) 2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <sstream>
#include <iostream>

#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include "helix/XmlNode.h"

#include "zypp/Resolvable.h"
#include "zypp/ResTraits.h"
#include "zypp/ResPool.h"
#include "zypp/PoolItem.h"
#include "zypp/Capability.h"
#include "zypp/CapSet.h"
#include "zypp/CapFactory.h"
#include "zypp/solver/libzypp_solver.h"

#include "zypp/Source.h"
#include "zypp/SourceFactory.h"
#include "zypp/SourceManager.h"

#include "zypp/base/String.h"
#include "zypp/base/Logger.h"
#include "zypp/base/Exception.h"

#include "zypp/base/Algorithm.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"

#include "zypp/media/MediaAccess.h"
#include "zypp/source/SourceImpl.h"

#include "helix/HelixSourceImpl.h"

using namespace std;
using namespace zypp;
using namespace zypp::solver::detail;

int assertOutput( const char* output)
{
    cout << "Assertion in " << __FILE__ << ", " << __LINE__ << " : " << output << " --> exit" << endl;
    exit (0);
}

# define assertExit(expr) \
  (__ASSERT_VOID_CAST ((expr) ? 0 :		 \
		       (assertOutput (__STRING(expr)))))

static string globalPath;
static ResPool *globalPool;
static SourceManager_Ptr manager;

typedef list<unsigned int> ChecksumList;
typedef std::set <zypp::PoolItem> PoolItemSet;

#define RESULT cout << ">!> "

//---------------------------------------------------------------------------

Resolvable::Kind
string2kind (const std::string & str)
{
    Resolvable::Kind kind = ResTraits<zypp::Package>::kind;
    if (!str.empty()) {
	if (str == "package") {
	    // empty 
	}
	else if (str == "patch") {
	    kind = ResTraits<zypp::Patch>::kind;
	}
	else if (str == "pattern") {
	    kind = ResTraits<zypp::Pattern>::kind;
	}
	else if (str == "script") {
	    kind = ResTraits<zypp::Script>::kind;
	}
	else if (str == "message") {
	    kind = ResTraits<zypp::Message>::kind;
	}
	else if (str == "product") {
	    kind = ResTraits<zypp::Product>::kind;
	}
	else {
	    cerr << "get_poolItem unknown kind '" << str << "'" << endl;
	}
    }
    return kind;
}

//---------------------------------------------------------------------------

#warning Locks not implemented
#if 0
static void
lock_poolItem (PoolItem_Ref poolItem)
{
    RCResItemDep *dep;
    RCResItemMatch *match;

    dep = rc_poolItem_dep_new_from_spec (RC_RESOLVABLE_SPEC (poolItem),
					RC_RELATION_EQUAL, RC_TYPE_RESOLVABLE,
					RC_CHANNEL_ANY, false, false);

    match = rc_poolItem_match_new ();
    rc_poolItem_match_set_dep (match, dep);

    rc_poolItem_dep_unref (dep);

    rc_world_add_lock (rc_get_world (), match);
}

#endif	// 0

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

//==============================================================================================================================

//---------------------------------------------------------------------------------------------------------------------
// helper functions

typedef list<string> StringList;

static void
assemble_install_cb (PoolItem_Ref poolItem,
		     void *data)
{
    StringList *slist = (StringList *)data;
    ostringstream s;
    s << str::form ("%-7s ", poolItem.status().isInstalled() ? "|flag" : "install");
    s << poolItem.resolvable();

    slist->push_back (s.str());
}


static void
assemble_uninstall_cb (PoolItem_Ref poolItem,
		       void *data)
{
    StringList *slist = (StringList *)data;
    ostringstream s;
    s << str::form ("%-7s ", poolItem.status().isInstalled() ? "remove" : "|unflag");
    s << poolItem.resolvable();

    slist->push_back (s.str());
}


static void
assemble_upgrade_cb (PoolItem_Ref res1,
		     PoolItem_Ref res2,
		     void *data)
{
    StringList *slist = (StringList *)data;
    ostringstream s;

    s << "upgrade ";

    s << res2.resolvable();
    s << " => ";
    s << res1.resolvable();

    slist->push_back (s.str());
}


static void
assemble_incomplete_cb (PoolItem_Ref poolItem,
		       void *data)
{
    StringList *slist = (StringList *)data;
    ostringstream s;
    s << str::form ("%-11s ", poolItem.status().isInstalled() ? "incomplete" : "|incomplete");
    s << poolItem.resolvable();

    slist->push_back (s.str());
}


static void
assemble_satisfy_cb (PoolItem_Ref poolItem,
		       void *data)
{
    StringList *slist = (StringList *)data;
    ostringstream s;
    s << str::form ("%-10s ", poolItem.status().isInstalled() ? "SATISFIED" : "|satisfied");
    s << poolItem.resolvable();

    slist->push_back (s.str());
}

static void
print_sep (void)
{
    cout << endl << "------------------------------------------------" << endl << endl;
}


static void
print_important (const string & str)
{
    RESULT << str.c_str() << endl;
}


static void
print_solution (ResolverContext_Ptr context, int *count, ChecksumList & checksum_list, bool instorder)
{
    if (context->isValid ()) {

	StringList items;
	items.clear();

	unsigned int checksum = 0;
	bool is_dup = false;

	RESULT << "Solution #" << *count << ":" << endl;
	++*count;

	context->foreachInstall (assemble_install_cb, &items);

	context->foreachUninstall (assemble_uninstall_cb, &items);

	context->foreachUpgrade (assemble_upgrade_cb, &items);

	context->foreachIncomplete (assemble_incomplete_cb, &items);

	context->foreachSatisfy (assemble_satisfy_cb, &items);

	items.sort ();

	for (StringList::const_iterator iter = items.begin(); iter != items.end(); iter++) {
	    const char *c = (*iter).c_str();
	    while (*c) {
		checksum = 17 * checksum + (unsigned int)*c;
		++c;
	    }
	}
	cout << str::form ("Checksum = %x", checksum) << endl;

	for (ChecksumList::const_iterator iter = checksum_list.begin(); iter != checksum_list.end() && !is_dup; iter++) {
	    if (*iter == checksum) {
		is_dup = true;
	    }
	}

	if (! is_dup) {
	    for (StringList::const_iterator iter = items.begin(); iter != items.end(); iter++) {
		print_important (*iter);
	    }
	    checksum_list.push_back (checksum);
	} else {
	    RESULT << "This solution is a duplicate." << endl;
	}

	items.clear();

    } else {
	RESULT << "Failed Attempt:" << endl;
    }

    RESULT << "installs=" << context->installCount() << ", upgrades=" << context->upgradeCount() << ", uninstalls=" << context->uninstallCount();
    int satisfied = context->satisfyCount();
    if (satisfied > 0) cout << ", satisfied=" << satisfied;
    cout << endl;
    cout << str::form ("download size=%.1fk, install size=%.1fk\n", context->downloadSize() / 1024.0, context->installSize() / 1024.0);
    cout << str::form ("total priority=%d, min priority=%d, max priority=%d\n", context->totalPriority(), context->minPriority(), context->maxPriority());
    cout << str::form ("other penalties=%d\n",  context->otherPenalties());
    cout << "- - - - - - - - - -" << endl;

    if (instorder) {
	cout << endl;
	RESULT << "Installation Order:" << endl << endl;
	PoolItemList installs = context->getMarked(1);
	PoolItemList dummy;

	InstallOrder order( globalPool, installs, dummy );		 // sort according top prereq
	order.init();
	const PoolItemList & installorder ( order.getTopSorted() );
	for (PoolItemList::const_iterator iter = installorder.begin(); iter != installorder.end(); iter++) {
		RESULT << (*iter) << endl;
	}
	cout << "- - - - - - - - - -" << endl;
    }

    fflush (stdout);

    context->spewInfo ();
    if (getenv ("RC_SPEW")) cout << context << endl;

}

//---------------------------------------------------------------------------------------------------------------------
struct FindPackage : public resfilter::ResObjectFilterFunctor
{
    PoolItem_Ref poolItem;

    FindPackage ()
    { }

    bool operator()( PoolItem_Ref p)
    {
	poolItem = p;
	return false;				// stop here, we found it
    }
};



static PoolItem_Ref
get_poolItem (const string & source_name, const string & package_name, const string & kind_name = "")
{
    PoolItem_Ref poolItem;
    Resolvable::Kind kind = string2kind (kind_name);

    try {
	const Source & source = manager->findSource (source_name);
	FindPackage info;

	invokeOnEach( globalPool->byNameBegin( package_name ),
		      globalPool->byNameEnd( package_name ),
		      functor::chain( resfilter::BySource(source), resfilter::ByKind (kind) ),
		      functor::functorRef<bool,PoolItem> (info) );

	poolItem = info.poolItem;
    }
    catch (Exception & excpt_r) {
	ZYPP_CAUGHT (excpt_r);
	cerr << "Can't find resolvable '" << package_name << "': source '" << source_name << "' not defined" << endl;
	return poolItem;
    }

    if (!poolItem) {
	cerr << "Can't find resolvable '" << package_name << "' in source '" << source_name << "': no such name/kind" << endl;
    }

    return poolItem;
}


//---------------------------------------------------------------------------------------------------------------------
// whatdependson


struct RequiringPoolItem : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItemSet itemset;
    PoolItem_Ref provider;
    Capability cap;
    bool first;

    RequiringPoolItem (PoolItem_Ref p)
	: provider (p)
    { }

    bool operator()( PoolItem_Ref requirer, const Capability & match )
    {
	if (itemset.insert (requirer).second) {
	    if (first) {
		cout << "\t" << provider.resolvable() << " provides " << cap << " required by" << endl;
		first = false;
	    }
	    cout << "\t\t" << requirer.resolvable() << " for " << cap << endl;
	}
	return true;
    }
};


static PoolItemSet
whatdependson (PoolItem_Ref poolItem)
{
    cout << endl << endl << "What depends on '" << poolItem.resolvable() << "'" << endl;

    RequiringPoolItem info (poolItem);

    // loop over all provides and call foreachRequiringResItem

    CapSet caps = poolItem->dep (Dep::PROVIDES);
    for (CapSet::const_iterator cap_iter = caps.begin(); cap_iter != caps.end(); ++cap_iter) {

	info.cap = *cap_iter;
	info.first = true;

	//world->foreachRequiringResItem (info.cap, requires_poolItem_cb, &info);

	Dep dep( Dep::REQUIRES );
	invokeOnEach( globalPool->byCapabilityIndexBegin( info.cap.index(), dep ),
		      globalPool->byCapabilityIndexEnd( info.cap.index(), dep ),
		      resfilter::callOnCapMatchIn( dep, info.cap, functor::functorRef<bool,PoolItem,Capability>(info) ) );

    }

    return info.itemset;
}


//---------------------------------------------------------------------------------------------------------------------
// whatprovides


struct ProvidingPoolItem : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItemSet itemset;

    bool operator()( PoolItem_Ref provider, const Capability & match )
    {
	itemset.insert (provider);
	return true;
    }
};


static PoolItemSet
get_providing_poolItems (const string & prov_name, const string & kind_name = "")
{
    PoolItemSet rs;
    Resolvable::Kind kind = string2kind (kind_name);

    CapFactory factory;
    Capability cap = factory.parse (kind, prov_name);

    Dep dep( Dep::PROVIDES );
    ProvidingPoolItem info;

    // world->foreachProvidingResItem (cap, providing_poolItem_cb, &rs);

    invokeOnEach( globalPool->byCapabilityIndexBegin( cap.index(), dep ),
		  globalPool->byCapabilityIndexEnd( cap.index(), dep ),
		  resfilter::callOnCapMatchIn( dep, cap, functor::functorRef<bool,PoolItem,Capability>(info) ) );

    return info.itemset;
}



//---------------------------------------------------------------------------------------------------------------------
// setup related functions


static void
load_source (const string & alias, const string & filename, const string & type, bool system_packages)
{
    Pathname pathname = globalPath + filename;
    int count = 0;

    try {
	media::MediaAccess::Ptr media = new media::MediaAccess();
	Source::Impl_Ptr impl = new HelixSourceImpl (media, pathname, alias);
	SourceFactory _f;
	Source s = _f.createFrom( impl );

	if (s) {

	    count = impl->resolvables (s).size();
	    manager->addSource (s);
	}
	cout << "Loaded " << count << " package(s) from " << pathname << endl;
    }
    catch ( Exception & excpt_r ) {
	ZYPP_CAUGHT (excpt_r);
	cout << "Loaded NO package(s) from " << pathname << endl;
    }
}

static void
undump (const std::string & filename)
{
    cerr << "undump not really supported" << endl;

    return load_source ("undump", filename, "undump", false);
}


static bool done_setup = false;

static void
parse_xml_setup (XmlNode_Ptr node)
{
    assertExit (node->equals("setup"));

    if (done_setup) {
	cerr << "Multiple <setup>..</setup> sections not allowed!" << endl;
	exit (0);
    }
    done_setup = true;

    node = node->children();
    while (node != NULL) {
	if (!node->isElement()) {
	    node = node->next();
	    continue;
	}

	if (node->equals ("system")) {

	    string file = node->getProp ("file");
	    assertExit (!file.empty());
	    load_source ("@system", file, "helix", true);

	} else if (node->equals ("source")) {

	    string name = node->getProp ("name");
	    string file = node->getProp ("file");
	    string type = node->getProp ("type");
	    assertExit (!name.empty());
	    assertExit (!file.empty());
	    load_source (name, file, type, false);

	} else if (node->equals ("undump")) {

	    string file = node->getProp ("file");
	    assertExit (!file.empty());
	    undump (file);

	} else if (node->equals ("force-install")) {

	    string source_name = node->getProp ("source");
	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem_Ref poolItem;

	    assertExit (!source_name.empty());
	    assertExit (!package_name.empty());

	    poolItem = get_poolItem (source_name, package_name, kind_name);
	    if (poolItem) {
		RESULT << "Force-installing " << package_name << " from source " << source_name << endl;;

		const Source & system_source = manager->findSource("@system");

		if (!system_source)
		    cerr << "No system source available!" << endl;
#warning Needs force-install
#if 0
		PoolItem_Ref r = boost::const_pointer_cast<PoolItem>(poolItem);
		r->setChannel (system_source);
		r->setInstalled (true);
#endif
	    } else {
		cerr << "Unknown package " << source_name << "::" << package_name << endl;
	    }

	} else if (node->equals ("force-uninstall")) {

	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem_Ref poolItem;

	    assertExit (!package_name.empty());
	    poolItem = get_poolItem ("@system", package_name, kind_name);
	    
	    if (! poolItem) {
		cerr << "Can't force-uninstall installed package '" << package_name << "'" << endl;
	    } else {
		RESULT << "Force-uninstalling " << package_name << endl;
#warning Needs pool remove
#if 0
		globalPool->remove (poolItem);
#endif
	    }

	} else if (node->equals ("lock")) {

	    string source_name = node->getProp ("source");
	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem_Ref poolItem;

	    assertExit (!source_name.empty());
	    assertExit (!package_name.empty());

	    poolItem = get_poolItem (source_name, package_name, kind_name);
	    if (poolItem) {
		RESULT << "Locking " << package_name << " from source " << source_name << endl;
#warning Needs locks
#if 0
		r->setLocked (true);
#endif
	    } else {
		cerr << "Unknown package " << source_name << "::" << package_name << endl;
	    }

	} else {
	    cerr << "Unrecognized tag '" << node->name() << "' in setup" << endl;
	}

	node = node->next();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// trial related functions

static void
report_solutions (Resolver & resolver, bool instorder)
{
    int count = 1;
    ChecksumList checksum_list;

    cout << endl;

    if (!resolver.completeQueues().empty()) {
	cout << "Completed solutions: " << (long) resolver.completeQueues().size() << endl;
    }

    if (resolver.prunedQueues().empty()) {
	cout << "Pruned solutions: " << (long) resolver.prunedQueues().size() << endl;
    }

    if (resolver.deferredQueues().empty()) {
	cout << "Deferred solutions: " << (long) resolver.deferredQueues().size() << endl;
    }

    if (resolver.invalidQueues().empty()) {
	cout << "Invalid solutions: " << (long) resolver.invalidQueues().size() << endl;
    }
    
    if (resolver.bestContext()) {
	cout << endl << "Best Solution:" << endl;
	print_solution (resolver.bestContext(), &count, checksum_list, instorder);

	ResolverQueueList complete = resolver.completeQueues();
	if (complete.size() > 1)
	    cout << endl << "Other Valid Solutions:" << endl;

	if (complete.size() < 20) {
	    for (ResolverQueueList::const_iterator iter = complete.begin(); iter != complete.end(); iter++) {
		ResolverQueue_Ptr queue = (*iter);
		if (queue->context() != resolver.bestContext()) 
		    print_solution (queue->context(), &count, checksum_list, instorder);
	    }
	}
    }

    ResolverQueueList invalid = resolver.invalidQueues();
    if (invalid.size() < 20) {
	cout << endl;

	for (ResolverQueueList::const_iterator iter = invalid.begin(); iter != invalid.end(); iter++) {
	    ResolverQueue_Ptr queue = (*iter);
	    cout << "Failed Solution: " << endl << queue->context() << endl;
	    cout << "- - - - - - - - - -" << endl;
	    queue->context()->spewInfo ();
	    fflush (stdout);
	}
    } else {
	cout << "(Not displaying more than 20 invalid solutions)" << endl;
    }
    fflush (stdout);
}

//-----------------------------------------------------------------------------
// system Upgrade

struct Unique : public resfilter::PoolItemFilterFunctor
{
    PoolItemSet itemset;

    bool operator()( PoolItem_Ref poolItem )
    {
	itemset.insert (poolItem);
	return true;
    }
};


// collect all installed items in a set

PoolItemSet
uniquelyInstalled (void)
{
    Unique info;

    invokeOnEach( globalPool->begin( ),
		  globalPool->end ( ),
		  resfilter::ByInstalled (),
		  functor::functorRef<bool,PoolItem> (info) );
    return info.itemset;
}

struct DoUpgrades : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItem_Ref installed;
    PoolItemSet upgrades;
    Resolver & resolver;
    int count;

    DoUpgrades (Resolver & r)
	: resolver (r)
	, count (0)
    {  }

    bool operator()( PoolItem_Ref poolItem )
    {
	if (upgrades.insert (poolItem).second) {			// only consider first match
	    resolver.addPoolItemToInstall (poolItem);
	    RESULT << "Upgrading " << installed << " => " << poolItem << endl;
	    ++count;
	}
	return true;
    }
};


int
foreach_system_upgrade (Resolver & resolver)
{
    PoolItemSet installed = uniquelyInstalled();

    DoUpgrades info (resolver);

    // world->foreachSystemUpgrade (true, trial_upgrade_cb, (void *)&resolver);

    for (PoolItemSet::iterator iter = installed.begin(); iter != installed.end(); ++iter) {
	PoolItem_Ref p = *iter;
	info.installed = p;
	invokeOnEach( globalPool->byNameBegin( p->name() ), globalPool->byNameEnd( p->name() ),
		      functor::chain( resfilter::ByKind( p->kind() ),
		      resfilter::byEdition<CompareByGT<Edition> >( p->edition() )),
		      functor::functorRef<bool,PoolItem>(info) );
    }

    return info.count;
}

//-----------------------------------------------------------------------------
// ResolverContext output

static void
print_marked_cb (PoolItem_Ref poolItem, void *data)
{
    RESULT << poolItem << " " << poolItem.status() << endl;
    return;
}


static void
freshen_marked_cb (PoolItem_Ref poolItem, void *data)
{
    Resolver *resolver = (Resolver *)data;
    if (poolItem.status().isIncomplete()) {
	resolver->addPoolItemToInstall (poolItem);
    }

    return;
}


static void
parse_xml_trial (XmlNode_Ptr node)
{
    bool verify = false;
    bool instorder = false;

    assertExit (node->equals ("trial"));

    DBG << "parse_xml_trial()" << endl;

    if (! done_setup) {
	cerr << "Any trials must be preceeded by the setup!" << endl;
	exit (0);
    }

    print_sep ();

    Resolver resolver (globalPool);

    ResolverContext_Ptr established = NULL;

    node = node->children();
    while (node) {
	if (!node->isElement()) {
	    node = node->next();
	    continue;
	}

	if (node->equals("note")) {

	    string note = node->getContent ();
	    cout << "NOTE: " << note << endl;

	} else if (node->equals ("verify")) {

	    verify = true;

	} else if (node->equals ("current")) {

	    string source_name = node->getProp ("source");
	    const Source & source = manager->findSource (source_name);

	    if (source) {
//FIXME		resolver.setCurrentChannel (source);
	    } else {
		cerr << "Unknown source '" << source_name << "' (current)" << endl;
	    }

	} else if (node->equals ("subscribe")) {

	    string source_name = node->getProp ("source");
	    Source & source = manager->findSource (source_name);

	    if (source) {
//FIXME		source->setSubscription (true);
	    } else {
		cerr << "Unknown source '" << source_name << "' (subscribe)" << endl;
	    }

	} else if (node->equals ("install")) {

	    string source_name = node->getProp ("source");
	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem_Ref poolItem;

	    assertExit (!source_name.empty());
	    assertExit (!package_name.empty());

	    poolItem = get_poolItem (source_name, package_name, kind_name);
	    if (poolItem) {
		RESULT << "Installing " << package_name << " from source " << source_name << endl;;
		resolver.addPoolItemToInstall (poolItem);
	    } else {
		cerr << "Unknown package " << source_name << "::" << package_name << endl;
	    }

	} else if (node->equals ("uninstall")) {

	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem_Ref poolItem;

	    assertExit (!package_name.empty());

	    poolItem = get_poolItem ("@system", package_name, kind_name);
	    if (poolItem) {
		RESULT << "Uninstalling " << package_name << endl;
		resolver.addPoolItemToRemove (poolItem);
	    } else {
		cerr << "Unknown system package " << package_name << endl;
	    }

	} else if (node->equals ("upgrade")) {

	    RESULT << "Checking for upgrades..." << endl;

	    int count = foreach_system_upgrade (resolver);
	    
	    if (count == 0)
		RESULT << "System is up-to-date, no upgrades required" << endl;
	    else
		RESULT << "Upgrading " << count << " package" << (count > 1 ? "s" : "") << endl;

	} else if (node->equals ("establish")
		   || node->equals ("freshen")) {

	    RESULT << "Establishing state ..." << endl;

	    resolver.establishState (established);
//cerr << "established<" << established << "> -> <" << resolver.bestContext() << ">" << endl;
	    established = resolver.bestContext();
	    if (established == NULL)
		RESULT << "Established NO context !" << endl;
	    else {
		RESULT << "Established context" << endl;
		established->foreachMarked (print_marked_cb, NULL);
		if (node->equals ("freshen")) {
		    RESULT << "Freshening ..." << endl;
		    established->foreachMarked (freshen_marked_cb, &resolver);
		}
	    }

	} else if (node->equals ("instorder")) {

	    RESULT << "Calculating installation order ..." << endl;

	    instorder = true;

	} else if (node->equals ("solvedeps")) {
#if 0
	    XmlNode_Ptr iter = node->children();

	    while (iter != NULL) {
		Dependency_Ptr dep = new Dependency (iter);

		/* We just skip over anything that doesn't look like a dependency. */

		if (dep) {
		    string conflict_str = iter->getProp ("conflict");

		    RESULT << "Solvedeps " << (conflict_str.empty() ? "" : "conflict ") << dep->asString().c_str() << endl;

		    resolver.addExtraDependency (dep);

		}
		iter = iter->next();
	    }
#else
#warning solvedeps disabled
#endif

	} else if (node->equals ("whatprovides")) {

	    string kind_name = node->getProp ("kind");
	    string prov_name = node->getProp ("provides");

	    PoolItemSet poolItems;

	    cout << "poolItems providing '" << prov_name << "'" << endl;

	    poolItems = get_providing_poolItems (prov_name, kind_name);

	    if (poolItems.empty()) {
		cerr << "None found" << endl;
	    } else {
		for (PoolItemSet::const_iterator iter = poolItems.begin(); iter != poolItems.end(); ++iter) {
		    cout << (*iter) << endl;
		}
	    }

	} else if (node->equals ("whatdependson")) {

	    string source_name = node->getProp ("source");
	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");
	    string prov_name = node->getProp ("provides");

	    PoolItemSet poolItems;

	    assert (!source_name.empty());
	    assert (!package_name.empty());

	    if (!prov_name.empty()) {
		if (!package_name.empty()) {
		    cerr << "<whatdependson ...> can't have both package and provides." << endl;
		    exit (1);
		}
		poolItems = get_providing_poolItems (prov_name, kind_name);
	    }
	    else {
		PoolItem_Ref poolItem = get_poolItem (source_name, package_name, kind_name);
		if (poolItem) poolItems.insert (poolItem);
	    }
	    if (poolItems.empty()) {
		cerr << "Can't find matching package" << endl;
	    } else {
		for (PoolItemSet::const_iterator iter = poolItems.begin(); iter != poolItems.end(); ++iter) {
		    PoolItemSet dependants = whatdependson (*iter);
		    for (PoolItemSet::const_iterator dep_iter = dependants.begin(); dep_iter != dependants.end(); ++dep_iter) {
			cout << (*dep_iter) << endl;
		    }
		}
	    }

	} else if (node->equals ("reportproblems")) {
	    if (resolver.resolveDependencies (established) == true) {
		RESULT << "No problems so far" << endl;
	    }
	    else {
		ResolverProblemList problems = resolver.problems ();
		RESULT << problems.size() << " problems found:" << endl;
		for (ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter) {
		    cout << *iter << endl;
		}
	    }

	} else {
	    cerr << "Unknown tag '" << node->name() << "' in trial" << endl;
	}

	node = node->next();
    }

    if (getenv ("RC_DEPS_TIME")) {
	int timeout = atoi (getenv ("RC_DEPS_TIME"));

	resolver.setTimeout (timeout);
    }

    if (verify)
	resolver.verifySystem ();
    else
	resolver.resolveDependencies (established);

    report_solutions (resolver, instorder);
}

//---------------------------------------------------------------------------------------------------------------------

static void
parse_xml_test (XmlNode_Ptr node)
{
    assertExit (node->equals("test"));

    node = node->children();

    while (node) {
	if (node->type() == XML_ELEMENT_NODE) {
	    if (node->equals("setup")) {
		parse_xml_setup (node);
	    } else if (node->equals ("trial")) {
		parse_xml_trial (node);
	    } else {
		cerr << "Unknown tag '" << node->name() << "' in test" << endl;
	    }
	}

	node = node->next();
    }
}


static void
process_xml_test_file (const string & filename)
{
    xmlDocPtr xml_doc;
    XmlNode_Ptr root;

    xml_doc = xmlParseFile (filename.c_str());
    if (xml_doc == NULL) {
	cerr << "Can't parse test file '" << filename << "'" << endl;
	exit (0);
    }

    root = new XmlNode (xmlDocGetRootElement (xml_doc));

    DBG << "Parsing file '" << filename << "'" << endl;
    
    parse_xml_test (root);
    
    xmlFreeDoc (xml_doc);
}


//---------------------------------------------------------------------------------------------------------------------

int
main (int argc, char *argv[])
{
    setenv("ZYPP_NOLOG","1",1); // no logging
    
    if (argc != 2) {
	cerr << "Usage: deptestomatic testfile.xml" << endl;
	exit (0);
    }

    manager = SourceManager::sourceManager();

    globalPath = argv[1];
    globalPath = globalPath.substr (0, globalPath.find_last_of ("/") +1);

    DBG << "init_libzypp() done" << endl;

    process_xml_test_file (string (argv[1]));

    return 0;
}
