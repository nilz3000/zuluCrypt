/*
 * 
 *  Copyright (c) 2012
 *  name : mhogo mchungu 
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../lib/includes.h"
#include <blkid/blkid.h>
#include <errno.h>
#include <unistd.h>

#include "../constants.h"
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
/*
 * This source file makes sure the user who started the tool( usually non root user ) has permission 
 * to perform operations they want on paths they presented.
 * 
 * This feature allows tradition unix permissions to be set on a paths to control non user access to volumes  
 */

static int has_access( const char * path,int c )
{
	int f ;
	if( c == READ )
		f = open( path,O_RDONLY );
	else
		f = open( path,O_WRONLY );
	
	if( f >= 0 ){
		close( f ) ;
		return 0 ;
	}else{
		switch( errno ){
			case EACCES : return 1 ; /* permission denied */
			case ENOENT : return 2 ; /* invalid path*/
			default     : return 3 ; /* common error */    
		}
	}
}

static int create_group( const char * groupname )
{
	process_t p ;
	int st = 1 ;
	if( zuluCryptSecurityGainElevatedPrivileges() ){
		p = Process( ZULUCRYPTgroupadd ) ;
		ProcessSetArgumentList( p,"-f",groupname,ENDLIST ) ;
		ProcessStart( p ) ;
		st = ProcessExitStatus( p ) ;
		ProcessDelete( &p ) ;
	}
	return st == 0 ;
}

int zuluCryptUserIsAMemberOfAGroup( uid_t uid,const char * groupname )
{
	int i = 0 ;
	struct group * grp ;
	struct passwd * pass ;
	
	const char ** entry ;
	const char * name ;
	
	if( groupname == NULL )
		return 0 ;
	pass = getpwuid( uid ) ;
	
	if( pass == NULL )
		return 0 ;
	
	grp = getgrnam( groupname ) ;
	
	if( grp == NULL ){
		create_group( groupname )  ;
		return 0 ;
	}
	
	name = ( const char * )pass->pw_name ;
	entry = ( const char ** )grp->gr_mem ;
	
	while( entry[ i ] != NULL ){
		if( strcmp( entry[ i ],name ) == 0 ){
			return 1 ;
		}else{
			i++ ;
		}
	}
	
	return 0 ;
}

static int has_security_access( const char * path,int mode )
{
	int st ;
	if( strncmp( path,"/dev/shm/",9 ) == 0 )
		return 2 ;
	if( strncmp( path,"/dev/",5 ) != 0 ){
		return has_access( path,mode ) ;
	}else{
		if( zuluCryptSecurityGainElevatedPrivileges() ){
			st = has_access( path,mode ) ;
			zuluCryptSecurityDropElevatedPrivileges() ;
			return st ;
		}else{
			return 3 ;
		}
	}
}

static int check_permissions( const char * path,int mode,const char * groupname,uid_t uid )
{
	if( uid == 0 ){
		return has_access( path,mode ) ;
	}else{
		/*
		 * zuluCryptPartitionIsSystemPartition() is defined in ./partitions.c
		 */
		if( zuluCryptPartitionIsSystemPartition( path ) ){
			if( zuluCryptUserIsAMemberOfAGroup( uid,groupname ) ){
				return has_security_access( path,mode ) ;
			}else{
				return 3 ;
			}
		}else{
			return has_security_access( path,mode ) ;
		}
	}
}

int zuluCryptSecurityPathIsValid( const char * path,uid_t uid __attribute__((unused)) )
{
	int st = 0 ;
	if( strncmp( path,"/dev/",5 ) != 0 )
		return has_access( path,READ ) == 0 ;
	else{
		if( zuluCryptSecurityGainElevatedPrivileges() ){
			st = has_access( path,READ ) == 0 ;
			zuluCryptSecurityDropElevatedPrivileges() ;
		}
		return st ;
	}
}

