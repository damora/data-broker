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

#include "cluster_info.h"

dbBE_Redis_cluster_info_t* dbBE_Redis_cluster_info_create_single( char *url )
{
  // don't attempt to create cluster info for NULL-ptr url
  if( url == NULL )
    return NULL;

  dbBE_Redis_cluster_info_t *ci = (dbBE_Redis_cluster_info_t*)calloc( 1, sizeof( dbBE_Redis_cluster_info_t ) );
  if( ci == NULL )
  {
    LOG( DBG_ERR, stderr, "Unable to allocate sufficient memory for cluster info\n" );
    return NULL;
  }

  ci->_cluster_size = 1;
  ci->_nodes[0] = dbBE_Redis_server_info_create_single( url );

  if( ci->_nodes[0] == NULL )
  {
    dbBE_Redis_cluster_info_destroy( ci );
    ci = NULL;
  }
  return ci;
}



dbBE_Redis_cluster_info_t* dbBE_Redis_cluster_info_create( dbBE_Redis_result_t *cir )
{
  if(( cir == NULL ) || ( cir->_type != dbBE_REDIS_TYPE_ARRAY ))
    return NULL;

  if(( cir->_data._array._len <= 0 ) || ( cir->_data._array._len > DBBE_REDIS_CLUSTER_MAX_SIZE ))
  {
    LOG( DBG_ERR, stderr, "Redis cluster size (%d) invalid or exceeds pre-defined limit of %d\n", cir->_data._array._len, DBBE_REDIS_CLUSTER_MAX_SIZE );
    return NULL;
  }


  dbBE_Redis_cluster_info_t *ci = (dbBE_Redis_cluster_info_t*)calloc( 1, sizeof( dbBE_Redis_cluster_info_t ) );
  if( ci == NULL )
  {
    LOG( DBG_ERR, stderr, "Unable to allocate sufficient memory for cluster info\n" );
    return NULL;
  }

  ci->_cluster_size = cir->_data._array._len;

  // extract cluster slots info from result and fill cluster info structure
  int n;
  for( n=0; n < ci->_cluster_size; ++n )
  {
    dbBE_Redis_result_t *srv = &cir->_data._array._data[ n ];
    if( srv->_type != dbBE_REDIS_TYPE_ARRAY )
    {
      LOG( DBG_ERR, stderr, "Expected ARRAY type in CLUSTER SLOTS response at index %d. Found %d\n", n, srv->_type );
      goto error;
    }

    ci->_nodes[ n ] = dbBE_Redis_server_info_create( srv );
    if( ci->_nodes[ n ] == NULL )
    {
      LOG( DBG_ERR, stderr, "Error while creating server info of entry: %d\n", n );
      goto error;
    }
  }

  return ci;

error:
  dbBE_Redis_cluster_info_destroy( ci );
  return NULL;
}



int dbBE_Redis_cluster_info_remove_entry_ptr( dbBE_Redis_cluster_info_t *ci,
                                              dbBE_Redis_server_info_t *srv )
{
  if(( ci == NULL ) || ( srv == NULL ))
    return -EINVAL;

  int n;
  int rc = 0;
  int shift = 0;
  for( n = 0; n < ci->_cluster_size; ++n )
  {
    if( srv == ci->_nodes[ n ] )
    {
      rc = dbBE_Redis_server_info_destroy( ci->_nodes[ n ] );
      ci->_nodes[ n ] = NULL;
      ++shift;
    }
    if( n < ci->_cluster_size - 1 )
      ci->_nodes[ n ] = ci->_nodes[ shift ];
  }
  if(( shift == 0 )||( ci->_cluster_size == 0 ))
    return -ENOENT;

  ci->_nodes[ ci->_cluster_size - shift ] = NULL;

  return rc;
}



int dbBE_Redis_cluster_info_remove_entry_idx( dbBE_Redis_cluster_info_t *ci, const int idx )
{
  if(( ci == NULL ) || ( idx >= ci->_cluster_size ) || ( idx < 0 ))
    return -EINVAL;

  int n;
  // remove the entry
  int rc = dbBE_Redis_server_info_destroy( ci->_nodes[ idx ] );

  // move all following entries to close any gaps
  for( n = idx; ( rc == 0 ) && ( n < ci->_cluster_size - 1 ); ++n )
    ci->_nodes[ n ] = ci->_nodes[ n+1 ];
  ci->_nodes[ ci->_cluster_size - 1 ] = NULL;
  --ci->_cluster_size;

  return rc;
}




int dbBE_Redis_cluster_info_destroy( dbBE_Redis_cluster_info_t *ci )
{
  if( ci == NULL )
    return -EINVAL;

  int n;
  for( n=0; n<ci->_cluster_size; ++n )
  {
    dbBE_Redis_server_info_destroy( ci->_nodes[ n ] );
    ci->_nodes[ n ] = NULL;
  }

  memset( ci, 0, sizeof( dbBE_Redis_cluster_info_t ) );
  free( ci );
  return 0;
}
