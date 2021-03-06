/*
 * Copyright © 2019 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef BACKEND_REDIS_CLUSTER_INFO_H_
#define BACKEND_REDIS_CLUSTER_INFO_H_

/**
 * cluster info keeps track of the addresses associated with hash slot ranges
 * unless outdated, it should reflect what redis command CLUSTER SLOTS returns
 * including any info about replicas
 */

#include "address.h"
#include "result.h"
#include "server_info.h"

#include <errno.h>

/*
 * maximim allowed cluster size (number of master servers)
 */
#define DBBE_REDIS_CLUSTER_MAX_SIZE ( 512 )

typedef struct
{
  int _cluster_size;
  dbBE_Redis_server_info_t *_nodes[ DBBE_REDIS_CLUSTER_MAX_SIZE ];
} dbBE_Redis_cluster_info_t;

/*
 * return the size of the cluster (number of master processes without replicas)
 */
static inline
int dbBE_Redis_cluster_info_getsize( dbBE_Redis_cluster_info_t *ci )
{
  return ( ci != NULL ) ? ci->_cluster_size : 0;
}

/*
 * return a ptr to the server info indicated by index
 */
static inline
dbBE_Redis_server_info_t* dbBE_Redis_cluster_info_get_server( dbBE_Redis_cluster_info_t *ci, const int index )
{
  return ( ci != NULL ) && ( index >= 0 ) && ( index < DBBE_REDIS_CLUSTER_MAX_SIZE ) ? ci->_nodes[ index ] : NULL;
}

static inline
dbBE_Redis_server_info_t* dbBE_Redis_cluster_info_get_server_by_addr( dbBE_Redis_cluster_info_t *ci,
                                                                      const char *url )
{
  if(( url == NULL ) || ( ci == NULL ))
    return NULL;

  int n;
  for( n=0; n < ci->_cluster_size; ++n )
  {
    char *master = dbBE_Redis_server_info_get_master( ci->_nodes[ n ] );
    if( master == NULL )
      return NULL;
    if( strncmp( master, url, DBR_SERVER_URL_MAX_LENGTH ) == 0 )
      return ci->_nodes[ n ];
  }
  // if the requested URL was not a master:
  for( n=0; n < ci->_cluster_size; ++n )
  {
    dbBE_Redis_server_info_t *si = ci->_nodes[ n ];
    int s;
    for( s=0; s < dbBE_Redis_server_info_getsize( si ); ++s )
    {
      char *srv = dbBE_Redis_server_info_get_replica( si, s );
      if( srv == NULL )
        continue;
      if( strncmp( srv, url, DBR_SERVER_URL_MAX_LENGTH ) == 0 )
        return ci->_nodes[ n ];
    }
  }
  return NULL;
}

/*
 * create cluster info of a single-node Redis based on the url
 */
dbBE_Redis_cluster_info_t* dbBE_Redis_cluster_info_create_single( char *url );

/*
 * create cluster info from a parsed result of a CLUSTER SLOTS call
 */
dbBE_Redis_cluster_info_t* dbBE_Redis_cluster_info_create( dbBE_Redis_result_t *cir );

/*
 * remove a server info entry from a cluster based on the ptr/address
 */
int dbBE_Redis_cluster_info_remove_entry_ptr( dbBE_Redis_cluster_info_t *ci,
                                              dbBE_Redis_server_info_t *srv );

/*
 * remove a server info entry from a cluster based on the index in the server array
 */
int dbBE_Redis_cluster_info_remove_entry_idx( dbBE_Redis_cluster_info_t *ci,
                                              const int idx );

/*
 * cleanup and destroy the cluster info structure
 */
int dbBE_Redis_cluster_info_destroy( dbBE_Redis_cluster_info_t *ci );

#endif /* BACKEND_REDIS_CLUSTER_INFO_H_ */
