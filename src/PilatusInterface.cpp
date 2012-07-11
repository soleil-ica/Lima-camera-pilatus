#include <sys/statvfs.h>
#include <sys/types.h>
#include <pwd.h>
#include <algorithm>
#include "Debug.h"
#include "PilatusInterface.h"

using namespace lima;
using namespace lima::Pilatus;

static const char* CAMERA_INFO_FILE = "p2_det/config/cam_data/camera.def";
static const char* CAMERA_DEFAULT_USER= "det";

static const char CAMERA_NAME_TOKEN[] = "camera_name";
static const char CAMERA_WIDE_TOKEN[] = "camera_wide";
static const char CAMERA_HIGH_TOKEN[] = "camera_high";

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
	while(fgets(aReadBuffer,sizeof(aReadBuffer),aConfFile))
	  {
	    int aWidth = -1,aHeight = -1;
	    if(!strncmp(aReadBuffer,
			CAMERA_NAME_TOKEN,sizeof(CAMERA_NAME_TOKEN)))
	      {
		char *aBeginPt = strchr(aReadBuffer,(unsigned int)'"');
		char *aEndPt = strrchr(aBeginPt,(unsigned int)'"');
		*aEndPt = '\0';	// remove last "
		m_info.m_det_model = aBeginPt;
	      }
	    else if(!strncmp(aReadBuffer,
			     CAMERA_HIGH_TOKEN,sizeof(CAMERA_HIGH_TOKEN)))
	      {
		char *aPt = aReadBuffer;
		while(*aPt && *aPt < '1' && *aPt > '9') ++aPt;
		aHeight = atoi(aPt);
	      }
	    else if(!strncmp(aReadBuffer,
			     CAMERA_WIDE_TOKEN,sizeof(CAMERA_WIDE_TOKEN)))
	      {
		char *aPt = aReadBuffer;
		while(*aPt && *aPt < '1' && *aPt > '9') ++aPt;
		aWidth = atoi(aPt);
	      }
	    if(aWidth < 0 || aHeight < 0)
	      {
		fclose(aConfFile);
		THROW_HW_ERROR(Error) << "Can't get detector info";
	      }
	    m_info.m_det_size = Size(aWidth,aHeight);
	  }
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
    image_type= Bpp32;
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
    valid_ranges.min_lat_time = min_time;
    valid_ranges.max_lat_time = max_time;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj:: prepareAcq()
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
 * \brief Hw Interface constructor
 *******************************************************************/

Interface::Interface(Camera& cam,const Interface::Info* info)
            :   m_cam(cam),
                m_det_info(info ? &info->m_det_info : NULL),
                m_buffer(cam,m_det_info,
			 info ? &info->m_buffer_info : NULL),
                m_sync(cam,m_det_info)
{
    DEB_CONSTRUCTOR();

    HwDetInfoCtrlObj *det_info = &m_det_info;
    m_cap_list.push_back(HwCap(det_info));

    HwBufferCtrlObj *buffer = &m_buffer;
    m_cap_list.push_back(HwCap(buffer));

    HwSyncCtrlObj *sync = &m_sync;
    m_cap_list.push_back(HwCap(sync));

}

//-----------------------------------------------------
//
//-----------------------------------------------------
Interface::~Interface()
{
    DEB_DESTRUCTOR();
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
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::prepareAcq()
{
    DEB_MEMBER_FUNCT();

    Camera::Status cam_status = m_cam.status();
    if (cam_status == Camera::DISCONNECTED)
        m_cam.connect(m_cam.serverIP(),m_cam.serverPort());
    m_buffer.reset();
    m_sync.prepareAcq();

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::startAcq()
{
    DEB_MEMBER_FUNCT();  
    m_cam.startAcquisition();
    m_buffer.start();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::stopAcq()
{
    DEB_MEMBER_FUNCT();
    m_buffer.stop();    
    m_cam.stopAcquisition();

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getStatus(StatusType& status)
{

    DEB_MEMBER_FUNCT();
    Camera::Status cam_status = Camera::STANDBY;
    cam_status = m_cam.status();

    if(cam_status == Camera::STANDBY)
    {
	status.det = DetIdle;

        int nbFrames;
        m_sync.getNbHwFrames(nbFrames);
	status.acq = getNbHwAcquiredFrames() >= nbFrames ? AcqReady : AcqRunning;
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
    status.det_mask = DetExposure | DetReadout | DetLatency;
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
void Interface::setMxSettings(const std::string& str)
{
    std::string str_to_send ="mxsettings ";
    str_to_send+=str;
    m_cam.sendAnyCommand(str_to_send);
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
