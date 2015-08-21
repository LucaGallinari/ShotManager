/*

	ISSUE TODO:
		- ...
	TODO:
		- popup while filling the buffer
		- progress bar while processing
		- UI restyle

*/

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	qDebug() << "Starting up";

	//TODO: set this dynamically
	int numPrev = 10;

	autoSort = true;
	_currMarkerRow = -1;
	_currMarkerCol = -1;

	_bmng = new ImagesBuffer(numPrev);
	_prevWidg = new PreviewsWidget(0, this, _bmng);
	_playerWidg = new PlayerWidget(0, this, _bmng);
	_markersWidg = new MarkersWidget(0, this, ui->markersTableWidget);

	ui->previewsLayout->addWidget(_prevWidg);

	sliderPageStep = ui->videoSlider->pageStep();
	sliderMaxVal = ui->videoSlider->maximum() + 1;
	ui->labelVideoFrame->setMinimumSize(1,1);

	ui->progressLbl->setText("Started");

	initializeIcons();
}

MainWindow::~MainWindow()
{
	delete ui;
}


/*! \brief Open a dialog with video infos
*
*	Open a dialog with video infos
*/
void MainWindow::showInfo() 
{
	QDialog *infoDialog = new QDialog(this);

	QGridLayout *base = new QGridLayout();

	base->addWidget(new QLabel("Video path:"), 0, 0);
	base->addWidget(new QLabel(_bmng->getPath()), 0, 1);

	base->addWidget(new QLabel("Type:"), 1, 0);
	base->addWidget(new QLabel(_bmng->getType()), 1, 1);

	base->addWidget(new QLabel("Duration:"), 2, 0);
	base->addWidget(new QLabel(_bmng->getDuration()), 2, 1);

	base->addWidget(new QLabel("Number of frames:"), 3, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getNumFrames())), 3, 1);

	base->addWidget(new QLabel("Time base:"), 4, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getTimeBase())), 4, 1);

	base->addWidget(new QLabel("Frame rate:"), 5, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getFrameRate())), 5, 1);

	base->addWidget(new QLabel("Frame ms (theorycal):"), 6, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getFrameMsec())), 6, 1);

	base->addWidget(new QLabel("Frame ms (real):"), 7, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getFrameMsecReal())), 7, 1);

	base->addWidget(new QLabel("Frame width:"), 8, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getFrameWidth())), 8, 1);

	base->addWidget(new QLabel("Frame height:"), 9, 0);
	base->addWidget(new QLabel(QString::number(_bmng->getFrameHeight())), 9, 1);

	base->addWidget(new QLabel("Bitrate:"), 10, 0);
	base->addWidget(new QLabel(_bmng->getBitrate()), 10, 1);

	base->addWidget(new QLabel("Programs:"), 11, 0);
	base->addWidget(new QLabel(_bmng->getProgramsString()), 11, 1);

	base->addWidget(new QLabel("Metadata:"), 12, 0);
	base->addWidget(new QLabel(_bmng->getMetadataString()), 12, 1);

	infoDialog->setLayout(base);

	infoDialog->show();

}



/**********************************************
******************** EVENTS ******************
***********************************************/

void MainWindow::changeEvent(QEvent *e)
{
	QMainWindow::changeEvent(e);
	switch (e->type()) {
		case QEvent::LanguageChange:
			ui->retranslateUi(this);
			break;
		default:
			break;
	}
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	_playerWidg->reloadFrame();
	_prevWidg->reloadLayout();
}

/*! \brief initialize player icons.
*
*	Initialize player icons.
*/
void MainWindow::initializeIcons() {
	playIcon  = QPixmap(":/icons/playB.png");
	pauseIcon = QPixmap(":/icons/pauseB.png");
}


/******************
****** SLOTS ******
*******************/

void MainWindow::updateSlider()
{
	ui->videoSlider->setValue(round(_playerWidg->currentTimePercentage() * sliderMaxVal));
}

void MainWindow::changePlayPause(bool playState)
{
	if (playState) {
		ui->playPauseBtn->setToolTip("Pause");
		ui->playPauseBtn->setIcon(QIcon(pauseIcon));
	} else {
		ui->playPauseBtn->setToolTip("Play");
		ui->playPauseBtn->setIcon(QIcon(playIcon));
	}
}


