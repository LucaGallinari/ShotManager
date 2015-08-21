/*
   QTFFmpegWrapper Demo
   Copyright (C) 2009,2010:
		 Daniel Roggen, droggen@gmail.com

   All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, 
	are permitted provided that the following conditions are met:

   1.	Redistributions of source code must retain the above copyright notice, this 
		list of conditions and the following disclaimer.
   2.	Redistributions in binary form must reproduce the above copyright notice, 
		this list of conditions and the following disclaimer in the documentation 
		and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED 
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD 
	PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "QVideoDecoder.h"

#include <stdint.h>


/*! \brief TODO
*
*   Constructor
*/
QVideoDecoder::QVideoDecoder()
{
	InitVars();
	initCodec();
}
/*! \brief TODO
*
*   Constructor
*/
QVideoDecoder::QVideoDecoder(const QString file)
{
	InitVars();
	initCodec();

	ok = openFile(file.toStdString().c_str());
}

/*! \brief Destroyer
*
*   Destroyer
*/
QVideoDecoder::~QVideoDecoder()
{
	close();
	InitVars();
}

/*! \brief Codec initialization
*
*   Codec initialization
*/
void QVideoDecoder::initCodec()
{
	ffmpeg::avcodec_register_all();
	ffmpeg::av_register_all();

	qDebug() << "License: " << ffmpeg::avformat_license();
	qDebug() << "AVCodec version: " << ffmpeg::avformat_version();
	qDebug() << "AVFormat configuration: " << ffmpeg::avformat_configuration();
}

/*! \brief Variables initialization
*
*   Variables initialization
*/
void QVideoDecoder::InitVars()
{
	ok=false;
	pFormatCtx=0;
	pCodecCtx=0;
	pCodec=0;
	pFrame=0;
	pFrameRGB=0;
	buffer=0;
	img_convert_ctx=0;
	millisecondbase = { 1, 1000 };
}

/*! \brief Close the file and reset all variables
*
*   Close the file and reset all variables
*/
void QVideoDecoder::close()
{
	if(!ok)
		return;

	// Free the RGB image
	if(buffer)
		delete [] buffer;

	// Free the YUV frame
	if(pFrame)
		av_free(pFrame);

	// Free the RGB frame
	if(pFrameRGB)
		av_free(pFrameRGB);

	// Close the codec
	if(pCodecCtx)
		avcodec_close(pCodecCtx);

	// Close the video file
	if(pFormatCtx)
		avformat_close_input(&pFormatCtx);
}


