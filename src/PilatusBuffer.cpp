/*
 This file is part of LImA, a Library for Image Acquisition

 Copyright (C) : 2009-2011
 European Synchrotron Radiation Facility
 BP 220, Grenoble 38043
 FRANCE

 This is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "Debug.h"
#include "PilatusInterface.h"
using namespace lima;
using namespace lima::Pilatus;

#ifndef DEFAULT_WATCH_PATH
#define DEFAULT_WATCH_PATH "/lima_data"
#endif

#ifndef DEFAULT_FILE_BASE
#define DEFAULT_FILE_BASE "tmp_img_"
#endif

#ifndef DEFAULT_FILE_EXTENTION
#define DEFAULT_FILE_EXTENTION ".edf"
#endif

#ifndef DEFAULT_FILE_PATERN
#define DEFAULT_FILE_PATERN "tmp_img_%.5d.edf"
#endif

const static int DECTRIS_EDF_OFFSET = 1024;

class BufferCtrlObj::_LocalReader : public BufferCtrlObj::Reader
{
  DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj::_LocalReader", "Pilatus");

public:
  _LocalReader(BufferCtrlObj &buffer,Camera &cam,
	       const BufferCtrlObj::Info& info) :
    m_buffer(buffer),
    m_cam(cam),
    m_info(info),
    m_quit(false),
    m_wait(true),
    m_thread_running(true),
    m_inotify_fd(-1),
    m_inotify_wd(-1),
    m_thread_id(-1)
  {
    DEB_CONSTRUCTOR();

    if(pipe(m_pipes))
      THROW_HW_ERROR(Error) << "Can't open pipe";

    m_inotify_fd = inotify_init();
    _startWatch();
    if(m_inotify_fd < 0)
      {      
	_clean();
	THROW_HW_ERROR(Error) << "Can't add watch directory" 
			      << DEB_VAR1(m_info);
      }
    if(pthread_create(&m_thread_id,NULL,_LocalReader::_run,this))
      {
	_clean();
	THROW_HW_ERROR(Error) << "Can't start watching thread";
      }
  }

  ~_LocalReader()
  {
    _clean();
  }
  
  void stop()
  {
    AutoMutex lock(m_cond.mutex());
    m_wait = true;
    write(m_pipes[1],"|",1);
  }
  bool isStopped() const
  {
    AutoMutex lock(m_cond.mutex());
    return m_wait;
  }

  void prepareAcq()
  {
    AutoMutex lock(m_cond.mutex());
    m_wait = true;
    m_image_nb_expected = -1;
    for(DatasPendingType::iterator i = m_datas_pending.begin();
	i != m_datas_pending.end();m_datas_pending.erase(i++))
      delete [] i->second;

    //Clean watch directory
    DIR* aWatchDir = opendir(m_info.m_watch_path.c_str());
    if(aWatchDir)
      {
	while(1)
	  {
	    char fullPath[1024];
	    struct dirent aDirentStruct,*result;
	    int status = readdir_r(aWatchDir,
				   &aDirentStruct,&result);
	    if(status || !result)
	      break;
	    snprintf(fullPath,sizeof(fullPath),
		     "%s/%s",m_info.m_watch_path.c_str(),
		     result->d_name);
	    unlink(fullPath);
	  }
	closedir(aWatchDir);
      }
  }
  void start()
  {
    AutoMutex lock(m_cond.mutex());
    m_cam.setImgpath(m_info.m_watch_path);
    m_cam.setFileName(m_info.m_file_patern);
    m_wait = false;
    m_cond.signal();
  }
  int getLastAcquiredFrame() const
  {
    AutoMutex lock(m_cond.mutex());
    return m_image_nb_expected;
  }
private:
  void _clean()
  {
    m_cond.acquire();
    m_quit = true;
    m_cond.signal();
    m_cond.release();

    close(m_pipes[1]);
    void *tReturn;
    if(m_thread_id >= 0)
      pthread_join(m_thread_id,&tReturn);
    close(m_pipes[0]);
    _stopWatch();
    if(m_inotify_fd >= 0)
      close(m_inotify_fd);
  }

  static void* _run(void* arg)
  {
    _LocalReader *reader = (_LocalReader*)arg;
    reader->run();
    return NULL;
  }

  void run()
  {
    struct pollfd fds[2];
    fds[0].fd = m_pipes[0];
    fds[0].events = POLLIN;
    fds[1].fd = m_inotify_fd;
    fds[1].events = POLLIN;
    FrameDim aFrameDim;
    m_buffer.getFrameDim(aFrameDim);

    while(1)
      {
	poll(fds,2,-1);
	if(fds[1].revents)
	  {
	    char buffer[EVENT_BUF_LEN];
	    int length = read(m_inotify_fd,
			      buffer,sizeof(buffer));
	    char *aPt = buffer;
	    while(length > 0)
	      {
		struct inotify_event *event = (struct inotify_event*)aPt;
		if(event->len)
		  {
		    const char* filename = event->name;
		    const char *extention = strrchr(filename,'.');

		    AutoMutex lock(m_cond.mutex());
		    if(m_wait)
		      _stopWatch();
		    else if(extention && !strcmp(extention,".edf") &&
			    !m_info.m_file_base.compare(0,m_info.m_file_base.size(),
							filename,m_info.m_file_base.size()))
		      {
			int imageNb = atoi(filename + 
					   m_info.m_file_base.size());
			int nextImageExpected = m_image_nb_expected + 1;
			std::string fullPath = m_info.m_watch_path + "/";
			fullPath += filename;
			char *aDataBuffer = NULL;
			try
			  {
			    aDataBuffer = m_buffer._readImage(fullPath.c_str());
			  }
			catch(Exception &e) // Error ????
			  {
			    m_wait = true;
			    m_cam.stopAcquisition();
			    break;
			  }
			if(aDataBuffer)
			  {
			    if(imageNb == nextImageExpected)
			      {
				HwFrameInfoType aNewFrameInfo(imageNb,aDataBuffer,&aFrameDim,
							      Timestamp(),0,
							      HwFrameInfoType::Shared);
				m_wait = !m_buffer.newFrameReady(aNewFrameInfo);
				m_image_nb_expected = imageNb;
				DatasPendingType::iterator i = m_datas_pending.begin();
				while(i != m_datas_pending.end())
				  {
				    nextImageExpected = m_image_nb_expected + 1;
				    if(i->first == nextImageExpected)
				      {
					HwFrameInfoType aFrameInfo(i->first,i->second,&aFrameDim,
								   Timestamp(),0,
								   HwFrameInfoType::Shared);
					m_wait = !m_buffer.newFrameReady(aFrameInfo);
					m_image_nb_expected = i->first;
					m_datas_pending.erase(i++);
				      }
				    else
				      break;
				  }
				
			      }
			    else
			      {
				m_datas_pending.insert(std::pair<int,char*>(imageNb,aDataBuffer));
			      }
			    if(m_info.m_keep_nb_images >= 0)
			      {
				int imageId2Remove = imageNb - m_info.m_keep_nb_images;
				if(imageId2Remove >= 0)
				  {
				    char aFileName[128];
				    snprintf(aFileName,sizeof(aFileName),
					     m_info.m_file_patern.c_str(),imageId2Remove);
				    char aFullPathBuffer[1024];
				    snprintf(aFullPathBuffer,sizeof(aFullPathBuffer),
					     "%s/%s",m_info.m_watch_path.c_str(),aFileName);
				    unlink(aFullPathBuffer);
				  }
			      }
			  }
		      }
		  }
		aPt += EVENT_SIZE + event->len;
		length -= EVENT_SIZE + event->len;
	      }
	  }
	else if(fds[0].revents)
	  {
	    char buffer[1024];
	    read(m_pipes[0],buffer,sizeof(buffer));
	    AutoMutex lock(m_cond.mutex());
	    if(m_quit) break;
	    while(m_wait && !m_quit)
	      {
		_stopWatch();
		m_thread_running = false;
		m_cond.signal();
		m_cond.wait();
		m_thread_running = true;
		_startWatch();
	      }
	  }
      }
  }
  inline void _stopWatch()
  {
    if(m_inotify_wd >= 0)
      {
	inotify_rm_watch(m_inotify_fd,m_inotify_wd);
	m_inotify_wd = -1;
      }
  }
  inline void _startWatch()
  {
    m_inotify_wd = inotify_add_watch(m_inotify_fd,
				     m_info.m_watch_path.c_str(),
				     IN_CLOSE_WRITE);
  }

  static const size_t EVENT_SIZE = sizeof(struct inotify_event);
  static const size_t EVENT_BUF_LEN = (1024 * (EVENT_SIZE + 16));

  typedef std::map<int,char*> DatasPendingType;

  BufferCtrlObj&		m_buffer;
  Camera& 			m_cam;
  const BufferCtrlObj::Info& 	m_info;
  mutable Cond			m_cond;
  //@brief variables used for synchronisation
  //@{
  int				m_pipes[2];
  volatile bool 		m_quit;
  volatile bool			m_wait;
  volatile bool			m_thread_running;
  //@}

  int				m_image_nb_expected;
  DatasPendingType		m_datas_pending;
  int 				m_inotify_fd;
  int 				m_inotify_wd;
  pthread_t			m_thread_id;
};
/*******************************************************************
 * \brief BufferCtrlObj constructor
 *******************************************************************/