int zuluCryptSecurityCanOpenPathForReading( const char * path,uid_t uid )
{
	return check_permissions( path,READ,"zulucrypt",uid ) ;
}

int zuluCryptSecurityCanOpenPathForWriting( const char * path,uid_t uid )
{
	return check_permissions( path,WRITE,"zulucrypt-write",uid ) ;
}

static void _create_prefix_directories( void )
{
	mkdir( "/run",S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH | S_IROTH ) ;
	chown( "/run/",0,0 ) ;
	mkdir( "/run/media",S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH | S_IROTH ) ;
	chown( "/run/media",0,0 ) ;
}

static string_t _create_default_mount_point( const char * device,uid_t uid,string_t path )
{
	string_t st = StringVoid ;
	const char * p = StringContent( path ) ;
	if( zuluCryptSecurityGainElevatedPrivileges() ){
		_create_prefix_directories() ;
		mkdir( p,S_IRWXU ) ;
		chown( p,uid,uid ) ;
		p = StringAppend( path,strrchr( device,'/' ) ) ;
		if( mkdir( p,S_IRWXU ) == 0 ){
			st = path ;
			chown( p,uid,uid ) ;
		}else{
			StringDelete( &path ) ;
		}
		zuluCryptSecurityDropElevatedPrivileges() ;
	}
	return st ;
}

static string_t _create_custom_mount_point( const char * label,uid_t uid,string_t path )
{
	string_t st = StringVoid ;
	const char * p = StringAppend( path,"/" ) ;
	const char * q = strrchr( label,'/' ) ;
	if( zuluCryptSecurityGainElevatedPrivileges() ){
		_create_prefix_directories() ;
		mkdir( p,S_IRWXU ) ;
		chown( p,uid,uid ) ;
		if( q == NULL ){
			p = StringAppend( path,label ) ;
		}else{
			p = StringAppend( path,q + 1 ) ;
		}
		if( mkdir( p,S_IRWXU ) == 0 ){
			st = path ;
			chown( p,uid,uid ) ;
		}else{
			StringDelete( &path ) ;
		}
		zuluCryptSecurityDropElevatedPrivileges() ;
	}
	return st ;
}

int zuluCryptSecurityMountPointPrefixMatch( const char * m_path,uid_t uid )
{
	int st ;
	int xt ;
	string_t uname = zuluCryptGetUserName( uid ) ;
	const char * str = StringPrepend( uname,"/run/media/" ) ;
	xt = StringLength( uname ) ;
	st = strncmp( str,m_path,xt ) ;
	StringDelete( &uname ) ;
	return st == 0 ;
}

string_t zuluCryptSecurityCreateMountPoint( const char * device,const char * label,uid_t uid )
{
	string_t path = zuluCryptGetUserName( uid ) ;
	StringPrepend( path,"/run/media/" ) ;
	if( label == NULL ){
		return _create_default_mount_point( device,uid,path ) ;
	}else{
		return _create_custom_mount_point( label,uid,path ) ;
	}
}

/*
 *  return values:
 *  5 - couldnt get key from the socket
 *  4 -permission denied
 *  1  invalid path
 *  2  insufficient memory to open file
 *  0  success * 
 */