/*! \brief Open a file and setup all variables
*
*   Open a file and setup all variables
*	@param filename path of the file to open
*	@ret success or not
*/
bool QVideoDecoder::openFile(const QString filename)
{
	// Close last video..
	close();

	LastLastFrameTime = INT_MIN;       // Last last must be small to handle the seek well
	LastFrameTime = 0;
	LastLastFrameNumber = INT_MIN;
	LastFrameNumber = 0;
	LastIdealFrameNumber = 0;
	LastFrameOk = false;

	// Open video file
	if(avformat_open_input(&pFormatCtx, filename.toStdString().c_str(), NULL, NULL)!=0)
		return false; // Couldn't open file

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return false; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, filename.toStdString().c_str(), false);

	// Find the first video stream
	videoStream=-1;
	for(unsigned i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==ffmpeg::AVMEDIA_TYPE_VIDEO)
		{
			videoStream=i;
			break;
		}
	if(videoStream==-1)
		return false; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
		return false; // Codec not found

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return false; // Could not open codec

	// Hack to correct wrong frame rates that seem to be generated by some
	// codecs
	if(pCodecCtx->time_base.num>1000 && pCodecCtx->time_base.den==1)
		pCodecCtx->time_base.den=1000;

	// Allocate video frame
	pFrame=ffmpeg::avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameRGB=ffmpeg::avcodec_alloc_frame();
	if(pFrameRGB==NULL)
		return false;

	// Determine required buffer size and allocate buffer
	numBytes=ffmpeg::avpicture_get_size(ffmpeg::PIX_FMT_RGB24, pCodecCtx->width,pCodecCtx->height);
	buffer=new uint8_t[numBytes];

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	avpicture_fill((ffmpeg::AVPicture *)pFrameRGB, buffer, ffmpeg::PIX_FMT_RGB24,
		pCodecCtx->width, pCodecCtx->height);

	// Set variables
	path			= filename;
	type			= QString(pFormatCtx->iformat->name);
	duration		= pFormatCtx->duration;
	baseFrameRate	= av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);
	frameMSec		= 1000 / baseFrameRate;
	if (type == "matroska,webm") {
		frameMSecReal =
			(double)(pFormatCtx->streams[videoStream]->time_base.den /
			pFormatCtx->streams[videoStream]->time_base.num) /
			(double)(pFormatCtx->streams[videoStream]->codec->time_base.den /
			(double)pFormatCtx->streams[videoStream]->codec->time_base.num)*
			pCodecCtx->ticks_per_frame;
	}
	else {
		frameMSecReal = frameMSec;
	}
	baseFRateReal	= 1000 / (double) frameMSecReal;
	timeBaseRat		= pFormatCtx->streams[videoStream]->time_base;
	timeBase		= av_q2d(timeBaseRat);
	w				= pCodecCtx->width;
	h				= pCodecCtx->height;
	
	// if (type=="mpeg") {
	firstDts = pFormatCtx->streams[videoStream]->first_dts;
	if (firstDts == AV_NOPTS_VALUE) 
		firstDts = 0;
	startTs = pFormatCtx->streams[videoStream]->start_time;
	if (startTs == AV_NOPTS_VALUE)
		firstDts = 0;
	//}

	ok = true;

	dumpFormat(0);

	return true;
}


/*! \brief Seek the given frame and decode it
*
*   Decodes the video stream until the first frame with number larger or equal 
*	than 'idealFrameNumber' is found.
*	TODO: if we already passed the wanted frame number?
*	@param idealFrameNumber desired frame number
*	@ret success or not
*/
bool QVideoDecoder::decodeSeekFrame (const qint64 idealFrameNumber)
{
	if (!ok)
		return false;

	qint64 f, t;
	bool done = false;

	// If the last decoded frame satisfies the time condition we return it
	if (
		idealFrameNumber != -1 &&
		(LastFrameOk == true && idealFrameNumber >= LastLastFrameNumber && idealFrameNumber <= LastFrameNumber)
	) {
		return true;
	}   

	while (!done) {

		// Read a frame
		if (av_read_frame(pFormatCtx, &packet)<0)
			return false;	// end of stream?            

		// Packet of the video stream?
		if (packet.stream_index==videoStream) {

			int frameFinished;
			avcodec_decode_video2(pCodecCtx,pFrame,&frameFinished,&packet);

			// Frame is completely decoded?
			if (frameFinished) {

				// Calculate real frame number and time based on the format
				if (type == "mpeg" || type == "asf") {
					f = (long)((packet.dts - startTs) * (baseFrameRate*timeBase) + 0.5);
					t = ffmpeg::av_rescale_q(packet.dts - startTs, timeBaseRat, millisecondbase);
				}
				else if (type == "matroska,webm") {
					t = av_frame_get_best_effort_timestamp(pFrame);
					f = round(t / frameMSec);
				}
				else { // avi
					f = packet.dts;
					t = ffmpeg::av_rescale_q(packet.dts, timeBaseRat, millisecondbase);
				}

				qDebug() << "real num: " << f;
				qDebug() << "real time: " << t;

				if (LastFrameOk) {
					// If we decoded 2 frames in a row, the last times are okay
					LastLastFrameTime = LastFrameTime;
					LastLastFrameNumber = LastFrameNumber;
					LastFrameTime = t;
					LastFrameNumber = f;
				}
				else {
					LastFrameOk = true;
					LastLastFrameTime = LastFrameTime = t;
					LastLastFrameNumber = LastFrameNumber = f;
				}

				// this is the desired frame or at least one just after it
				if (idealFrameNumber == -1 || LastFrameNumber >= idealFrameNumber)
				{
					qDebug() << "fake num: " << idealFrameNumber;
					qDebug() << "dts: " << packet.dts << endl;
					
					// Convert and save the frame
					img_convert_ctx = ffmpeg::sws_getCachedContext(
						img_convert_ctx, w, h, 
						pCodecCtx->pix_fmt, w, h, 
						ffmpeg::PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL
					);

					if (img_convert_ctx == NULL) {
						qDebug() << "Cannot initialize the conversion context!";
						return false;
					}
					ffmpeg::sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

					LastFrame = QImage(w, h, QImage::Format_RGB888);

					for (int y=0; y < h; y++)
						memcpy(LastFrame.scanLine(y), pFrameRGB->data[0] + y * pFrameRGB->linesize[0], w * 3);

					LastFrameOk = true;
					done = true;
				} // frame of interes
			}  // frameFinished
		}  // stream_index==videoStream

		av_free_packet(&packet);
	}
	return done;
}

