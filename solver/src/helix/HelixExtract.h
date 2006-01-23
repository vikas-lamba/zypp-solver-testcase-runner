/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/*
 * HelixExtract.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 * Copyright (c) 2005 SUSE Linux Products GmbH
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

#ifndef HELIXEXTRACT_H
#define HELIXEXTRACT_H

#include "zypp/Resolvable.h"
#include "zypp/ResStore.h"
#include "zypp/solver/temporary/XmlNodePtr.h"
#include "zypp/solver/temporary/ChannelPtr.h"

#include "HelixParser.h"

namespace zypp {

class HelixSourceImpl;

int extractHelixBuffer (const char *data, size_t len, solver::detail::Channel_Ptr channel, HelixSourceImpl *impl);
int extractHelixFile (const std::string & filename, solver::detail::Channel_Ptr channel, HelixSourceImpl *impl);

}

#endif /* HELIXEXTRACT_H */

