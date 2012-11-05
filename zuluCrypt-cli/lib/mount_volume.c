/*
 * 
 *  Copyright (c) 2011
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

#include <sys/mount.h>
#include <mntent.h>
#include <blkid/blkid.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * below header file does not ship with the source code, it is created at configure time
 * */
#include "libmount_header.h"

#define FAT_FAMILY_FS 1
#define OTHER_FAMILY_FS 2

typedef struct{
	const char * device ;
	const char * m_point ;
	const char * mode ;
	const char * fs ;
	const char * opts ;
	uid_t uid ;
	unsigned long m_flags ;
}m_struct;

static inline int zuluExit( int st,stringList_t stl )
{
	StringListDelete( &stl ) ;
	return st ;
}

static inline string_t resolveUUIDAndLabel( string_t st )
{
	char * e = NULL ;
	string_t xt = StringVoid ;
	
	if( StringStartsWith( st,"LABEL=" ) ) {
		e = blkid_evaluate_tag( "LABEL",StringContent( st ) + 6,NULL ) ;
		xt = StringInherit( &e ) ;
	}else if( StringStartsWith( st,"UUID=" ) ){
		e = blkid_evaluate_tag( "UUID",StringContent( st ) + 5,NULL ) ;
		xt = StringInherit( &e ) ;
	}
		
	return xt ;
}

string_t zuluCryptGetMountOptionsFromFstab( const char * device,int pos )
{
	string_t options = StringVoid ;
	string_t entry = StringVoid;
	string_t fstab = StringGetFromFile( "/etc/fstab" );
	
	stringList_t fstabList  ;
	stringList_t entryList  ;
	
	size_t i ;
	size_t j ;
	
	int st ;
	
	if( fstab == StringVoid )
		return StringVoid ;
	
	fstabList = StringListStringSplit( fstab,'\n' ) ;
	
	StringDelete( &fstab ) ;
	
	if( fstabList == StringListVoid )
		return StringVoid ;
		
	j = StringListSize( fstabList ) ;
	
	for( i = 0 ; i < j ; i++ ){
		
		entry = StringListStringAt( fstabList,i ) ;
		entryList = StringListStringSplit( entry,' ' ) ;
		
		if( entryList == StringListVoid )
			continue ;
		
		entry = StringListStringAt( entryList,0 ) ;
		
		if( !StringStartsWith( entry,"#" ) ){
		
			if( StringStartsWith( entry,"/dev/" ) ){
				st = StringEqual( entry,device ) ;
			}else{
				entry = resolveUUIDAndLabel( entry ) ;
				st = StringEqual( entry,device ) ;
				StringDelete( &entry ) ;
			}
		
			if( st == 1 ){
				options = StringListDetachAt( entryList,pos ) ;
				StringListDelete( &entryList ) ;
				break ;
			}
		}
		
		StringListDelete( &entryList ) ;
	}
	
	StringListDelete( &fstabList ) ;
	
	return options ;
}

static inline int ms_family( const char * fs )
{
	if(     strcmp( fs,"ntfs" ) == 0  || 
		strcmp( fs,"vfat" ) == 0  || 
		strcmp( fs,"fat" ) == 0   ||
		strcmp( fs,"msdos" ) == 0 ||
		strcmp( fs,"umsdos" ) == 0 )
		return 1 ;
	else
		return 0 ;
}

static inline int other_fs( const char * fs )
{
	if( strcmp( fs,"affs" ) == 0 || strcmp( fs,"hfs" ) == 0 || strcmp( fs,"iso9660" ) == 0 )
		return 1 ;
	else
		return 0 ;
}

static inline string_t set_mount_options( m_struct * mst )
{
	string_t opt = zuluCryptGetMountOptionsFromFstab( mst->device,3 ) ;
	string_t uid = StringIntToString( mst->uid ) ;

	if( opt == StringVoid )
		opt = String( mst->mode ) ;
	else	
		StringMultipleAppend( opt,",",mst->mode,'\0' ) ;
	
	if( ms_family( mst->fs ) ){
		if( !StringContains( opt,"dmask=" ) )
			StringAppend( opt,",dmask=0000" ) ;
		if( !StringContains( opt,"umask=" ) )
			StringAppend( opt,",umask=0000" ) ;
		if( !StringContains( opt,"uid=" ) )
			StringAppend( opt,",uid=UID" ) ;
		if( !StringContains( opt,"gid=" ) )
			StringAppend( opt,",gid=UID" ) ;
		if( !StringContains( opt,"fmask=" ) )
			StringAppend( opt,",fmask=0000" ) ;
		
		if( strcmp( mst->fs,"vfat" ) == 0 ){
			if( !StringContains( opt,"flush" ) )
				StringAppend( opt,",flush" ) ;
			if( !StringContains( opt,"shortname=" ) )
				StringAppend( opt,",shortname=mixed" ) ;
		}
		
	}else if( other_fs( mst->fs ) ){ 
		
		if( !StringContains( opt,"uid=" ) )
			StringAppend( opt,"uid=UID" ) ;
		if( !StringContains( opt,"gid=" ) )
			StringAppend( opt,"gid=UID" ) ;
	}else{
		/*
		 * ext file systems and raiserfs among others go here
		 * we dont set any options for them.
		 */
		;
	}
	
	StringReplaceString( opt,"UID",StringContent( uid ) );
	StringDelete( &uid ) ;
	
	/*
	 * Below options are not file system options and are rejectected by mount() command and hence we are removing them.
	 */
	StringRemoveString( opt,"nouser" ) ;
	StringRemoveString( opt,"users" ) ;
	StringRemoveString( opt,"user" ) ;
	StringRemoveString( opt,"default" ) ;
	StringRemoveString( opt,"noauto" ) ;
	StringRemoveString( opt,"auto" ) ;
	
	/*
	 * remove below two now because we are going to add them below,reason for removing them 
	 * and readding them is because we want to make sure they are at the beginning of the string
	 */
	StringRemoveString( opt,"ro" ) ;
	StringRemoveString( opt,"rw" ) ;
	
	if( mst->m_flags == MS_RDONLY )
		StringPrepend( opt,"ro," ) ;
	else
		StringPrepend( opt,"rw," ) ;
	
	StringReplaceString( opt,",,","," );
		
	if( StringEndsWith( opt,"," ) )
		StringRemoveRight( opt,1 ) ;
	
	mst->opts = StringContent( opt ) ;
	
	return opt;
}