/*! \brief Seek the next frame
*
*   Seek the next frame.
*	@return success or not
*   @see seekFrame()
*   @see seekPrevFrame()
*/
bool QVideoDecoder::seekNextFrame()
{
	bool ret = decodeSeekFrame(LastIdealFrameNumber + 1);

	if (ret)
		++LastIdealFrameNumber;
	else
		LastFrameOk=false;
	return ret;
}

/*! \brief Seek the previous frame
*
*   Seek the previous frame.
*	@return success or not
*   @see seekFrame()
*   @see seekNextFrame()
*/
bool QVideoDecoder::seekPrevFrame()
{
	bool ret = seekFrame(LastIdealFrameNumber - 1);

	if (!ret)
		LastFrameOk = false;      
	return ret;
}

/*! \brief Seek the closest frame to the given time
*
*   Seek the closest frame to the given time.
*	@param tsms time in milliseconds
*	@return success or not
*   @see seekFrame()
*/
bool QVideoDecoder::seekMs(const qint64 tsms)
{
   if(!ok)
	  return false;
	return seekFrame((tsms <= 0 ? 0 : round(tsms / frameMSec)));
}

/*! \brief Seek the desired frame
*
	TODO: case when idelFrameNumber less than 0
*   Seek and retrieve desired frame by number.
*	@param idealFrameNumber number of the desired frame
*	@return success or not
*   @see seekToAndGetFrame()
*   @see seekMs()
*/
bool QVideoDecoder::seekFrame(const qint64 idealFrameNumber)
{
	if (!ok)
		return false;

	// no seek needed, go to next frame
	if (LastIdealFrameNumber + 1 == idealFrameNumber)
		return seekNextFrame();
	
	// have to seek?
	if ((LastFrameOk == false) || 
		((LastFrameOk == true) && (idealFrameNumber <= LastLastFrameNumber || idealFrameNumber > LastFrameNumber)))
	{
		if (!correctSeekToKeyFrame(idealFrameNumber))
			return false;

		avcodec_flush_buffers(pCodecCtx);
		LastIdealFrameNumber = idealFrameNumber;
		LastFrameOk = false;
	}

	// decode
	return decodeSeekFrame(idealFrameNumber);
}