/*! \brief Update the frame image.
*
*	Draw the new frame image in the proper label.
*	
*	@param p qpixmap image
*/
void MainWindow::updateFrame(QPixmap p)
{
	// set a scaled pixmap to a w x h window keeping its aspect ratio
	ui->labelVideoFrame->setPixmap(
		p.scaled(
			ui->labelVideoFrame->width(),
			ui->labelVideoFrame->height(),
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation
		)
	);
}

/*! \brief Update the time box.
*
*	This functions updates the timebox with the proper actual time of the video.
*	
*	@param time actual msec of the video
*/
void MainWindow::updateTime(qint64 time)
{
	int ms = time % 1000;
	int s  = (time / 1000) % 60;
	int m  = (time / (1000*60)) % 60;
	int h  = (time / (1000*60*60)) % 24;
	ui->labelVideoInfo->setText(
		QString::number(h)+":"+
		QString("%1").arg(m, 2, 10, QChar('0'))+":"+
		QString("%1").arg(s, 2, 10, QChar('0'))+" "+
		QString("%1").arg(ms, 3, 10, QChar('0'))
	);
}



/*! \brief Update progress bar text.
*
*	This functions updates the progress bar text.
*	
*	@param m message to set
*/
void MainWindow::updateProgressText(QString m)
{
	ui->progressLbl->setText(m);
	ui->progressLbl->repaint();
}

/*! \brief .
*
*	
*/
void MainWindow::endOfStream()
{
	_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
}

/*! \brief .
*
*
*/
void MainWindow::jumpToFrame(const qint64 num)
{
	if (!_playerWidg->isVideoLoaded() || _playerWidg->isVideoPlaying())
		return;

	_playerWidg->seekToFrame(num);
	updateSlider();
	_prevWidg->reloadAndDrawPreviews(num);
}

/*! \brief .
*
*
*/
void MainWindow::changeStartEndBtn(const bool markerStarted)
{
	if (markerStarted) {
		ui->startMarkerBtn->setToolTip("End current marker and start a new one");
		ui->startMarkerBtn->setText("} {");
	}
	else {
		ui->startMarkerBtn->setToolTip("Insert a new marker");
		ui->startMarkerBtn->setText("{");
	}
}

/**********************************************
******************** ACTIONS ******************
***********************************************/
void MainWindow::on_actionQuit_triggered()
{
	close();
}

void MainWindow::on_actionLoad_video_triggered()
{
	// Prompt a video to load
	QString fileName = QFileDialog::getOpenFileName(this, "Load Video",QString(),"Video (*.avi *.asf *.mpg *.wmv *.mkv)");
	if (!fileName.isNull())
	{
		ui->videoSlider->setValue(0);
		_playerWidg->loadVideo(fileName);
		_prevWidg->setupPreviews();
	}
}

void MainWindow::on_actionCompare_triggered()
{
	_dial = new CompareMarkersDialog(_markersWidg->getInputFile(), this);
	_dial->show();
}


/**********************************************
******************* BUTTONS *******************
***********************************************/

/***  PLAYER  ***/
void MainWindow::on_nextFrameBtn_clicked()
{
	if (_playerWidg->isVideoPlaying())
		return;
	_playerWidg->nextFrame();
	updateSlider();
	_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
}

void MainWindow::on_prevFrameBtn_clicked()
{
	if (_playerWidg->isVideoPlaying())
		return;
	_playerWidg->prevFrame();
	updateSlider();
	_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
}

void MainWindow::on_seekFrameBtn_clicked()
{
	if (!_playerWidg->isVideoLoaded() || _playerWidg->isVideoPlaying())
		return;

	// Ask for the frame number
	bool ok;
	int frameNum = QInputDialog::getInt(
		this,
		tr("Seek to frame"), tr("Frame number:"),
		0, 0, _playerWidg->getNumFrames() - 1, 1,
		&ok
	);

	if (!ok) {
		QMessageBox::critical(this,"Error","Invalid frame number");
		return;
	}

	updateProgressText("Seeking to desired frame number..");
	_playerWidg->seekToFrame(frameNum);
	updateSlider();
	_prevWidg->reloadAndDrawPreviews(frameNum);
	updateProgressText("");
}

void MainWindow::on_playPauseBtn_clicked()
{
	_playerWidg->playPause();
	if (!_playerWidg->isVideoPlaying()) {
		_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
	}
}

void MainWindow::on_stopBtn_clicked()
{
	_playerWidg->stopVideo(true);
	_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
}