static inline int mount_ntfs( const m_struct * mst )
{
	int status ;
	process_t p = Process( ZULUCRYPTmount ) ;
	ProcessSetArgumentList( p,"-t","ntfs-3g","-o",mst->opts,mst->device,mst->m_point,ENDLIST ) ;
	ProcessStart( p ) ;
	status = ProcessExitStatus( p ) ; 
	ProcessDelete( &p ) ;
	return status ;
}

static inline int mount_mapper( const m_struct * mst )
{
	int h = mount( mst->device,mst->m_point,mst->fs,mst->m_flags,mst->opts + 3 ) ;
	
	if( h == 0 && mst->m_flags != MS_RDONLY && ms_family( mst->fs ) == 0 && other_fs( mst->fs ) == 0 )
		chmod( mst->m_point,S_IRWXU|S_IRWXG|S_IRWXO ) ;
	
	return h ;
}

int zuluCryptMountVolume( const char * mapper,const char * m_point,const char * mode,uid_t id )
{
	struct mntent mt  ;
	blkid_probe blkid ;
	int h ;
	const char * cf = NULL ;
	FILE * f ;
#if USE_NEW_LIBMOUNT_API
	struct libmnt_lock * m_lock ;
#else
	mnt_lock * m_lock ;
#endif
	stringList_t stl = StringListInit() ;
	
	string_t * opts ;
	string_t fs ;
	
	m_struct mst ;
	mst.device = mapper ;
	mst.m_point = m_point ;
	mst.mode = mode ;
	mst.uid = id ;
	
	if( strstr( mode,"ro" ) != NULL )
		mst.m_flags = MS_RDONLY ;
	else
		mst.m_flags = 0 ;
	
	blkid = blkid_new_probe_from_filename( mapper ) ;
	blkid_do_probe( blkid );	
	blkid_probe_lookup_value( blkid,"TYPE",&cf,NULL ) ;
		
	fs = StringListAssignString( stl,String( cf ) );
		
	blkid_free_probe( blkid );
	
	if( fs == StringVoid ){
		/*
		 * failed to read file system,probably because the volume does have any or is an encrypted plain volume
		 */
		return zuluExit( 4,stl ) ;
	}
	
	if( StringEqual( fs,"crypto_LUKS" ) ){
		/*
		 * we cant mount an encrypted volume, exiting
		 */
		return zuluExit( 4,stl ) ;
	}
	
	mst.fs = StringContent( fs ) ;
	
	opts = StringListAssign( stl ) ;
	*opts = set_mount_options( &mst ) ;
	
	/*
	 * Currently, i dont know how to use mount system call to use ntfs-3g instead of ntfs to mount ntfs file systems.
	 * Use fork to use mount executable as a temporary solution.
	*/
	if( StringEqual( fs,"ntfs" ) ){
		h = mount_ntfs( &mst ) ;
		StringListDelete( &stl ) ;
		switch( h ){
			case 0  : return 0 ;
			case 16 : return 12 ;
			default : return 1 ;
		}
	}
	 
	/*
	 * zuluCryptMtabIsAtEtc() is defined in print_mounted_volumes.c
	 * 1 is return if "mtab" is found to be a file located at "/etc/"
	 * 0 is returned otherwise,probably because "mtab" is a soft like to "/proc/mounts"
	 */
	
	if( !zuluCryptMtabIsAtEtc() ){
		h = mount_mapper( &mst ) ;
	}else{
		/* 
		 * "/etc/mtab" is not a symbolic link to /proc/mounts, manually,add an entry to it since 
		 * mount API does not
		 */		
		m_lock = mnt_new_lock( "/etc/mtab~",getpid() ) ;
		if( mnt_lock_file( m_lock ) != 0 ){
			h = 12 ;
		}else{		
			h = mount_mapper( &mst ) ;
			if( h == 0 ){
				f = setmntent( "/etc/mtab","a" ) ;
				mt.mnt_fsname = ( char * ) mst.device ;
				mt.mnt_dir    = ( char * ) mst.m_point ;
				mt.mnt_type   = ( char * ) mst.fs ;
				mt.mnt_opts   = ( char * ) mst.opts ;
				mt.mnt_freq   = 0 ;
				mt.mnt_passno = 0 ;
				addmntent( f,&mt ) ;
				endmntent( f ) ;
			}
			
			mnt_unlock_file( m_lock ) ;
		}
		
		mnt_free_lock( m_lock ) ;
	}
	
	return zuluExit( h,stl ) ;
}