/*! \brief Corrects the seeking operation
*
*   Corrects the seeking operation to a "key frame" because this varies from 
*	format and format. This is based on a prediction so we have to add a margin
*	of error that can allow us to ..TODO
*	@param idealFrameNumber number of the desired frame
*	@return success or not
*   @see seekFrame()
*/
bool QVideoDecoder::correctSeekToKeyFrame(const qint64 idealFrameNumber)
{
	qint64 desiredDts;
	qint64 startDts = INT64_MIN;
	int flag;

	if (type == "mpeg") { // .mpg
		// ffmpeg bug?: with H.264 avformat_seek_file often seeks not in a keyframe, 
		// thus the following avcodec_decode_video2 iterations may go past desiredDts
		desiredDts = (idealFrameNumber - 0.5 - baseFrameRate) / (baseFrameRate*timeBase) + firstDts;
		if (desiredDts < firstDts)	desiredDts = firstDts;
		startDts = -0x7ffffffffffffff;
		flag = AVSEEK_FLAG_BACKWARD;
	}
	else if (type == "asf"){ // .asf (wmv)

		desiredDts = idealFrameNumber * frameMSec;
		flag = AVSEEK_FLAG_BACKWARD;
	}
	else if (type == "matroska,webm") { // .mkv
		// this prediction is not perfect but it gives a good start point close 
		// to the desired frame number
		qint64 targetDts = idealFrameNumber *
			(pFormatCtx->streams[videoStream]->time_base.den /
			pFormatCtx->streams[videoStream]->time_base.num) /
			(pFormatCtx->streams[videoStream]->codec->time_base.den /
			pFormatCtx->streams[videoStream]->codec->time_base.num)*
			pCodecCtx->ticks_per_frame;

		desiredDts = round(idealFrameNumber * frameMSec);

		av_seek_frame(pFormatCtx, videoStream, targetDts, AVSEEK_FLAG_FRAME);
		avcodec_flush_buffers(pCodecCtx);
		
		bool done = false;
		bool firstEOF = true;
		qint64 t;

		while (!done) {

			// Read a frame
			if (av_read_frame(pFormatCtx, &packet) < 0) {
				// first end of stream? try to seek backward
				if (firstEOF) {
					firstEOF = false;

					targetDts -= 3000; // TODO: a better value?
					if (targetDts < 0)
						targetDts = 0;
					av_seek_frame(pFormatCtx, videoStream, targetDts, AVSEEK_FLAG_BACKWARD);
					avcodec_flush_buffers(pCodecCtx);

				}
				else { // another end of stream? frame not found
					return false;
				}

			} else {

				if (packet.stream_index == videoStream) {

					int frameFinished;
					avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

					if (frameFinished) {
						t = av_frame_get_best_effort_timestamp(pFrame);

						// if i am after the desired frame, have to reseek
						if (t > desiredDts) {
							qDebug() << "Sono oltre, vorrei " << targetDts << " ma sono a " << t;
							qDebug() << "Vado indietro di 3s da: " << targetDts;
							targetDts -= 3000; // TODO: a better value?
							if (targetDts < 0)
								targetDts = 0;
							av_seek_frame(pFormatCtx, videoStream, targetDts, AVSEEK_FLAG_BACKWARD);
							avcodec_flush_buffers(pCodecCtx);
						}
						else { // i am before, seek done
							done = true;
							qDebug() << "Ho trovato il seek giusto: " << targetDts;
							av_seek_frame(pFormatCtx, videoStream, targetDts, AVSEEK_FLAG_BACKWARD);
							avcodec_flush_buffers(pCodecCtx);
						}
					}
				}
			}

			av_free_packet(&packet);
		}
		return true;
	}
	else { // .avi, .wmv
		desiredDts = idealFrameNumber;
		flag = AVSEEK_FLAG_FRAME;
		
	}
	
	if (ffmpeg::avformat_seek_file(pFormatCtx, videoStream, startDts, desiredDts, INT64_MAX, flag) < 0) {
		return false;
		// qDebug() << "!!!SEEK ERROR!!!";
	}
	return true;
}

/*! \brief Seek and retrieve desired frame
*
*   Seek and retrieve desired frame by number.
*	@param idealFrameNumber number of the desired frame
*	@param img where it stores the frame
*	@param frameNum where it stores the frame number
*	@param frameTime where it stores the frame time
*	@return success or not
*/
bool QVideoDecoder::seekToAndGetFrame(const qint64 idealFrameNumber, QImage&img, qint64 *frameNum, qint64 *frameTime)
{
	if (!seekFrame(idealFrameNumber))
		return false;
	return getFrame(img, frameNum, frameTime);
}



/***************************************
*********        GETTERS      *********
***************************************/

/*! \brief A file is loaded
*
*   A file is loaded
*	@return video loaded successfully
*/
bool QVideoDecoder::isOk()
{
	return ok;
}

