
/*
   Copyright (c) 2003-2006 Clarence Dang <dang@kde.org>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <kpcolorsimilaritydialog.h>

#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>

#include <klocale.h>
#include <knuminput.h>

#include <kpcolorsimilaritycube.h>


// public static
const double kpColorSimilarityDialog::maximumColorSimilarity = .30;


kpColorSimilarityDialog::kpColorSimilarityDialog (kpMainWindow *mainWindow,
                                                  QWidget *parent)
    : KDialog (parent ),
      m_mainWindow (mainWindow)
{
    setCaption( i18n ("Color Similarity") );
    setButtons( KDialog::Ok | KDialog::Cancel);
    QWidget *baseWidget = new QWidget (this);
    setMainWidget (baseWidget);


    QGroupBox *cubeGroupBox = new QGroupBox (i18n ("Preview"), baseWidget);

    m_colorSimilarityCube = new kpColorSimilarityCube (kpColorSimilarityCube::Plain,
                                                       mainWindow, cubeGroupBox);
    m_colorSimilarityCube->setMinimumSize (240, 180);

    QPushButton *updatePushButton = new QPushButton (i18n ("&Update"), cubeGroupBox);


    QVBoxLayout *cubeLayout = new QVBoxLayout (cubeGroupBox);
    cubeLayout->setSpacing(spacingHint ());
    cubeLayout->setMargin(marginHint () * 2);
    cubeLayout->addWidget (m_colorSimilarityCube, 1/*stretch*/);
    cubeLayout->addWidget (updatePushButton, 0/*stretch*/, Qt::AlignHCenter);


    connect (updatePushButton, SIGNAL (clicked ()),
             this, SLOT (slotColorSimilarityValueChanged ()));


    QGroupBox *inputGroupBox = new QGroupBox (i18n ("RGB Color Cube Distance"), baseWidget);

    m_colorSimilarityInput = new KIntNumInput (inputGroupBox);
    m_colorSimilarityInput->setRange (0, int (kpColorSimilarityDialog::maximumColorSimilarity * 100 + .1/*don't floor below target int*/),
                                      5/*step*/, true/*slider*/);
    m_colorSimilarityInput->setSuffix (i18n ("%"));
    m_colorSimilarityInput->setSpecialValueText (i18n ("Exact Match"));


    QVBoxLayout *inputLayout = new QVBoxLayout (inputGroupBox);
    inputLayout->setSpacing(spacingHint ());
    inputLayout->setMargin(marginHint () * 2);
    inputLayout->addWidget (m_colorSimilarityInput);


    connect (m_colorSimilarityInput, SIGNAL (valueChanged (int)),
             this, SLOT (slotColorSimilarityValueChanged ()));


    QVBoxLayout *baseLayout = new QVBoxLayout (baseWidget);
    baseLayout->setSpacing(spacingHint () * 2);
    baseLayout->setMargin(0/*margin*/);
    baseLayout->addWidget (cubeGroupBox, 1/*stretch*/);
    baseLayout->addWidget (inputGroupBox);
}

kpColorSimilarityDialog::~kpColorSimilarityDialog ()
{
}


// public
double kpColorSimilarityDialog::colorSimilarity () const
{
    return m_colorSimilarityCube->colorSimilarity ();
}

// public
void kpColorSimilarityDialog::setColorSimilarity (double similarity)
{
    m_colorSimilarityInput->setValue (qRound (similarity * 100));
}


// private slot
void kpColorSimilarityDialog::slotColorSimilarityValueChanged ()
{
    m_colorSimilarityCube->setColorSimilarity (double (m_colorSimilarityInput->value ()) / 100);
}


#include <kpcolorsimilaritydialog.moc>