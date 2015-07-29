
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QPainter>

#include "PlayerWidget.h"
#include "mainwindow.h"

/*! \brief PlayerWidget Constructor.
*
*   TODO
*/
PlayerWidget::PlayerWidget(QWidget *parent,QWidget *mainwin) : QWidget(parent)
{
    playState = false;

    playbackTimer = new QTimer(this);
    connect(playbackTimer, SIGNAL(timeout()), this, SLOT(updateFrame()));

    // S&S to mainwin
    connect(this, SIGNAL(newFrame(QPixmap)), mainwin, SLOT(updateFrame(QPixmap)));
    connect(this, SIGNAL(timeChanged(qint64)), mainwin, SLOT(updateTime(qint64)));
    connect(this, SIGNAL(playPauseToggle(bool)), mainwin, SLOT(changePlayPause(bool)));
    connect(this, SIGNAL(frameChanged()), mainwin, SLOT(updateSlider()));
}

PlayerWidget::~PlayerWidget()
{}

/***************** PRIVATE METHODS ************/

/*! \brief from QImage to QPixmap.
*
*   Convert a QImage to a QPixmap.
*	@param img as QImage
*	@param pixmap as QPixmap
*/
void PlayerWidget::image2Pixmap(QImage &img,QPixmap &pixmap)
{
   // Convert the QImage to a QPixmap for display
   pixmap = QPixmap(img.size());
   QPainter painter;
   painter.begin(&pixmap);
   painter.drawImage(0,0,img);
   painter.end();
}

/*! \brief display last loaded frame.
*
*   Retrieve and display last loaded frame.
*/
void PlayerWidget::displayFrame()
{
    if (!isVideoLoaded())
        return;

    // Decode a frame
    QImage img;
    int et,en;
    if (!decoder.getFrame(img,&en,&et)) {
        QMessageBox::critical(NULL,"Error","Error decoding the frame");
        return;
    }

    // Convert the QImage to a QPixmap and display it
    QPixmap p;
    image2Pixmap(img,p);

    qDebug() << "framEChange: " << currentFrameTime();
    emit newFrame(p);
    emit timeChanged(currentFrameTime());
}

/*! \brief load and display the next frame
*
*   Load and display the next frame and emit the signal frameChanged()
*/
void PlayerWidget::updateFrame()
{
    if (!nextFrame()) {
        playbackTimer->stop();
        return;
    }
    emit frameChanged();
}


/******************* PUBLIC METHODS ************/

/***************************************
 *********    FRAME ACTIONS    *********
 ***************************************/

/*! \brief reload and display last frame.
*
*   Reload last decoded frame, usefull when a resize event occurs.
*/
void PlayerWidget::reloadFrame()
{
    // this is needed otherwise a "Load video first"
    // error is thrown on app startup if used inside resizeEvent
    if (decoder.isOk()==true)
        displayFrame();
}

/*! \brief displays next frame.
*
*	Displays the next frame from the current player position.
*	The frame number is calculated and may not be correct.
*	@see prevFrame()
*/
bool PlayerWidget::nextFrame(){
    if (!decoder.seekNextFrame()) {
        QMessageBox::critical(NULL, "Error", "seekNextFrame failed");
        return false;
    }
    displayFrame();
    return true;
}

/*! \brief displays previous frame.
*
*	Displays the previous frame from the current player position.
*	The frame number is calculated and may not be correct.
*	@see nextFrame()
*/
bool PlayerWidget::prevFrame(){
    if (!decoder.seekPrevFrame()) {
        QMessageBox::critical(NULL, "Error", "seekPrevFrame failed");
        return false;
    }
    displayFrame();
    return true;
}

/*! \brief seek to frame number
*
*	Displays the given frame number
*   @param num frame number
*   @see seekToTime()
*/
void PlayerWidget::seekToFrame(qint64 num){
    // Check we've loaded a video successfully
    if (!isVideoLoaded())
        return;

    // Seek to the desired frame
    if (!decoder.seekFrame(num)) {
        QMessageBox::critical(NULL,"Error","Seek failed");
        return;
    }
    displayFrame();
}

/*! \brief seek to given time
*
*	Displays the frame near the given time
*   @param ms time in milliseconds
*   @see seekToFrame()
*/
void PlayerWidget::seekToTime(qint64 ms){
    // Check we've loaded a video successfully
    if (!isVideoLoaded())
        return;

    // Seek to the desired ms
    if(!decoder.seekMs(ms)) {
       QMessageBox::critical(NULL,"Error","Seek failed, invalid time");
       return;
    }
    displayFrame();
}

/*! \brief seek to given time percentage
*
*	Displays the frame near the given percentage of the entire video length
*   @param perc double value from 0 to 1
*/
void PlayerWidget::seekToTimePercentage(double perc){
    // Check we've loaded a video successfully
    if (!isVideoLoaded())
        return;

    int ms = videoLength * perc;
    qDebug() << "ms: " << ms << ", vlen: " << videoLength << ", perc: " << perc;
    // Seek to the desired ms
    if(!decoder.seekMs(ms)) {
       QMessageBox::critical(NULL,"Error","Seek failed, invalid time");
       return;
    }
    displayFrame();
}


