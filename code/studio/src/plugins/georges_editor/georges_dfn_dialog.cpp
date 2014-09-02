#include "georges_dfn_dialog.h"
#include <QInputDialog>
#include <QMessageBox>

#include "georges.h"
#include "dfn_browser_ctrl.h"

class GeorgesDFNDialogPvt
{
public:
	GeorgesDFNDialogPvt()
	{
		dfn = NULL;
		ctrl = new DFNBrowserCtrl();
	}

	~GeorgesDFNDialogPvt()
	{
		delete ctrl;
		ctrl = NULL;
	}

	NLGEORGES::CFormDfn *dfn;
	DFNBrowserCtrl *ctrl;
};

GeorgesDFNDialog::GeorgesDFNDialog( QWidget *parent ) :
GeorgesDockWidget( parent )
{
	m_ui.setupUi( this );
	
	m_ui.addButton->setEnabled( false );
	m_ui.removeButton->setEnabled( false );

	m_pvt = new GeorgesDFNDialogPvt();
	m_pvt->ctrl->setBrowser( m_ui.browser );

	setupConnections();
}

GeorgesDFNDialog::~GeorgesDFNDialog()
{
	delete m_pvt;
	m_pvt = NULL;
}

bool GeorgesDFNDialog::load( const QString &fileName )
{
	GeorgesQt::CGeorges georges;
	NLGEORGES::UFormDfn *udfn = georges.loadFormDfn( fileName.toUtf8().constData() );
	if( udfn == NULL )
		return false;

	setWindowTitle( fileName );

	NLGEORGES::CFormDfn *cdfn = static_cast< NLGEORGES::CFormDfn* >( udfn );
	m_pvt->dfn = cdfn;
	m_pvt->ctrl->setDFN( cdfn );

	uint c = m_pvt->dfn->getNumEntry();
	for( uint i = 0; i < c; i++ )
	{
		NLGEORGES::CFormDfn::CEntry &entry = m_pvt->dfn->getEntry( i );
		m_ui.list->addItem( entry.getName().c_str() );
	}

	if( c > 0 )
	{
		m_ui.list->setCurrentRow( 0 );
	}

	m_ui.commentsEdit->setPlainText( cdfn->getComment().c_str() );
	m_ui.logEdit->setPlainText( cdfn->Header.Log.c_str() );

	return true;
}

void GeorgesDFNDialog::write()
{
	setModified( false );
	setWindowTitle( windowTitle().remove( "*" ) );
}

void GeorgesDFNDialog::onAddClicked()
{
	QString name = QInputDialog::getText( this,
											tr( "New element" ),
											tr( "Enter name of the new element" ) );

	QList< QListWidgetItem* > list = m_ui.list->findItems( name, Qt::MatchFixedString );
	if( !list.isEmpty() )
	{
		QMessageBox::information( this,
									tr( "Item already exists" ),
									tr( "That item already exists!" ) );
		return;
	}

	m_ui.list->addItem( name );
}

void GeorgesDFNDialog::onRemoveClicked()
{
	int row = m_ui.list->currentRow();
	if( row < 0 )
		return;

	QListWidgetItem *item = m_ui.list->takeItem( row );
	delete item;
}

void GeorgesDFNDialog::onCurrentRowChanged( int row )
{
	if( row < 0 )
		return;

	m_pvt->ctrl->onElementSelected( row );
}

void GeorgesDFNDialog::onValueChanged( const QString &key, const QString &value )
{
	if( !isModified() )
	{
		setModified( true );
		setWindowTitle( windowTitle() + "*" );
		
		Q_EMIT modified();
	}
}

void GeorgesDFNDialog::setupConnections()
{
	connect( m_ui.addButton, SIGNAL( clicked( bool ) ), this, SLOT( onAddClicked() ) );
	connect( m_ui.removeButton, SIGNAL( clicked( bool ) ), this, SLOT( onRemoveClicked() ) );
	connect( m_ui.list, SIGNAL( currentRowChanged( int ) ), this, SLOT( onCurrentRowChanged( int ) ) );
	connect( m_pvt->ctrl, SIGNAL( valueChanged( const QString&, const QString& ) ), this, SLOT( onValueChanged( const QString&, const QString& ) ) );
}