/*! \brief Get last loaded frame
*
*   Get last loaded frame
*	@param img where it stores the frame
*	@param frameNum where it stores the frame number
*	@param frameTime where it stores the frame time
*	@return last frame was valid or not
*/
bool QVideoDecoder::getFrame(QImage &img, qint64 *frameNum, qint64 *frameTime)
{
	img = LastFrame;

	if (frameNum)
		*frameNum = LastFrameNumber;
	if (frameTime)
		*frameTime = LastFrameTime;

	return LastFrameOk;
}

/*! \brief Get last loaded frame "CODEC number"
*
*   Get last loaded frame "CODEC number", "CODEC number" because codecs and
*	formats use a different type of "number", some uses timestamps and some
*	normal integers.
*	@return last loaded frame number
*/
qint64 QVideoDecoder::getActualFrameNumber()
{
	if (!isOk())
		return -1;
	return LastFrameNumber;
}

/*! \brief Get last loaded frame "IDEAL number"
*
*   Get last loaded frame "IDEAL number", "IDEAL number" because codecs and
*	formats use a different type of "number", some uses timestamps and some
*	normal integers.
*	@return last loaded frame number
*/
qint64 QVideoDecoder::getIdealFrameNumber()
{
	if (!isOk())
		return -1;
	return LastIdealFrameNumber;
}

/*! \brief Get last loaded frame time milliseconds
*
*   Get last loaded frame time milliseconds.
*	@return last loaded frame time
*/
qint64 QVideoDecoder::getFrameTime()
{
	if (!isOk())
		return -1;
	return LastFrameTime;
}

/*! \brief Get frame number by time
*
*   Get the number of the closest frame to the given time
*	@return frame number
*/
qint64 QVideoDecoder::getNumFrameByTime(const qint64 tsms)
{
	if (!ok)
		return false;
	if (tsms <= 0)
		return 0;
	return round(tsms / frameMSec);
}

/*! \brief Get video duration in milliseconds
*
*   Get video duration in milliseconds.
*	@return video length in ms
*/
qint64 QVideoDecoder::getVideoLengthMs()
{
	if (!isOk())
		return -1;

	qint64 secs = pFormatCtx->duration / AV_TIME_BASE;
	qint64 us = pFormatCtx->duration % AV_TIME_BASE;
	return secs * 1000 + us / 1000;
}

/*! \brief Get number of frames (Not accurate with some formats)
*
*   Get number of frames based on video duration and frame rate. Some containers
*	save a wrong value for duration and so the number of frames could be not so
*	accurate.
*	@return number of frams
*/
qint64 QVideoDecoder::getNumFrames()
{
	return round(getVideoLengthMs() * (baseFrameRate / 1000.0));
}

/*! \brief Get video path
*
*	Retrieve video path
*/
QString QVideoDecoder::getPath() {
	return path;
}

/*! \brief Get video type
*
*	Retrieve video type
*/
QString QVideoDecoder::getType() {
	return type;
}

/*! \brief Get video time base as AVRational
*
*   Get video time base, it's the base time that the container uses
*	@return video time base
*/
ffmpeg::AVRational QVideoDecoder::getTimeBaseRat()
{
	return timeBaseRat;
}

/*! \brief Get video time base as double
*
*   Get video time base, it's the base time that the container uses
*	@return video time base
*/
double QVideoDecoder::getTimeBase()
{
	return timeBase;
}

/*! \brief Get video frame rate
*
*	Retrieve video frame rate
*/
double QVideoDecoder::getFrameRate() {
	return baseFrameRate;
}

/*! \brief Get video frame ms (theorycal)
*
*	Retrieve video frame ms (theorycal)
*/
double QVideoDecoder::getFrameMsec() {
	return frameMSec;
}

/*! \brief Get video frame ms (real)
*
*	Retrieve video frame ms (real). Actually only "matroska"'s files have a
*	different theorycal and real frame msec.
*/
double QVideoDecoder::getFrameMsecReal() {
	return frameMSecReal;
}