BufferCtrlObj::BufferCtrlObj(Camera& cam, DetInfoCtrlObj& det,
			     const BufferCtrlObj::Info* anInfo) :
  m_cam(cam),
  m_det(det),
  m_reader(NULL)
{
  DEB_CONSTRUCTOR();
  if(anInfo)
    m_info = *anInfo;
  else				// assume local config
    {
      m_info.m_running_on_detector_pc = true;
      m_info.m_keep_nb_images = 0;	// will be calculated after

      m_info.m_watch_path = DEFAULT_WATCH_PATH;
      m_info.m_file_base = DEFAULT_FILE_BASE;
      m_info.m_file_extention = DEFAULT_FILE_EXTENTION;
      m_info.m_file_patern = DEFAULT_FILE_PATERN;
    }
  
  if(!m_info.m_keep_nb_images)
    m_info.m_keep_nb_images = _calcNbMaxImages();

  if(m_info.m_running_on_detector_pc)
    m_reader = new _LocalReader(*this,cam,m_info);
  else
    ;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
BufferCtrlObj::~BufferCtrlObj()
{
  DEB_DESTRUCTOR();
  delete m_reader;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setFrameDim(const FrameDim&)
{
  DEB_MEMBER_FUNCT();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getFrameDim(FrameDim& frame_dim)
{
  DEB_MEMBER_FUNCT();
  Size current_size;
  m_det.getDetectorImageSize(current_size);
  ImageType current_image_type;
  m_det.getCurrImageType(current_image_type);
    
  frame_dim.setSize(current_size);
  frame_dim.setImageType(current_image_type);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::start()
{
    DEB_MEMBER_FUNCT();
    m_reader->start();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::stop()
{
    DEB_MEMBER_FUNCT();
    m_reader->stop();
}

bool BufferCtrlObj::isStopped() const
{
  return m_reader->isStopped();
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::reset()
{
    DEB_MEMBER_FUNCT();
    m_reader->prepareAcq();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setNbBuffers(int nb_buffers)
{
  DEB_MEMBER_FUNCT();
  // no need to change
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getNbBuffers(int& nb_buffers)
{
  DEB_MEMBER_FUNCT();
  nb_buffers = m_info.m_keep_nb_images;
  if(nb_buffers < 0)
    nb_buffers = _calcNbMaxImages();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setNbConcatFrames(int nb_concat_frames)
{
  DEB_MEMBER_FUNCT();
  if(nb_concat_frames != 1)
    THROW_HW_ERROR(NotSupported) << "Not managed";
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getNbConcatFrames(int& nb_concat_frames)
{
    DEB_MEMBER_FUNCT();
    nb_concat_frames = 1;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getMaxNbBuffers(int& max_nb_buffers)
{
    DEB_MEMBER_FUNCT();
    max_nb_buffers = _calcNbMaxImages();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void *BufferCtrlObj::getBufferPtr(int buffer_nb, int concat_frame_nb)
{
    DEB_MEMBER_FUNCT();
    return NULL;		// should not be called
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void *BufferCtrlObj::getFramePtr(int acq_frame_nb)
{
    DEB_MEMBER_FUNCT();
    return NULL;		// should not be called
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int BufferCtrlObj::getLastAcquiredFrame()
{
  return m_reader->getLastAcquiredFrame();
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getStartTimestamp(Timestamp&)
{
    DEB_MEMBER_FUNCT();
    //@todo
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getFrameInfo(int acq_frame_nb, HwFrameInfoType& info)
{
    DEB_MEMBER_FUNCT();

    char filename[1024];
    snprintf(filename,sizeof(filename),
	     m_info.m_file_patern.c_str(),acq_frame_nb);
    std::string fullPath = m_info.m_watch_path + "/";
    fullPath += filename;
    FrameDim aFrameDim;
    getFrameDim(aFrameDim);
    char *aDataBuffer = _readImage(fullPath.c_str());
    info = HwFrameInfoType(acq_frame_nb,aDataBuffer,&aFrameDim,
			   Timestamp(),0,
			   HwFrameInfoType::Shared);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::registerFrameCallback(HwFrameCallback& frame_cb)
{
    DEB_MEMBER_FUNCT();
    HwFrameCallbackGen::registerFrameCallback(frame_cb);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::unregisterFrameCallback(HwFrameCallback& frame_cb)
{
    DEB_MEMBER_FUNCT();
    HwFrameCallbackGen::unregisterFrameCallback(frame_cb);
}

int BufferCtrlObj::_calcNbMaxImages()
{
  DEB_MEMBER_FUNCT();

  struct statvfs fsstat;
  if(statvfs(m_info.m_watch_path.c_str(),&fsstat))
    THROW_HW_ERROR(Error) << "Can't figure out what is the size of that filesystem: " 
			  << m_info.m_watch_path;


  FrameDim anImageDim;
  getFrameDim(anImageDim);
  int nb_block_for_image = anImageDim.getMemSize() / fsstat.f_bsize;
  int aTotalImage = fsstat.f_blocks / nb_block_for_image;
  return aTotalImage / 2;
}

char* BufferCtrlObj::_readImage(const char* filename)
{
  DEB_MEMBER_FUNCT();

  FrameDim anImageDim;
  getFrameDim(anImageDim);
  
  void *aReturnData;
  posix_memalign(&aReturnData,16,anImageDim.getMemSize());
  
  int fd = open(filename,O_RDONLY);
  if(fd < 0)
    {
      free(aReturnData);
      THROW_HW_ERROR(Error) << "Can't open file:" << DEB_VAR1(filename);
    }
  off_t fileSize = lseek(fd,0,SEEK_END);
  if(fileSize - DECTRIS_EDF_OFFSET != anImageDim.getMemSize())
    {
      close(fd),free(aReturnData);
      THROW_HW_ERROR(Error) << "Image is not available yet";
    }
  
  lseek(fd,DECTRIS_EDF_OFFSET,SEEK_SET);
  ssize_t aReadSize = read(fd,aReturnData,anImageDim.getMemSize());
  if(aReadSize != anImageDim.getMemSize())
    {
      close(fd),free(aReturnData);
      THROW_HW_ERROR(Error) << "Problem to read image:" << DEB_VAR1(filename);
    }
  close(fd);
  return (char*)aReturnData;
}

std::ostream& Pilatus::operator <<(std::ostream& os, const BufferCtrlObj::Info& info)
{
  os << '<'
     << "running_on_detector_pc=" << (info.m_running_on_detector_pc ? "YES" : "NO") << ", "
     << "keep_nb_images=" << info.m_keep_nb_images << ", "
     << "watch_path=" << info.m_watch_path << ", "
     << "file_base=" << info.m_file_base << ", "
     << "file_extention=" << info.m_file_extention << ", "
     << "file_patern=" << info.m_file_patern
     << '>';

  return os;
}
