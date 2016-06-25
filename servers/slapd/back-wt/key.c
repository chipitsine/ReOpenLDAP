/* OpenLDAP WiredTiger backend */
/* $ReOpenLDAP$ */
/* Copyright (c) 2015,2016 Leonid Yuriev <leo@yuriev.ru>.
 * Copyright (c) 2015,2016 Peter-Service R&D LLC <http://billing.ru/>.
 *
 * This file is part of ReOpenLDAP.
 *
 * ReOpenLDAP is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * ReOpenLDAP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Copyright 2002-2014 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* ACKNOWLEDGEMENTS:
 * This work was developed by HAMANO Tsukasa <hamano@osstech.co.jp>
 * based on back-bdb for inclusion in OpenLDAP Software.
 * WiredTiger is a product of MongoDB Inc.
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>
#include "back-wt.h"
#include "config.h"
#include "idl.h"

/* read a key */
int
wt_key_read(
	Backend *be,
	WT_CURSOR *cursor,
	struct berval *k,
	ID *ids,
	WT_CURSOR **saved_cursor,
	int get_flag
	)
{
	int rc;
	WT_ITEM key;
	int exact;
	WT_ITEM key2;
	ID id;

	Debug( LDAP_DEBUG_TRACE, "=> key_read\n", 0, 0, 0 );

	WT_IDL_ZERO(ids);

	bv2ITEM(k, &key);
	cursor->set_key(cursor, &key, 0);
	rc = cursor->search_near(cursor, &exact);
	if( rc ){
		Debug( LDAP_DEBUG_ANY,
			   LDAP_XSTRING(wt_key_read)
			   ": search_near failed: %s (%d)\n",
			   wiredtiger_strerror(rc), rc, 0 );
		goto done;
	}

	do {
		rc = cursor->get_key(cursor, &key2, &id);
		if( rc ){
			Debug( LDAP_DEBUG_ANY,
				   LDAP_XSTRING(wt_key_read)
				   ": get_key failed: %s (%d)\n",
				   wiredtiger_strerror(rc), rc, 0 );
			break;
		}

		if (key.size != key2.size || memcmp(key.data, key2.data, key.size)) {
			if(exact < 0){
				rc = cursor->next(cursor);
				if (rc) {
					break;
				}else{
					continue;
				}
			}
			break;
		}
		exact = 0;
		wt_idl_append_one(ids, id);
		rc = cursor->next(cursor);
	} while(rc == 0);

	if (rc == WT_NOTFOUND ) {
		rc = LDAP_SUCCESS;
	}

done:
	if( rc != LDAP_SUCCESS ) {
		Debug( LDAP_DEBUG_TRACE, "<= wt_key_read: failed (%d)\n",
			   rc, 0, 0 );
	} else {
		Debug( LDAP_DEBUG_TRACE, "<= wt_key_read %ld candidates\n",
			   (long) WT_IDL_N(ids), 0, 0 );
	}

	return rc;
}

/* Add or remove stuff from index files */
int
wt_key_change(
	Backend *be,
	WT_CURSOR *cursor,
	struct berval *k,
	ID id,
	int op
)
{
	int	rc;
	WT_ITEM item;

	Debug( LDAP_DEBUG_TRACE, "=> key_change(%s,%lx)\n",
		   op == SLAP_INDEX_ADD_OP ? "ADD":"DELETE", (long) id, 0 );

	bv2ITEM(k, &item);
	cursor->set_key(cursor, &item, id);
	cursor->set_value(cursor, NULL);

	if (op == SLAP_INDEX_ADD_OP) {
		/* Add values */
		rc = cursor->insert(cursor);
		if ( rc == WT_DUPLICATE_KEY ) rc = 0;
	} else {
		/* Delete values */
		rc = cursor->remove(cursor);
		if ( rc == WT_NOTFOUND ) rc = 0;
	}
	if( rc ) {
		Debug( LDAP_DEBUG_ANY,
			   LDAP_XSTRING(wt_key_change)
			   ": error: %s (%d)\n",
			   wiredtiger_strerror(rc), rc, 0);
		return rc;
	}

	Debug( LDAP_DEBUG_TRACE, "<= key_change %d\n", rc, 0, 0 );

	return rc;
}

/*
 * Local variables:
 * indent-tabs-mode: t
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */