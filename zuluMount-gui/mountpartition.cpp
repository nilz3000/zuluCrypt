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


#include "mountpartition.h"
#include "ui_mountpartition.h"

mountPartition::mountPartition( QWidget * parent,QTableWidget * table,QString folderOpener ) :
	QWidget( parent ),m_ui(new Ui::mountPartition)
{
	m_ui->setupUi(this);
	m_table = table ;
	this->setFixedSize( this->size() );
	this->setWindowFlags( Qt::Window | Qt::Dialog );
	this->setFont( parent->font() );

	m_ui->pbMount->setFocus();

	m_ui->checkBoxMountReadOnly->setCheckState( openvolumereadonly::getOption( QString( "zuluMount-gui" ) ) );

	connect( m_ui->pbMount,SIGNAL( clicked() ),this,SLOT(pbMount() ) ) ;
	connect( m_ui->pbMountFolder,SIGNAL( clicked() ),this,SLOT( pbOpenMountPath() ) ) ;
	connect( m_ui->pbCancel,SIGNAL( clicked() ),this,SLOT( pbCancel() ) ) ;
	connect( m_ui->checkBox,SIGNAL( stateChanged( int ) ),this,SLOT( stateChanged( int ) ) ) ;
	connect( m_ui->checkBoxMountReadOnly,SIGNAL( stateChanged(int) ),this,SLOT( checkBoxReadOnlyStateChanged( int ) ) ) ;
	m_ui->pbMountFolder->setIcon( QIcon( QString( ":/folder.png" ) ) );

	userfont F( this ) ;
	this->setFont( F.getFont() );

	m_ui->pbMountFolder->setVisible( false );
	m_folderOpener = folderOpener ;
}

void mountPartition::checkBoxReadOnlyStateChanged( int state )
{
	m_ui->checkBoxMountReadOnly->setEnabled( false );
	m_ui->checkBoxMountReadOnly->setChecked( openvolumereadonly::setOption( this,state,QString( "zuluMount-gui" ) ) );
	m_ui->checkBoxMountReadOnly->setEnabled( true );
	if( m_ui->lineEdit->text().isEmpty() )
		m_ui->lineEdit->setFocus();
	else
		m_ui->pbMount->setFocus();
}

void mountPartition::enableAll()
{
	if( m_label != QString( "Nil" ) )
		m_ui->checkBox->setEnabled( true );
	m_ui->checkBoxMountReadOnly->setEnabled( true );
	m_ui->labelMountPoint->setEnabled( true );
	m_ui->lineEdit->setEnabled( true );
	m_ui->pbCancel->setEnabled( true );
	m_ui->pbMount->setEnabled( true );
	m_ui->pbMountFolder->setEnabled( true );
}

void mountPartition::disableAll()
{
	m_ui->pbMount->setEnabled( false );
	m_ui->checkBox->setEnabled( false );
	m_ui->checkBoxMountReadOnly->setEnabled( false );
	m_ui->labelMountPoint->setEnabled( false );
	m_ui->lineEdit->setEnabled( false );
	m_ui->pbCancel->setEnabled( false );
	m_ui->pbMountFolder->setEnabled( false );
}

void mountPartition::pbCancel()
{
	this->HideUI();
}

void mountPartition::pbMount()
{
	QString test_mount = m_ui->lineEdit->text() ;
	if( test_mount.contains( QString( "/" ) ) ){
		DialogMsg msg( this ) ;
		msg.ShowUIOK( tr( "ERROR" ),tr( "\"/\" character is not allowed in the mount name field" ) ) ;
		m_ui->lineEdit->setFocus();
		return ;
	}
	this->disableAll();
	managepartitionthread * part = new managepartitionthread() ;
	part->setDevice( m_path );
	if( m_ui->checkBoxMountReadOnly->isChecked() )
		part->setMode( QString( "ro" ) );
	else
		part->setMode( QString( "rw" ) );
	m_point = m_ui->lineEdit->text() ;
	part->setMountPoint( utility::mountPath( m_point ) );
	connect( part,SIGNAL( signalMountComplete( int,QString ) ),this,SLOT( slotMountComplete( int,QString ) ) ) ;

	part->startAction( QString( "mount" ) ) ;
	//savemountpointpath::savePath( m_ui->lineEdit->text(),QString( "zuluMount-MountPointPath" ) ) ;
}

void mountPartition::pbOpenMountPath()
{
	QString p = tr( "Select Path to mount point folder" ) ;
	QString Z = QFileDialog::getExistingDirectory( this,p,QDir::homePath(),QFileDialog::ShowDirsOnly ) ;

	if( !Z.isEmpty() ){
		Z = Z + QString( "/" ) + m_ui->lineEdit->text().split( "/" ).last() ;
		m_ui->lineEdit->setText( Z );
	}
}

void mountPartition::ShowUI( QString path,QString label )
{
	m_path = path ;
	m_label = label ;
	m_point = m_path.split( QString( "/" ) ).last() ;
	m_ui->lineEdit->setText( m_point ) ;
	//m_ui->lineEdit->setText( savemountpointpath::getPath( path,QString( "zuluMount-MountPointPath" ) ) ) ;

	if( label == QString( "Nil" ) )
		m_ui->checkBox->setEnabled( false );
	this->show();
}

void mountPartition::stateChanged( int i )
{
	Q_UNUSED( i ) ;
	m_ui->checkBox->setEnabled( false );
	if( m_ui->checkBox->isChecked() )
		m_ui->lineEdit->setText( m_label );
	else
		m_ui->lineEdit->setText( m_path.split( QString( "/" ) ).last() );
	m_ui->checkBox->setEnabled( true );
}

void mountPartition::volumeMiniProperties( QString prp )
{
	MainWindow::volumeMiniProperties( m_table,prp,utility::mountPath( m_point ) ) ;
	this->HideUI();
}

void mountPartition::fileManagerOpenStatus( int exitCode, int exitStatus,int startError )
{
	Q_UNUSED( startError ) ;
	if( exitCode != 0 || exitStatus != 0 ){
		DialogMsg msg( this ) ;
		msg.ShowUIOK( tr( "warning" ),tr( "could not open mount point because \"%1\" tool does not appear to be working correctly").arg( m_folderOpener ) );
	}
}

void mountPartition::slotMountComplete( int status,QString msg )
{
	if( status ){
		DialogMsg m( this ) ;
		m.ShowUIOK( QString( "ERROR" ),msg );
		this->enableAll();
	}else{
		managepartitionthread * mpt = new managepartitionthread() ;
		mpt->setDevice( m_path );
		connect( mpt,SIGNAL( signalProperties( QString ) ),this,SLOT( volumeMiniProperties( QString ) ) ) ;
		mpt->startAction( QString( "volumeMiniProperties" ) ) ;

		openmountpointinfilemanager * omp = new openmountpointinfilemanager( m_folderOpener,utility::mountPath( m_point ) ) ;
		connect( omp,SIGNAL( errorStatus( int,int,int ) ),this,SLOT( fileManagerOpenStatus( int,int,int ) ) ) ;
		omp->start();
	}
}
void mountPartition::HideUI()
{
	emit hideUISignal();
	this->hide();
}

void mountPartition::closeEvent( QCloseEvent * e )
{
	e->ignore();
	this->HideUI();
}

mountPartition::~mountPartition()
{
	delete m_ui;
}


