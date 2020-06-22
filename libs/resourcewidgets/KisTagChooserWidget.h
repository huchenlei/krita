/*
 *    This file is part of the KDE project
 *    Copyright (c) 2002 Patrick Julien <freak@codepimps.org>
 *    Copyright (c) 2007 Jan Hambrecht <jaham@gmx.net>
 *    Copyright (c) 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *    Copyright (C) 2011 Srikanth Tiyyagura <srikanth.tulasiram@gmail.com>
 *    Copyright (c) 2011 José Luis Vergara <pentalis@gmail.com>
 *    Copyright (c) 2013 Sascha Suelzer <s.suelzer@gmail.com>
 *    Copyright (c) 2019 Boudewijn Rempt <boud@valdyas.org> 
 *    Copyright (c) 2020 Agata Cacko <cacko.azh@gmail.com>
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 */

#ifndef KISTAGCHOOSERWIDGET_H
#define KISTAGCHOOSERWIDGET_H

#include <QWidget>
#include "kritaresourcewidgets_export.h"

#include <KisTag.h>
#include <KisTagModel.h>

///
/// \brief The KisTagChooserWidget class is responsible for all the logic that tags combobox has in various resource choosers.
///
/// (Example of usage: tag combobox in Brushes docker).
/// It uses KisTagModel as a model for items in the combobox.
/// It is also responsible for the popup for tag removal, renaming and creation
/// that appears on the right side of the tag combobox (via KisTagToolButton)
/// All the logic for adding and removing tags is done through KisTagModel.
///
/// For logic related to tagging and untagging resources, check KisTaggingManager
/// and KisItemChooserContextMenu.
///
class KRITARESOURCEWIDGETS_EXPORT KisTagChooserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KisTagChooserWidget(KisTagModel* model, QWidget* parent);
    ~KisTagChooserWidget() override;


    /// \brief setCurrentIndex sets the current index in the combobox
    /// \param index index is the index of the tag in the combobox
    ///
    void setCurrentIndex(int index);

    ///
    /// \brief currentIndex returns the current index in tags combobox
    /// \return the index of the current item in tags combobox
    ///
    int currentIndex() const;

    /// \brief currentlySelectedTag returns the current tag from combobox
    /// \see currentIndex
    /// \return the tag that is currently selected in the tag combobox
    ///
    KisTagSP currentlySelectedTag();
    ///
    /// \brief selectedTagIsReadOnly checks whether the tag is readonly (generated by Krita)
    /// \return true if the tag was generated by Krita, false if it's just a normal tag
    ///
    bool selectedTagIsReadOnly();

Q_SIGNALS:
    ///
    /// \brief sigTagChosen is emitted when the selected tag in the combobox changes due to user interaction or by other means
    /// \param tag current tag
    ///
    void sigTagChosen(const KisTagSP tag);

public Q_SLOTS:

    ///
    /// \brief tagChanged slot for the signal from the combobox that the index changed
    /// \param index new index
    ///
    /// When the index in the combobox changes, for example because of user's interaction,
    ///  combobox emits a signal; this method is called when it happens.
    void tagChanged(int index);

private Q_SLOTS:
    ///
    /// \brief tagToolCreateNewTag slot for the signal from KisTagToolButton that a new tag needs to be created
    /// \param tag tag with the name to be created
    /// \return created tag taken from the model, with a valid id
    ///
    KisTagSP tagToolCreateNewTag(KisTagSP tag);
    ///
    /// \brief tagToolRenameCurrentTag slot for the signal from KisTagToolButton that the current tag needs to be renamed
    /// \param newName new name for the tag
    ///
    void tagToolRenameCurrentTag(const KisTagSP newName);
    ///
    /// \brief tagToolDeleteCurrentTag slot for the signal from the KisTagToolButton that the current tag needs to be deleted
    ///
    /// Note that tags are not deleted but just marked inactive in the database.
    ///
    void tagToolDeleteCurrentTag();

    ///
    /// \brief tagToolUndeleteLastTag slot for the signal from the KisTagToolButton that the last deleted tag needs to be undeleted
    /// \param tag tag to be undeleted (marked active)
    ///
    void tagToolUndeleteLastTag(const KisTagSP tag);

    ///
    /// \brief tagToolContextMenuAboutToShow slot for the signal from the KisTagToolButton that the popup will be shown soon
    ///
    /// Based on the current tag (if it's readonly or not), the popup looks different, so this function
    ///  sets the correct mode on the KisTagToolButton popup.
    ///
    void tagToolContextMenuAboutToShow();

    ///
    /// \brief slotModelAboutToBeReset is called before the tag model is being reset.
    ///
    /// It remembers the last selected tag to select it again after the reset of the model.
    ///
    void slotModelAboutToBeReset();
    ///
    /// \brief slotModelReset is called after the tag model is reset.
    ///
    /// It restores the last selected tag.
    ///
    void slotModelReset();


private:
    ///
    /// \brief setCurrentItem sets the tag from the param as the current tag in the combobox
    /// \param tag tag to be set as current in the combobox
    /// \return true if successful, false if not successful
    ///
    bool setCurrentItem(KisTagSP tag);

private:
    class Private;
    Private* const d;

};

#endif // KOTAGCHOOSERWIDGET_H