/***  SLIDER  ***/
void MainWindow::on_videoSlider_actionTriggered(int action)
{
	// if single step page actions
	if (action==3 || action==4) {
		if (!_playerWidg->isVideoLoaded()) {
			ui->videoSlider->setValue(0);
			return;
		}
		// the slider has not already been updated so i have to predict its future value
		int val = ui->videoSlider->value() + (action==3 ? 1 : -1) * sliderPageStep;
		if (val >= sliderMaxVal)
			val = sliderMaxVal - 1;
		updateProgressText("Seeking to desired position..");
		_playerWidg->seekToTimePercentage(val / (double)sliderMaxVal);
		_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
		updateProgressText("");
	}
}

void MainWindow::on_videoSlider_sliderReleased()
{
	if (!_playerWidg->isVideoLoaded()) {
		ui->videoSlider->setValue(0);
		return;
	}
	updateProgressText("Seeking to desired position..");
	_playerWidg->seekToTimePercentage(ui->videoSlider->value() / (double)sliderMaxVal);
	_prevWidg->reloadAndDrawPreviews(_playerWidg->currentFrameNumber());
	updateProgressText("");
}



/***  SPLITTERS  ***/
void MainWindow::on_splitter_splitterMoved(int pos, int index)
{
	if (!_playerWidg->isVideoLoaded()) {
		ui->videoSlider->setValue(0);
		return;
	}
	_playerWidg->reloadFrame();
	_prevWidg->reloadLayout();
}

void MainWindow::on_splitter_2_splitterMoved(int pos, int index)
{
	if (!_playerWidg->isVideoLoaded()) {
		ui->videoSlider->setValue(0);
		return;
	}
	_playerWidg->reloadFrame();
}



/***  MARKERS  ***/
void MainWindow::on_startMarkerBtn_clicked()
{
	qint64 frameNum = _playerWidg->currentFrameNumber();
	if (_markersWidg->_markerStarted) {// end + start
		_markersWidg->endAndStartMarker(frameNum, frameNum + 1);
	}
	else {// start
		_markersWidg->endAndStartMarker(-1, frameNum);
	}
}

void MainWindow::on_endMarkerBtn_clicked()
{
	_markersWidg->endAndStartMarker(_playerWidg->currentFrameNumber(), -1);
}


void MainWindow::on_markersSaveBtn_clicked()
{
	QString path = _markersWidg->saveFile();
	if (path != "") {
		ui->markersFileText->setText(path);
	}
	else {
		QMessageBox::critical(this, "Error", "Error while saving! Check folders and files permissions..");
		return;
	}
}

void MainWindow::on_markersLoadBtn_clicked()
{
	_currMarkerRow = -1;
	QString path = _markersWidg->loadFile();
	if (path != "") {
		ui->markersFileText->setText(path);
	}
	else {
		QMessageBox::critical(this, "Error", "Error while loading! Check folders and files permissions..");
		return;
	}
}

void MainWindow::on_markersNewBtn_clicked()
{
	_currMarkerRow = -1;
	if (_markersWidg->newFile()) {
		ui->markersFileText->setText("");
	}
	else {
		QMessageBox::critical(this, "Error", "Error while clearing the list! Try reloading..");
		return;
	}
}

void MainWindow::on_markersTableWidget_customContextMenuRequested(const QPoint &pos)
{
	_markersWidg->showContextMenu(ui->markersTableWidget->mapToGlobal(pos));
}

void MainWindow::on_markersTableWidget_cellChanged(int row, int column)
{
	/* Don't want to call this all the time that the value of a cell is changed
	 * but only when it's changed by the user. We can do this by tracking the last
	 * selected row and exec following actions only if the selected row changed.
	 * It's not perfect but it works well.
	*/
	if (row == _currMarkerRow) {
		bool ok;
		int val = ui->markersTableWidget->item(row, column)->text().toUInt(&ok);
		if (!ok) {
			QMessageBox::critical(this,"Error","Expected a number");
			ui->markersTableWidget->item(row, column)->setText("0");
			return;
		}
		_markersWidg->markerChanged(row, column, val);
	}
}

void MainWindow::on_markersTableWidget_itemSelectionChanged()
{
	_currMarkerRow = ui->markersTableWidget->currentRow();
}

void MainWindow::on_infoBtn_clicked()
{
	showInfo();
}