/***************************************
 *********    VIDEO ACTIONS    *********
 ***************************************/

/*! \brief load a video.
*
*   Open and load a video by using ffmpeg's decoder.
*	@param fileName path to the video
*/
void PlayerWidget::loadVideo(QString fileName)
{
   decoder.openFile(fileName);
   numFrames = decoder.getNumFrames();
   videoLength = decoder.getVideoLengthMs();
   fps = decoder.getFps();
   frameMs = round(1000 / fps);

   if (decoder.isOk()==false) {
      QMessageBox::critical(NULL,"Error","Error loading the video");
      return;
   }

   // Seek to the first frame
   if (!decoder.seekFrame(0)) {
       QMessageBox::critical(NULL, "Error", "Seek the first frame failed");
       nextFrame(); // try to get the next frame instead of showing nothing
   }

   // Display a frame
   displayFrame();
}

/*! \brief play/pause the video.
*
*	Change the state of the player between play and pause.
*	If the player isn't playing starts, otherwhise if it's
*   paused it resumes the playback.
*/
void PlayerWidget::playPause()
{
    if (!isVideoLoaded())
        return;

    playState = !playState;

    if (playState) {
        playVideo();
    } else {
        pauseVideo();
    }

    emit playPauseToggle(playState);
}

/*! \brief play the video.
*
*	Play the video by starting the timer.
*   @see pauseVideo()
*   @see stopVideo()
*/
bool PlayerWidget::playVideo()
{
    if (!isVideoLoaded())
        return false;
    playbackTimer->start(frameMs);
    return true; //TODO: check if start() went ok
}

/*! \brief pause the video.
*
*	Pause the video by stopping the timer.
*   @see playVideo()
*   @see stopVideo()
*/
bool PlayerWidget::pauseVideo()
{
    if (!isVideoLoaded())
        return false;
    playbackTimer->stop();
    return true; //TODO: check if stop() went ok
}

/*! \brief stop the video.
*
*	Stop the video playback and seek to its start point.
*   @see playVideo()
*   @see pauseVideo()
*/
bool PlayerWidget::stopVideo()
{
    if (!isVideoLoaded())
        return false;
    // pause if playing
    if (playState) {
        playPause();
    }
    // reset
    seekToFrame(0);
    emit frameChanged();
    return true; //TODO: check if playPuase() and seekToFrame() went ok
}


/***************************************
 *********        GETTERS      *********
 ***************************************/

/*! \brief a video was loaded?
*
*   Checks if a video has been previously loaded.
*/
bool PlayerWidget::isVideoLoaded()
{
    if (decoder.isOk()==false) {
        QMessageBox::critical(NULL,"Error","Load a video first");
        return false;
    }
    return true;
}

/*! \brief the video is playing?
*
*   Checks if the video is playing.
*/
bool PlayerWidget::isVideoPlaying()
{
    return playState;
}

/*! \brief Get current frame number.
*
*	This functions is used to get the current frame number.
*	@return current frame number.
*   @see currentFrameTime()
*   @see previousFrameNumber()
*   @see nextFrameNumber()
*/
qint64 PlayerWidget::currentFrameNumber() {
    return decoder.getFrameNumber();
}

/*! \brief Get current frame time.
*
*	This functions is used to get the time of the current frame.
*	@return current frame time.
*   @see currentFrameNumber()
*/
qint64 PlayerWidget::currentFrameTime() {
    return decoder.getFrameTime();
}

/*! \brief Get the previous frame number.
*
*	This functions is used to get the previous frame number.
*	@return previous frame number.
*   @see currentFrameNumber()
*   @see nextFrameNumber()
*/
qint64 PlayerWidget::previousFrameNumber() {
    return currentFrameNumber() - 1;
}

/*! \brief Get the next frame number.
*
*	This functions is used to get the next frame number.
*	@return next frame number.
*   @see currentFrameNumber()
*   @see previousFrameNumber()
*/
qint64 PlayerWidget::nextFrameNumber() {
    return currentFrameNumber() + 1;
}

/*! \brief Get number of frames
*
*	Retrieve the number of frames
*/
qint64 PlayerWidget::getNumFrames() {
    return numFrames;
}

/*! \brief Get number of frames
*
*	Retrieve the number of frames
*/
qint64 PlayerWidget::getVideoLengthMs() {
    return videoLength;
}

/*! \brief Percentage of time passed (0 to 1)
*
*	Calculated the ratio (0 to 1) of the current frame time compared to the
*   total video length.
*	@return ratio percentage decimal (0 to 1)
*   @see currentFrameTime()
*/
double PlayerWidget::currentTimePercentage() {
    return decoder.getFrameTime() / (double) videoLength;
}
