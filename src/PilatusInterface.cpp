//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#include <algorithm>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include "Debug.h"
#include "PilatusInterface.h"

using namespace lima;
using namespace lima::Pilatus;

static const char* CAMERA_INFO_FILE = "p2_det/config/cam_data/camera.def";
static const char* CAMERA_DEFAULT_USER= "det";

static const char CAMERA_NAME_TOKEN[] = "camera_name";
static const char CAMERA_WIDE_TOKEN[] = "camera_wide";
static const char CAMERA_HIGH_TOKEN[] = "camera_high";

static const char WATCH_PATH[] = "/lima_data";
static const char FILE_PATTERN[] = "tmp_img_%.5d.edf";
static const int  DECTRIS_EDF_OFFSET = 1024;

/*******************************************************************
 * \brief DetInfoCtrlObj constructor
 * \param info if info is NULL look for ~det/p2_det/config/cam_data/camera.def file
 *******************************************************************/
DetInfoCtrlObj::DetInfoCtrlObj(const DetInfoCtrlObj::Info* info)
{
    DEB_CONSTRUCTOR();
    if(info)
      m_info = *info;
    else			// look for local file
      {
	char aBuffer[2048];
	struct passwd aPwd;
	struct passwd *aResultPwd;
	if(getpwnam_r(CAMERA_DEFAULT_USER,&aPwd,
		       aBuffer,sizeof(aBuffer),
		      &aResultPwd))
	  THROW_HW_ERROR(Error) << "Can't get information of user : " 
				<< CAMERA_DEFAULT_USER;
	
	char aConfigFullPath[1024];
	snprintf(aConfigFullPath,sizeof(aConfigFullPath),
		 "%s/%s",aPwd.pw_dir,CAMERA_INFO_FILE);
	FILE* aConfFile = fopen(aConfigFullPath,"r");
	if(!aConfFile)
	  THROW_HW_ERROR(Error) << "Can't open config file :"
				<< aConfigFullPath;
	char aReadBuffer[1024];
	int aWidth = -1,aHeight = -1;
	while(fgets(aReadBuffer,sizeof(aReadBuffer),aConfFile))
	  {
	    if(!strncmp(aReadBuffer,
			CAMERA_NAME_TOKEN,sizeof(CAMERA_NAME_TOKEN) - 1))
	      {
		char *aBeginPt = strchr(aReadBuffer,(unsigned int)'"');
		++aBeginPt;
		char *aEndPt = strrchr(aBeginPt,(unsigned int)'"');
		*aEndPt = '\0';	// remove last "
		m_info.m_det_model = aBeginPt;
	      }
	    else if(!strncmp(aReadBuffer,
			     CAMERA_HIGH_TOKEN,sizeof(CAMERA_HIGH_TOKEN) - 1))
	      {
		char *aPt = aReadBuffer;
		while(*aPt && (*aPt < '1' || *aPt > '9')) ++aPt;
		aHeight = atoi(aPt);
	      }
	    else if(!strncmp(aReadBuffer,
			     CAMERA_WIDE_TOKEN,sizeof(CAMERA_WIDE_TOKEN) - 1))
	      {
		char *aPt = aReadBuffer;
		while(*aPt && (*aPt < '1' || *aPt > '9')) ++aPt;
		aWidth = atoi(aPt);
	      }
	  }
	if(aWidth <= 0 || aHeight <= 0)
	  {
	    fclose(aConfFile);
	    THROW_HW_ERROR(Error) << "Can't get detector info";
	  }
	m_info.m_det_size = Size(aWidth,aHeight);
	fclose(aConfFile);
      }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
DetInfoCtrlObj::~DetInfoCtrlObj()
{
    DEB_DESTRUCTOR();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getMaxImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    // get the max image size
    getDetectorImageSize(size);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    // get the max image size of the detector
    size = m_info.m_det_size;
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getDefImageType(ImageType& image_type)
{
    DEB_MEMBER_FUNCT();
    getCurrImageType(image_type);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getCurrImageType(ImageType& image_type)
{
    DEB_MEMBER_FUNCT();
    image_type= Bpp32S;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::setCurrImageType(ImageType image_type)
{
    DEB_MEMBER_FUNCT();
    ImageType valid_image_type;
    getDefImageType(valid_image_type);
    if (image_type != valid_image_type)
        throw LIMA_HW_EXC(InvalidValue, "Invalid Pixel depth value");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getPixelSize(double& x_size,double& y_size)
{
    DEB_MEMBER_FUNCT();
    x_size = y_size = 172.0e-6;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorType(std::string& type)
{
    DEB_MEMBER_FUNCT();
    type  = "Pilatus";

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorModel(std::string& model)
{
    DEB_MEMBER_FUNCT();
    model = m_info.m_det_model;
}



/*******************************************************************
 * \brief SyncCtrlObj constructor
 *******************************************************************/

SyncCtrlObj::SyncCtrlObj(Camera& cam,DetInfoCtrlObj &det_info)
  :  m_cam(cam),m_latency(det_info.getMinLatTime())

{
}

//-----------------------------------------------------
//
//-----------------------------------------------------
SyncCtrlObj::~SyncCtrlObj()
{
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool SyncCtrlObj::checkTrigMode(TrigMode trig_mode)
{
    bool valid_mode = false;
    switch (trig_mode)
    {
    case IntTrig:
    case IntTrigMult:
    case ExtTrigSingle:
    case ExtTrigMult:
    case ExtGate:
        valid_mode = true;
        break;

    default:
        valid_mode = false;
        break;
    }
    return valid_mode;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setTrigMode(TrigMode trig_mode)
{
    DEB_MEMBER_FUNCT();
    if (!checkTrigMode(trig_mode))
        throw LIMA_HW_EXC(InvalidValue, "Invalid trigger mode");
    Camera::TriggerMode trig;
    switch(trig_mode)
    {
        case IntTrig        : trig = Camera::INTERNAL_SINGLE;
        break;
        case IntTrigMult    : trig = Camera::INTERNAL_MULTI;
        break;
        case ExtTrigSingle  : trig = Camera::EXTERNAL_SINGLE;
        break;
        case ExtTrigMult    : trig = Camera::EXTERNAL_MULTI;
        break;
        case ExtGate        : trig = Camera::EXTERNAL_GATE;
        break;
	default: break;
    }

    m_cam.setTriggerMode(trig);

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getTrigMode(TrigMode& trig_mode)
{
    Camera::TriggerMode trig = m_cam.triggerMode();
    switch(trig)
    {
        case Camera::INTERNAL_SINGLE    :   trig_mode = IntTrig;
        break;
        case Camera::INTERNAL_MULTI     :   trig_mode = IntTrigMult;
        break;
        case Camera::EXTERNAL_SINGLE    :   trig_mode = ExtTrigSingle;
        break;
        case Camera::EXTERNAL_MULTI     :   trig_mode = ExtTrigMult;
        break;
        case Camera::EXTERNAL_GATE      :   trig_mode = ExtGate;
        break;
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setExpTime(double exp_time)
{
    m_exposure_requested = exp_time;
    m_cam.setExposure(exp_time);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getExpTime(double& exp_time)
{
    exp_time = m_cam.exposure();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setLatTime(double lat_time)
{
   m_latency = lat_time;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getLatTime(double& lat_time)
{
    lat_time = m_latency;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setNbHwFrames(int nb_frames)
{
    m_nb_frames = nb_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getNbHwFrames(int& nb_frames)
{
    nb_frames =  m_nb_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getValidRanges(ValidRangesType& valid_ranges)
{
    double min_time = 1e-9;
    double max_time = 1e6;
    valid_ranges.min_exp_time = min_time;
    valid_ranges.max_exp_time = max_time;
    valid_ranges.min_lat_time = m_latency;
    valid_ranges.max_lat_time = max_time;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::prepareAcq()
{

    double exposure =  m_exposure_requested;
    double exposure_period = exposure + m_latency;

    m_cam.setExposurePeriod(exposure_period);

    TrigMode trig_mode;
    getTrigMode(trig_mode);
    int nb_frames = (trig_mode == IntTrigMult)?1:m_nb_frames;
    m_cam.setNbImagesInSequence(nb_frames);

}

/*******************************************************************
 * \brief Interface::_BufferCallback
 *******************************************************************/
class Interface::_BufferCallback : public HwTmpfsBufferMgr::Callback
{
  DEB_CLASS_NAMESPC(DebModCamera, "_BufferCallback", "Pilatus");
public:
  _BufferCallback(Interface& hwInterface) : m_interface(hwInterface) {}

  virtual void prepare(const DirectoryEvent::Parameters &params)
  {
    DEB_MEMBER_FUNCT();

    m_interface.m_cam.setImgpath(params.watch_path);
    m_interface.m_cam.setFileName(params.file_pattern);
  }

  virtual bool getFrameInfo(int image_number,const char* full_path,
			    HwFileEventCallbackHelper::CallFrom from,
			    HwFrameInfoType &frame_info)
  {
    DEB_MEMBER_FUNCT();

    FrameDim anImageDim;
    getFrameDim(anImageDim);
  
    void *aDataBuffer;
    if(posix_memalign(&aDataBuffer,16,anImageDim.getMemSize()))
      THROW_HW_ERROR(Error) << "Can't allocate memory";

    int fd = open(full_path,O_RDONLY);
    if(fd < 0)
      {
	free(aDataBuffer);
	if(from == HwFileEventCallbackHelper::OnDemand)
	  THROW_HW_ERROR(Error) << "Image is no more available";
	else
	  {
	    m_interface.m_cam.errorStopAcquisition();
	    THROW_HW_ERROR(Error) << "Can't open file:" << DEB_VAR1(full_path);
	  }
      }
  
    lseek(fd,DECTRIS_EDF_OFFSET,SEEK_SET);
    ssize_t aReadSize = read(fd,aDataBuffer,anImageDim.getMemSize());
    if(aReadSize != anImageDim.getMemSize())
      {
	close(fd),free(aDataBuffer);
	m_interface.m_cam.errorStopAcquisition();
	THROW_HW_ERROR(Error) << "Problem to read image:" << DEB_VAR1(full_path);
      }
    close(fd);
    
    frame_info = HwFrameInfoType(image_number,aDataBuffer,&anImageDim,
				 Timestamp(),0,
				 HwFrameInfoType::Shared);
    bool aReturnFlag = true;
    if(m_interface.m_buffer.getNbOfFramePending() > 32)
      {
	m_interface.m_cam.errorStopAcquisition();
	aReturnFlag = false;
      }
    else
      aReturnFlag = (image_number + 1) != m_interface.m_cam.nbImagesInSequence();

    return aReturnFlag;
  }
  virtual void getFrameDim(FrameDim& frame_dim)
  {
    DEB_MEMBER_FUNCT();

    Size current_size;
    m_interface.m_det_info.getDetectorImageSize(current_size);
    ImageType current_image_type;
    m_interface.m_det_info.getCurrImageType(current_image_type);
    
    frame_dim.setSize(current_size);
    frame_dim.setImageType(current_image_type);
  }
private:
  Interface&	m_interface;
};

/*******************************************************************
 * \brief Hw Interface constructor
 *******************************************************************/

Interface::Interface(Camera& cam,const DetInfoCtrlObj::Info* info)
            :   m_cam(cam),
                m_det_info(info),
		m_buffer_cbk(new Interface::_BufferCallback(*this)),
                m_buffer(WATCH_PATH,FILE_PATTERN,
			 *m_buffer_cbk),
                m_sync(cam,m_det_info),
		m_saving(cam)
{
    DEB_CONSTRUCTOR();

    HwDetInfoCtrlObj *det_info = &m_det_info;
    m_cap_list.push_back(HwCap(det_info));

    HwBufferCtrlObj *buffer = &m_buffer;
    m_cap_list.push_back(HwCap(buffer));

    HwSyncCtrlObj *sync = &m_sync;
    m_cap_list.push_back(HwCap(sync));

    HwSavingCtrlObj *saving = &m_saving;
    m_cap_list.push_back(HwCap(saving));
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Interface::~Interface()
{
    DEB_DESTRUCTOR();
    delete m_buffer_cbk;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getCapList(HwInterface::CapList &cap_list) const
{
    DEB_MEMBER_FUNCT();
    cap_list = m_cap_list;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::reset(ResetLevel reset_level)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(reset_level);

    stopAcq();

    Size image_size;
    m_det_info.getMaxImageSize(image_size);
    ImageType image_type;
    m_det_info.getDefImageType(image_type);
    FrameDim frame_dim(image_size, image_type);
    m_buffer.setFrameDim(frame_dim);

    m_buffer.setNbConcatFrames(1);
    m_buffer.setNbBuffers(1);
    if(reset_level == HardReset)
        m_cam.hardReset();
    else
	m_cam.softReset();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::prepareAcq()
{
    DEB_MEMBER_FUNCT();

    if(m_saving.isActive())
      m_saving.prepare();
    else
      m_buffer.prepare();
    m_sync.prepareAcq();

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::startAcq()
{
    DEB_MEMBER_FUNCT();

    if(m_saving.isActive())
      m_saving.start();
    else
      m_buffer.start();

    m_cam.startAcquisition();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::stopAcq()
{
    DEB_MEMBER_FUNCT();

    if(m_saving.isActive())
      m_saving.stop();
    else
      m_buffer.stop();

    m_cam.stopAcquisition();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getStatus(StatusType& status)
{

    DEB_MEMBER_FUNCT();
    Camera::Status cam_status = m_cam.status();

    if(cam_status == Camera::STANDBY)
    {
	status.det = DetIdle;
	if(!m_saving.isActive())
	  {
	    int nbFrames;
	    m_sync.getNbHwFrames(nbFrames);
	    if(m_buffer.isStopped())
	      status.acq = AcqReady;
	    else
	      status.acq = getNbHwAcquiredFrames() >= nbFrames ? AcqReady : AcqRunning;
	  }
	else
	  status.acq = AcqReady;
    }
    else if(cam_status == Camera::DISCONNECTED ||
	    cam_status == Camera::ERROR)
    {
        status.det = DetFault;
        status.acq = AcqFault;
    }
    else
    {
        status.det = DetExposure;
        status.acq = AcqRunning;       
    }    
    status.det_mask = DetExposure;
    DEB_TRACE() << DEB_VAR2(cam_status,status);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Interface::getNbHwAcquiredFrames()
{
    DEB_MEMBER_FUNCT();
    int acq_frames = m_buffer.getLastAcquiredFrame()+1;
    return acq_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setThresholdGain(int threshold, Camera::Gain gain)
{
    m_cam.setThresholdGain(threshold, gain);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Interface::getThreshold(void)
{
    return m_cam.threshold();
}


//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::Gain Interface::getGain(void)
{
    return m_cam.gain();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::sendAnyCommand(const std::string& str)
{
    m_cam.sendAnyCommand(str);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setEnergy(double energy)
{
	m_cam.setEnergy(energy);
}
//-----------------------------------------------------
//
//-----------------------------------------------------
double Interface::getEnergy(void)
{
    return m_cam.energy();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
