/*
 * Copyright (c) 2013-2017 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dlgrequests.h"
#include "ui_dlgrequests.h"
#include <QMenu>
#include <QMessageBox>
#include "settings.h"
#include "okjsongbookapi.h"
#include <QProgressDialog>

extern Settings *settings;
extern OKJSongbookAPI *songbookApi;

DlgRequests::DlgRequests(RotationModel *rotationModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgRequests)
{
    curRequestId = -1;
    ui->setupUi(this);
    requestsModel = new RequestsTableModel(this);
    dbModel = new DbTableModel(this);
    dbDelegate = new DbItemDelegate(this);
    ui->tableViewRequests->setModel(requestsModel);
    connect(requestsModel, SIGNAL(layoutChanged()), this, SLOT(requestsModified()));
    ui->tableViewSearch->setModel(dbModel);
    ui->tableViewSearch->setItemDelegate(dbDelegate);
    ui->groupBoxAddSong->setDisabled(true);
    ui->groupBoxSongDb->setDisabled(true);
    connect(ui->tableViewRequests->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(requestSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tableViewSearch->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(songSelectionChanged(QItemSelection,QItemSelection)));
    connect(songbookApi, SIGNAL(synchronized(QTime)), this, SLOT(updateReceived(QTime)));
    connect(songbookApi, SIGNAL(sslError()), this, SLOT(sslError()));
    connect(songbookApi, SIGNAL(delayError(int)), this, SLOT(delayError(int)));
    rotModel = rotationModel;
    ui->comboBoxAddPosition->setEnabled(false);
    ui->comboBoxSingers->setEnabled(true);
    ui->lineEditSingerName->setEnabled(false);
    ui->labelAddPos->setEnabled(false);
    QStringList posOptions;
    posOptions << tr("After current singer");
    posOptions << tr("Fair (full rotation)");
    posOptions << tr("Bottom of rotation");
    ui->comboBoxAddPosition->addItems(posOptions);
    ui->comboBoxAddPosition->setCurrentIndex(1);
    ui->tableViewSearch->hideColumn(0);
    ui->tableViewSearch->hideColumn(5);
    ui->tableViewSearch->hideColumn(6);
    ui->tableViewSearch->hideColumn(7);
    ui->tableViewSearch->horizontalHeader()->resizeSection(4,75);
    ui->checkBoxDelOnAdd->setChecked(settings->requestRemoveOnRotAdd());
    connect(songbookApi, SIGNAL(venuesChanged(OkjsVenues)), this, SLOT(venuesChanged(OkjsVenues)));
    connect(ui->lineEditSearch, SIGNAL(escapePressed()), this, SLOT(lineEditSearchEscapePressed()));
    connect(ui->checkBoxDelOnAdd, SIGNAL(clicked(bool)), settings, SLOT(setRequestRemoveOnRotAdd(bool)));
    ui->cbxAutoShowRequestsDlg->setChecked(settings->requestDialogAutoShow());
    connect(ui->cbxAutoShowRequestsDlg, SIGNAL(clicked(bool)), settings, SLOT(setRequestDialogAutoShow(bool)));

    QFontMetrics fm(settings->applicationFont());
    QSize mcbSize(fm.height(), fm.height());

    ui->buttonRefresh->resize(mcbSize);
    ui->buttonRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->buttonRefresh->setIconSize(mcbSize);

    ui->pushButtonClearReqs->resize(mcbSize);
    ui->pushButtonClearReqs->setIcon(QIcon(QPixmap(":/icons/Icons/edit-clear.png").scaled(mcbSize)));
    ui->pushButtonClearReqs->setIconSize(mcbSize);
    autoSizeViews();


}

int DlgRequests::numRequests()
{
    return requestsModel->count();
}

DlgRequests::~DlgRequests()
{

    delete ui;
}

void DlgRequests::databaseAboutToUpdate()
{
    dbModel->revertAll();
    dbModel->setTable("");
}

void DlgRequests::databaseUpdateComplete()
{
    dbModel->refreshCache();
    dbModel->select();
}

void DlgRequests::databaseSongAdded()
{
    dbModel->select();
}

void DlgRequests::on_pushButtonClose_clicked()
{
    close();
}

void DlgRequests::requestsModified()
{
    if ((requestsModel->count() > 0) && (settings->requestDialogAutoShow()))
    {
        this->show();
    }
    autoSizeViews();
}

void DlgRequests::on_pushButtonSearch_clicked()
{
    dbModel->search(ui->lineEditSearch->text());
}

void DlgRequests::on_lineEditSearch_returnPressed()
{
    dbModel->search(ui->lineEditSearch->text());
}

void DlgRequests::requestSelectionChanged(const QItemSelection &current, const QItemSelection &previous)
{
    ui->groupBoxAddSong->setDisabled(true);
    if (current.indexes().size() == 0)
    {
        dbModel->search("yeahjustsomethingitllneverfind.imlazylikethat");
        ui->groupBoxSongDb->setDisabled(true);
        return;
    }
    QModelIndex index = current.indexes().at(0);
    Q_UNUSED(previous);
    if ((index.isValid()) && (ui->tableViewRequests->selectionModel()->selectedIndexes().size() > 0))
    {
        ui->groupBoxSongDb->setEnabled(true);
        ui->comboBoxSingers->clear();
        QString singerName = index.sibling(index.row(),0).data().toString();
        QStringList singers = rotModel->singers();
        ui->comboBoxSingers->addItems(singers);

        QString filterStr = index.sibling(index.row(),1).data().toString() + " " + index.sibling(index.row(),2).data().toString();
        dbModel->search(filterStr);
        ui->lineEditSearch->setText(filterStr);
        ui->lineEditSingerName->setText(singerName);

        int s = -1;
        for (int i=0; i < singers.size(); i++)
        {
            if (singers.at(i).toLower() == singerName.toLower())
            {
                s = i;
                break;
            }
        }
        if (s != -1)
        {
            ui->comboBoxSingers->setCurrentIndex(s);
        }
    }
    else
    {
        dbModel->search("yeahjustsomethingitllneverfind.imlazylikethat");
    }
}

void DlgRequests::songSelectionChanged(const QItemSelection &current, const QItemSelection &previous)
{
    Q_UNUSED(previous);
    if (current.indexes().size() == 0)
    {
        ui->groupBoxAddSong->setDisabled(true);
    }
    else
        ui->groupBoxAddSong->setEnabled(true);
}

void DlgRequests::on_radioButtonExistingSinger_toggled(bool checked)
{
    ui->comboBoxAddPosition->setEnabled(!checked);
    ui->comboBoxSingers->setEnabled(checked);
    ui->lineEditSingerName->setEnabled(!checked);
    ui->labelAddPos->setEnabled(!checked);
}

void DlgRequests::on_pushButtonClearReqs_clicked()
{
    QMessageBox msgBox;
    msgBox.setText("Are you sure?");
    msgBox.setInformativeText("This action will clear all received requests. This operation can not be undone.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.addButton(QMessageBox::Cancel);
    QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
    msgBox.exec();
    if (msgBox.clickedButton() == yesButton)
    {
        ui->tableViewRequests->selectionModel()->clearSelection();
        ui->lineEditSearch->clear();
        ui->lineEditSingerName->clear();
        ui->comboBoxSingers->clear();
        ui->radioButtonExistingSinger->setChecked(true);
        songbookApi->clearRequests();
    }
}

void DlgRequests::on_tableViewRequests_clicked(const QModelIndex &index)
{
    if (index.column() == 4)
    {
        songbookApi->removeRequest(index.data(Qt::UserRole).toInt());
        ui->tableViewRequests->selectionModel()->clearSelection();
        ui->lineEditSearch->clear();
        ui->lineEditSingerName->clear();
        ui->comboBoxSingers->clear();
        ui->radioButtonExistingSinger->setChecked(true);
    }
    else
    {
        curRequestId = index.data(Qt::UserRole).toInt();
    }
}

void DlgRequests::on_pushButtonAddSong_clicked()
{
    if (ui->tableViewRequests->selectionModel()->selectedIndexes().size() < 1)
        return;
    if (ui->tableViewSearch->selectionModel()->selectedIndexes().size() < 1)
        return;

    QModelIndex index = ui->tableViewSearch->selectionModel()->selectedIndexes().at(0);
    int songid = index.sibling(index.row(),0).data().toInt();
    if (ui->radioButtonNewSinger->isChecked())
    {
        if (ui->lineEditSingerName->text() == "")
            return;
        else if (rotModel->singerExists(ui->lineEditSingerName->text()))
            return;
        else
        {
            rotModel->singerAdd(ui->lineEditSingerName->text());
            if (rotModel->currentSinger() != -1)
            {
                int curSingerPos = rotModel->getSingerPosition(rotModel->currentSinger());
                if (ui->comboBoxAddPosition->currentIndex() == 0)
                    rotModel->singerMove(rotModel->rowCount() -1, curSingerPos + 1);
                else if ((ui->comboBoxAddPosition->currentIndex() == 1) && (curSingerPos != 0))
                    rotModel->singerMove(rotModel->rowCount() -1, curSingerPos);
            }
            emit addRequestSong(songid, rotModel->getSingerId(ui->lineEditSingerName->text()));
        }
    }
    else if (ui->radioButtonExistingSinger->isChecked())
    {
        emit addRequestSong(songid, rotModel->getSingerId(ui->comboBoxSingers->currentText()));
    }
    if (settings->requestRemoveOnRotAdd())
    {

        songbookApi->removeRequest(curRequestId);
        ui->tableViewRequests->selectionModel()->clearSelection();
        ui->lineEditSearch->clear();
        ui->lineEditSingerName->clear();
        ui->comboBoxSingers->clear();
        ui->radioButtonExistingSinger->setChecked(true);
    }
}

void DlgRequests::on_tableViewSearch_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->tableViewSearch->indexAt(pos);
    if (index.isValid())
    {
        rtClickFile = index.sibling(index.row(),5).data().toString();
        QMenu contextMenu(this);
        contextMenu.addAction(tr("Preview"), this, SLOT(previewCdg()));
        contextMenu.exec(QCursor::pos());
    }
}

void DlgRequests::updateReceived(QTime updateTime)
{
    ui->labelLastUpdate->setText(updateTime.toString("hh:mm:ss AP"));
}

void DlgRequests::on_buttonRefresh_clicked()
{
    songbookApi->refreshRequests();
    songbookApi->refreshVenues();
}

void DlgRequests::sslError()
{
    QMessageBox::warning(this, tr("SSL Handshake Error"), tr("An error was encountered while establishing a secure connection to the requests server.  This is usually caused by an invalid or self-signed cert on the server.  You can set the requests client to ignore SSL errors in the network settings dialog."));
}

void DlgRequests::delayError(int seconds)
{
    QMessageBox::warning(this, tr("Possible Connectivity Issue"), tr("It has been ") + QString::number(seconds) + tr(" seconds since we last received a response from the requests server.  You may be missing new submitted requests.  Please ensure that your network connection is up and working."));
}

void DlgRequests::on_checkBoxAccepting_clicked(bool checked)
{
    songbookApi->setAccepting(checked);
}

void DlgRequests::venuesChanged(OkjsVenues venues)
{
    int venue = settings->requestServerVenue();
    ui->comboBoxVenue->clear();
    int selItem = 0;
    for (int i=0; i < venues.size(); i++)
    {
        ui->comboBoxVenue->addItem(venues.at(i).name, venues.at(i).venueId);
        if (venues.at(i).venueId == venue)
        {
            selItem = i;
        }
    }
    ui->comboBoxVenue->setCurrentIndex(selItem);
    settings->setRequestServerVenue(ui->comboBoxVenue->itemData(selItem).toInt());
    ui->checkBoxAccepting->setChecked(songbookApi->getAccepting());
}

void DlgRequests::on_pushButtonUpdateDb_clicked()
{
    QMessageBox msgBox;
    msgBox.setText(tr("Are you sure?"));
    msgBox.setInformativeText(tr("This operation can take serveral minutes depending on the size of your song database and the speed of your internet connection."));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes)
    {
        QProgressDialog *progressDialog = new QProgressDialog(this);
        progressDialog->setCancelButton(0);
        progressDialog->setMinimum(0);
        progressDialog->setMaximum(20);
        progressDialog->setValue(0);
        progressDialog->setLabelText(tr("Updating request server song database"));
        progressDialog->show();
        QApplication::processEvents();
        connect(songbookApi, SIGNAL(remoteSongDbUpdateNumDocs(int)), progressDialog, SLOT(setMaximum(int)));
        connect(songbookApi, SIGNAL(remoteSongDbUpdateProgress(int)), progressDialog, SLOT(setValue(int)));
        //    progressDialog->show();
        songbookApi->updateSongDb();
        progressDialog->close();
    }
}

void DlgRequests::on_comboBoxVenue_activated(int index)
{
    int venue = ui->comboBoxVenue->itemData(index).toInt();
    settings->setRequestServerVenue(venue);
    songbookApi->refreshRequests();
    ui->checkBoxAccepting->setChecked(songbookApi->getAccepting());
    qWarning() << "Set venue_id to " << venue;
    qWarning() << "Settings now reporting venue as " << settings->requestServerVenue();
}

void DlgRequests::previewCdg()
{
    DlgCdgPreview *cdgPreviewDialog = new DlgCdgPreview(this);
    cdgPreviewDialog->setAttribute(Qt::WA_DeleteOnClose);
    cdgPreviewDialog->setSourceFile(rtClickFile);
    cdgPreviewDialog->preview();
}

void DlgRequests::on_lineEditSearch_textChanged(const QString &arg1)
{
    static QString lastVal;
    if (arg1.trimmed() != lastVal)
    {
        dbModel->search(arg1);
        lastVal = arg1.trimmed();
    }
}

void DlgRequests::lineEditSearchEscapePressed()
{
    QModelIndex index;
    index = ui->tableViewRequests->selectionModel()->selectedIndexes().at(0);
    QString filterStr = index.sibling(index.row(),2).data().toString() + " " + index.sibling(index.row(),3).data().toString();
    dbModel->search(filterStr);
    ui->lineEditSearch->setText(filterStr);
}

void DlgRequests::autoSizeViews()
{
    int fH = QFontMetrics(settings->applicationFont()).height();
    int durationColSize = QFontMetrics(settings->applicationFont()).width(tr(" Duration "));
    int songidColSize = QFontMetrics(settings->applicationFont()).width(" AA0000000-0000 ");
    int remainingSpace = ui->tableViewSearch->width() - durationColSize - songidColSize - 12;
    int artistColSize = (remainingSpace / 2) - 12;
    int titleColSize = (remainingSpace / 2);
    ui->tableViewSearch->horizontalHeader()->resizeSection(1, artistColSize);
    ui->tableViewSearch->horizontalHeader()->resizeSection(2, titleColSize);
    ui->tableViewSearch->horizontalHeader()->resizeSection(4, durationColSize);
    ui->tableViewSearch->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableViewSearch->horizontalHeader()->resizeSection(3, songidColSize);

    int tsWidth = QFontMetrics(settings->applicationFont()).width(" 00/00/00 00:00 xx ");
    qWarning() << "tsWidth = " << tsWidth;
    int delwidth = fH * 2;
    int singerColSize = QFontMetrics(settings->applicationFont()).width("_Isaac_Lightburn_");
    qWarning() << "singerColSize = " << singerColSize;
    remainingSpace = ui->tableViewRequests->width() - tsWidth - delwidth - singerColSize - 6;
    artistColSize = remainingSpace / 2;
    titleColSize = remainingSpace / 2;
    ui->tableViewRequests->horizontalHeader()->resizeSection(0, singerColSize);
    ui->tableViewRequests->horizontalHeader()->resizeSection(1, artistColSize);
    ui->tableViewRequests->horizontalHeader()->resizeSection(2, titleColSize);
    ui->tableViewRequests->horizontalHeader()->resizeSection(3, tsWidth);
    ui->tableViewRequests->horizontalHeader()->resizeSection(4, delwidth);
    ui->tableViewRequests->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
}


void DlgRequests::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    autoSizeViews();
}


void DlgRequests::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    autoSizeViews();
}