int zuluCryptSecurityGetPassFromFile( const char * path,uid_t uid,string_t * st )
{
	/*
	 * zuluCryptGetUserHomePath() is defined in ../lib/user_get_home_path.c
	 */
	size_t s ;
	string_t p ; 
	const char * z ;
	
	/*
	 * zuluCryptPathStartsWith() is defined in ./real_path.c
	 */
	if( zuluCryptPathStartsWith( path,"/dev/" ) ){
		/*
		 * whats wrong with you? :-) cant get a key from "/dev/"
		 */
		return 4 ;
	}
	
	p = zuluCryptGetUserHomePath( uid ) ;
	z = StringAppend( p,".zuluCrypt-socket" ) ;
	s = StringLength( p ) ;
	
	if( strncmp( path,z,s ) == 0 ){
		StringDelete( &p ) ;
		/*
		 * path that starts with $HOME/.zuluCrypt-socket is treated not as a path to key file but as path
		 * to a local socket to get a passphrase 
		 * 
		 * This function is defined in ../pluginManager/zuluCryptPluginManager.c
		 */
		return zuluCryptGetKeyFromSocket( path,st,uid ) > 0 ? 0 : 5 ;
	}
	
	StringDelete( &p ) ;
	
	switch ( zuluCryptSecurityCanOpenPathForReading( path,uid ) ){
		case 1 : return 4 ;
		case 2 : return 1 ;
	}
	
	switch( StringGetFromFile_3( st,path,0,KEYFILE_MAX_SIZE ) ){
		case 1 : return 1 ;
		case 2 : return 4 ;
		case 3 : return 2 ;
	}
	
	return 0 ;
}

int zuluCryptSecurityGainElevatedPrivileges( void )
{
	return setuid( 0 ) == 0 ;
}

/*
 * global_variable_user_uid is an extern variable set in main function in ./main.c
 */
uid_t global_variable_user_uid ;
int zuluCryptSecurityDropElevatedPrivileges( void )
{
	int x = seteuid( global_variable_user_uid ) ;
	int y = setuid( global_variable_user_uid ) ;
	return ( x == y ) == 1 ;
}

char * zuluCryptSecurityEvaluateDeviceTags( const char * tag,const char * values )
{
	char * result ;
	if( setuid( 0 ) != 0 )
		return NULL ;
	result = blkid_evaluate_tag( tag,values,NULL) ;
	zuluCryptSecurityDropElevatedPrivileges() ;
	return result ;
}

string_t zuluCryptSecurityGetFileSystemFromDevice( const char * path )
{
	string_t st = StringVoid ;
	if( zuluCryptSecurityGainElevatedPrivileges() ){
		/*
		 * zuluCryptGetFileSystemFromDevice() is defined in ../lib/mount_volume.c
		 */
		st = zuluCryptGetFileSystemFromDevice( path ) ;
		zuluCryptSecurityDropElevatedPrivileges() ;
	}
	return st ;
}

void zuluCryptSecuritySanitizeTheEnvironment( uid_t uid )
{
	char * c ;
	extern char ** environ ;
	const char ** env = ( const char ** ) environ ;
	string_t st ;
	ssize_t index ;
	stringList_t stl = StringListVoid ;
	StringListIterator  it ;
	StringListIterator end ;
	
	/*
	 * below two functions are defined in ../lib/user_home_path.c
	 */
	string_t user_home = zuluCryptGetUserHomePath( uid ) ;
	string_t user_name = zuluCryptGetUserName( uid ) ;
	
	/*
	 * dont want to iterate over an array while changing it size through deleting its members,so going to 
	 * make a copy of it and then delete its members getting them from the copy
	 */
	while( *env ){
		stl = StringListAppend( stl,*env ) ;
		env++ ;
	}

	it  = StringListBegin( stl ) ;
	end = StringListEnd( stl ) ;
	
	for( ; it != end ;it++ ){
		st = *it ;
		index = StringIndexOfChar( st,0,'=' ) ;
		if( index >= 0 ){
			unsetenv( StringSubChar( st,index,'\0' ) ) ;
		}
	}
	
	putenv( "PROGRAM_NAME=zuluCrypt" ) ;
	c = ( char * )StringPrepend( user_home,"HOME=" ) ;
	putenv( c ) ;
	c = ( char * )StringPrepend( user_name,"USER=" ) ;
	putenv( c ) ;
		
	putenv( "PATH=/bin:/sbin/:/usr/bin:/usr/local/bin:/usr/local/sbin:/usr/sbin:/usr/bin" );
	putenv( "IFS= \t\n" );
	
	StringListDelete( &stl ) ;
	StringMultipleDelete( &st,&user_home,&user_name,END ) ;
}