/*! \brief Get frame width
*
*	Get frame width
*/
int QVideoDecoder::getFrameWidth() {
	return w;
}

/*! \brief Get frame height
*
*	Get frame height
*/
int QVideoDecoder::getFrameHeight() {
	return h;
}

/*! \brief Get video bitrate
*
*	Get video bitrate
*/
QString QVideoDecoder::getBitrate() {
	return (pFormatCtx->bit_rate ? QString::number((int)(pFormatCtx->bit_rate / 1000)).append(" kb/s") : "N / A");
}

/*! \brief Get string of programs used to make the video
*
*	Get string of programs used to make the video
*/
QString QVideoDecoder::getProgramsString()
{
	QString s;
	// Programs
	if (pFormatCtx->nb_programs) {
		unsigned int j, total = 0;
		for (j = 0; j<pFormatCtx->nb_programs; j++) {
			ffmpeg::AVDictionaryEntry *name = av_dict_get(pFormatCtx->programs[j]->metadata, "name", NULL, 0);
			s.push_back(QString("%1 %2 \n").arg(pFormatCtx->programs[j]->id).arg(" ").arg(name ? name->value : ""));
			total += pFormatCtx->programs[j]->nb_stream_indexes;
		}
		if (total < pFormatCtx->nb_streams)
			s.push_back("None");
	}
	else{
		s.push_back("None");
	}
	return s;
}

/*! \brief Get string of metadatas presents in the video
*
*	Get string of metadatas presents in the video
*/
QString QVideoDecoder::getMetadataString()
{
	QString s;
	// Programs
	if (pFormatCtx->metadata) {
		ffmpeg::AVDictionaryEntry *tag = NULL;
		while ((tag = av_dict_get(pFormatCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			s.push_back(QString("%1: %2 \n").arg(tag->key).arg(tag->value));
		}
	}
	else{
		s.push_back("None");
	}
	return s;
}

/***************************************
*********        HELPERS      *********
***************************************/

/*! \brief Save a frame as PPM image
*
*   Save a frame as PPM image. Usefull for debugging.
*	@param pFrame the frame
*	@param width frame width
*	@param height frame height
*	@param iFrame frame number
*/
void QVideoDecoder::saveFramePPM(const ffmpeg::AVFrame *pFrame, const int width, const int height, const int iFrame)
{
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y<height; y++)
		fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

/*! \brief Output video's informations
*
*   Write video's information in the stdout. Usefull for debugging.
*	@param path file path
*	@param is_output writing or reading the file?
*/
void QVideoDecoder::dumpFormat(const int is_output) 
{
	qDebug() << (is_output ? "Output" : "Input");
	qDebug() << "File: " << path;
	qDebug() << "Stream: " << videoStream;
	qDebug() << "Type: " << (is_output ? pFormatCtx->oformat->name : type);
	qDebug() << "AV_TIME_BASE: " << AV_TIME_BASE;

	// General infos
	if (!is_output) {
		qDebug() << "Time Base: " << timeBase;
		qDebug() << "Start: " << startTs;
		qDebug() << "First Dts: " << firstDts;
		qDebug() << "FPS: " << baseFrameRate;
		qDebug() << "Frame ms: " << frameMSec;
		qDebug() << "special ms: " << frameMSecReal;
		qDebug() << "Frame w: " << w;
		qDebug() << "Frame h: " << h;
		qDebug() << "Number of frames: " << getNumFrames();
		qDebug() << "Duration: " << duration << " us";

		int hours, mins, secs, us;
		secs = duration / AV_TIME_BASE;
		us = duration % AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;
		qDebug() << "\t" << hours << "h " << mins << "m " << secs << "s " << (100 * us) / AV_TIME_BASE;
		qDebug() << "Bitrate: " << (pFormatCtx->bit_rate ? QString::number((int) (pFormatCtx->bit_rate / 1000)).append(" kb/s") : "N / A");
	}
	// Programs
	qDebug() << "Program: \n" << getProgramsString();
	// Metadata
	qDebug() << "Metadata: \n" << getMetadataString();
}

