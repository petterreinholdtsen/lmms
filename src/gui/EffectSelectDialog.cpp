/*
 * EffectSelectDialog.cpp - dialog to choose effect plugin
 *
 * Copyright (c) 2006-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - http://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "EffectSelectDialog.h"

#include "ui_EffectSelectDialog.h"

#include "gui_templates.h"
#include "embed.h"
#include "PluginFactory.h"

#include <QLabel>


EffectSelectDialog::EffectSelectDialog( QWidget * _parent ) :
	QDialog( _parent ),
	ui( new Ui::EffectSelectDialog ),
	m_sourceModel(),
	m_model(),
	m_descriptionWidget( NULL )
{
	ui->setupUi( this );

	setWindowIcon( embed::getIconPixmap( "setup_audio" ) );

	// query effects

	EffectKeyList subPluginEffectKeys;

	for (const Plugin::Descriptor* desc: pluginFactory->descriptors(Plugin::Effect))
	{
		if( desc->subPluginFeatures )
		{
			desc->subPluginFeatures->listSubPluginKeys(
				// as iterators are always stated to be not
				// equal with pointers, we dereference the
				// iterator and take the address of the item,
				// so we're on the safe side and the compiler
				// likely will reduce that to just "it"
							desc,
							subPluginEffectKeys );
		}
		else
		{
			m_effectKeys << EffectKey( desc, desc->name );

		}
	}

	m_effectKeys += subPluginEffectKeys;

	// and fill our source model
	QStringList pluginNames;
	for( EffectKeyList::ConstIterator it = m_effectKeys.begin(); it != m_effectKeys.end(); ++it )
	{
		if( ( *it ).desc->subPluginFeatures )
		{
			pluginNames += QString( "%1: %2" ).arg(  ( *it ).desc->displayName, ( *it ).name );
		}
		else
		{
			pluginNames += ( *it ).desc->displayName;
		}
	}

	int row = 0;
	for( QStringList::ConstIterator it = pluginNames.begin();
								it != pluginNames.end(); ++it )
	{
		m_sourceModel.setItem( row, 0, new QStandardItem( *it ) );
		++row;
	}

	// setup filtering
	m_model.setSourceModel( &m_sourceModel );
	m_model.setFilterCaseSensitivity( Qt::CaseInsensitive );

	connect( ui->filterEdit, SIGNAL( textChanged( const QString & ) ),
				&m_model, SLOT( setFilterRegExp( const QString & ) ) );
	connect( ui->filterEdit, SIGNAL( textChanged( const QString & ) ),
					this, SLOT( updateSelection() ) );

	ui->pluginList->setModel( &m_model );

	// setup selection model
	QItemSelectionModel * selectionModel = new QItemSelectionModel( &m_model );
	ui->pluginList->setSelectionModel( selectionModel );
	connect( selectionModel, SIGNAL( currentRowChanged( const QModelIndex &,
														const QModelIndex & ) ),
			SLOT( rowChanged( const QModelIndex &, const QModelIndex & ) ) );
	connect( ui->pluginList, SIGNAL( doubleClicked( const QModelIndex & ) ),
				SLOT( acceptSelection() ) );

	// try to accept current selection when pressing "OK"
	connect( ui->buttonBox, SIGNAL( accepted() ),
				this, SLOT( acceptSelection() ) );
	
	updateSelection();
	show();
}




EffectSelectDialog::~EffectSelectDialog()
{
	delete ui;
}




Effect * EffectSelectDialog::instantiateSelectedPlugin( EffectChain * _parent )
{
	if( !m_currentSelection.name.isEmpty() && m_currentSelection.desc )
	{
		return Effect::instantiate( m_currentSelection.desc->name,
										_parent, &m_currentSelection );
	}
	return NULL;
}




void EffectSelectDialog::acceptSelection()
{
	if( m_currentSelection.isValid() )
	{
		accept();
	}
}




void EffectSelectDialog::rowChanged( const QModelIndex & _idx,
										const QModelIndex & )
{
	delete m_descriptionWidget;
	m_descriptionWidget = NULL;

	if( m_model.mapToSource( _idx ).row() < 0 )
	{
		// invalidate current selection
		m_currentSelection = Plugin::Descriptor::SubPluginFeatures::Key();
	}
	else
	{
		m_currentSelection = m_effectKeys[m_model.mapToSource( _idx ).row()];
	}
    if( m_currentSelection.desc )
	{
		m_descriptionWidget = new QWidget;

        QHBoxLayout *hbox = new QHBoxLayout( m_descriptionWidget );

        Plugin::Descriptor const & descriptor = *( m_currentSelection.desc );

        if ( descriptor.logo )
        {
            QLabel *logoLabel = new QLabel( m_descriptionWidget );
            logoLabel->setPixmap( descriptor.logo->pixmap() );
            logoLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

            hbox->addWidget( logoLabel );
            hbox->setAlignment( logoLabel, Qt::AlignTop);
        }

        QWidget *textualInfoWidget = new QWidget( m_descriptionWidget );

        hbox->addWidget(textualInfoWidget);

        QVBoxLayout * textWidgetLayout = new QVBoxLayout( textualInfoWidget);
        textWidgetLayout->setMargin( 4 );
        textWidgetLayout->setSpacing( 0 );

        std::string stdName(descriptor.name);
        if ( stdName != "ladspaeffect" )
        {
            QLabel *label = new QLabel(m_descriptionWidget);
            QString labelText = "<p><b>" + tr("Name") + ":</b> " + QString::fromUtf8(descriptor.displayName) + "</p>";
            labelText += "<p><b>" + tr("Description") + ":</b> " + qApp->translate( "pluginBrowser", descriptor.description ) + "</p>";
            labelText += "<p><b>" + tr("Author") + ":</b> " + QString::fromUtf8(descriptor.author) + "</p>";

            label->setText(labelText);
            textWidgetLayout->addWidget(label);
        }

        if ( m_currentSelection.desc->subPluginFeatures )
        {
            QWidget *subWidget = new QWidget(textualInfoWidget);
            QVBoxLayout * subLayout = new QVBoxLayout( subWidget );
            subLayout->setMargin( 4 );
            subLayout->setSpacing( 0 );
            m_currentSelection.desc->subPluginFeatures->
                fillDescriptionWidget( subWidget, &m_currentSelection );
            foreach( QWidget * w, subWidget->findChildren<QWidget *>() )
            {
                if( w->parent() == subWidget )
                {
                    subLayout->addWidget( w );
                }
            }

            textWidgetLayout->addWidget(subWidget);
        }

        ui->scrollArea->setWidget( m_descriptionWidget );
		m_descriptionWidget->show();
	}
}




void EffectSelectDialog::updateSelection()
{
	// no valid selection anymore due to changed filter?
	if( ui->pluginList->selectionModel()->selection().size() <= 0 )
	{
		// then select our first item
		ui->pluginList->selectionModel()->select( m_model.index( 0, 0 ),
									QItemSelectionModel::ClearAndSelect );
		rowChanged( m_model.index( 0, 0 ), QModelIndex() );
	}
}





