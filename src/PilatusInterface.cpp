#include <algorithm>

#include <sys/types.h>
#include <pwd.h>
#include "PilatusReader.h"
#include "PilatusInterface.h"


static const char CAMERA_NAME_TOKEN[] = "camera_name";
static const char CAMERA_WIDTH_TOKEN[] = "camera_wide";
static const char CAMERA_HEIGHT_TOKEN[] = "camera_high";
static const char CAMERA_BPP_TOKEN[] = "camera_bpp";
/*******************************************************************
 * \brief DetInfoCtrlObj constructor
 *******************************************************************/
DetInfoCtrlObj::DetInfoCtrlObj(Camera& cam)
:m_cam(cam)
{
    DEB_CONSTRUCTOR();
    char aBuffer[2048];

    char aReadBuffer[1024];
    //by default it is a PILATUS 6M
    m_det_model = PILATUS_MODEL;
    int aWidth = PILATUS_6M_WIDTH, aHeight = PILATUS_6M_HEIGHT;
    m_bpp = 32;

    if(m_cam.camDefFileName() != "NONE")
    {
        FILE* aConfFile = fopen(m_cam.camDefFileName().c_str(), "r");
        if(!aConfFile)
            THROW_HW_ERROR(Error) << "Can't open config file :" << m_cam.camDefFileName().c_str();

        while(fgets(aReadBuffer, sizeof(aReadBuffer), aConfFile))
        {
            if(!strncmp(aReadBuffer, CAMERA_NAME_TOKEN, sizeof(CAMERA_NAME_TOKEN) - 1))
            {
                char *aBeginPt = strchr(aReadBuffer, (unsigned int)'"');
                ++aBeginPt;
                char *aEndPt = strrchr(aBeginPt, (unsigned int)'"');
                *aEndPt = '\0'; // remove last "
                m_det_model = aBeginPt;
            }
            else if(!strncmp(aReadBuffer, CAMERA_HEIGHT_TOKEN, sizeof(CAMERA_HEIGHT_TOKEN) - 1))
            {
                char *aPt = aReadBuffer;
                while(*aPt && (*aPt < '1' || *aPt > '9')) ++aPt;
                aHeight = atoi(aPt);
            }
            else if(!strncmp(aReadBuffer, CAMERA_WIDTH_TOKEN, sizeof(CAMERA_WIDTH_TOKEN) - 1))
            {
                char *aPt = aReadBuffer;
                while(*aPt && (*aPt < '1' || *aPt > '9')) ++aPt;
                aWidth = atoi(aPt);
            }
            else if(!strncmp(aReadBuffer, CAMERA_BPP_TOKEN, sizeof(CAMERA_BPP_TOKEN) - 1))
            {
                char *aPt = aReadBuffer;
                while(*aPt && (*aPt < '1' || *aPt > '9')) ++aPt;
                m_bpp = atoi(aPt);
            }
        }
        fclose(aConfFile);
    }

    m_det_size = Size(aWidth, aHeight);
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
    size = Size(m_det_size.getWidth(), m_det_size.getHeight());
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
    switch(m_bpp)//this is for PILATUS 1M, for PILATUS 6M we do not use image at all
    {
        case 32:
            image_type = Bpp32S;
            break;
        default:
            THROW_HW_ERROR(Error) << "Invalid Pixel depth value !";
            break;
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::setCurrImageType(ImageType image_type)
{
    DEB_MEMBER_FUNCT();
    switch(image_type)
    {
        case Bpp32S://By Default Bpp32S => NOP
            break;
        default:
            THROW_HW_ERROR(Error) << "Invalid Pixel depth value !";
            break;
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getPixelSize(double& x_size, double& y_size)
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
    type = "Pilatus";

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorModel(std::string& model)
{
    DEB_MEMBER_FUNCT();
    model = m_det_model;
}
/*******************************************************************
 * \brief BufferCtrlObj constructor
 *******************************************************************/

BufferCtrlObj::BufferCtrlObj(Camera& cam, DetInfoCtrlObj& det)
:
m_buffer_cb_mgr(m_buffer_alloc_mgr),
m_buffer_ctrl_mgr(m_buffer_cb_mgr),
m_cam(cam),
m_det(det)
{
    DEB_CONSTRUCTOR();
    m_reader = new Reader(cam, det, *this);
    m_reader->go(2000);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
BufferCtrlObj::~BufferCtrlObj()
{
    DEB_DESTRUCTOR();
    m_reader->stop();
    m_reader->exit();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setFrameDim(const FrameDim& frame_dim)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.setFrameDim(frame_dim);
    return;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getFrameDim(FrameDim& frame_dim)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.getFrameDim(frame_dim); //remove or not ??
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

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::reset()
{
    DEB_MEMBER_FUNCT();
    m_reader->reset();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool BufferCtrlObj::isTimeoutSignaled()
{
    return m_reader->isTimeoutSignaled();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setTimeout(double TO)
{
    DEB_MEMBER_FUNCT();
    m_reader->setTimeout(TO);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool BufferCtrlObj::isRunning()
{
    return m_reader->isRunning();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setNbBuffers(int nb_buffers)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.setNbBuffers(nb_buffers);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getNbBuffers(int& nb_buffers)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.getNbBuffers(nb_buffers);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::setNbConcatFrames(int nb_concat_frames)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.setNbConcatFrames(nb_concat_frames);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getNbConcatFrames(int& nb_concat_frames)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.getNbConcatFrames(nb_concat_frames);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getMaxNbBuffers(int& max_nb_buffers)
{
    DEB_MEMBER_FUNCT();

    Size imageSize;
    m_det.getMaxImageSize(imageSize);
    max_nb_buffers = ((Camera::DEFAULT_TMPFS_SIZE) / (imageSize.getWidth() * imageSize.getHeight() * 4)); //4 == image 32bits
    m_buffer_ctrl_mgr.getMaxNbBuffers(max_nb_buffers);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void *BufferCtrlObj::getBufferPtr(int buffer_nb, int concat_frame_nb)
{
    DEB_MEMBER_FUNCT();
    return m_buffer_ctrl_mgr.getBufferPtr(buffer_nb, concat_frame_nb);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void *BufferCtrlObj::getFramePtr(int acq_frame_nb)
{
    DEB_MEMBER_FUNCT();
    return m_buffer_ctrl_mgr.getFramePtr(acq_frame_nb);
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
void BufferCtrlObj::getStartTimestamp(Timestamp& start_ts)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.getStartTimestamp(start_ts);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::getFrameInfo(int acq_frame_nb, HwFrameInfoType& info)
{
    DEB_MEMBER_FUNCT();
    m_buffer_ctrl_mgr.getFrameInfo(acq_frame_nb, info);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::registerFrameCallback(HwFrameCallback& frame_cb)
{
    DEB_MEMBER_FUNCT();
    //@TODO
    m_buffer_ctrl_mgr.registerFrameCallback(frame_cb);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::unregisterFrameCallback(HwFrameCallback& frame_cb)
{
    DEB_MEMBER_FUNCT();
    //@TODO
    m_buffer_ctrl_mgr.unregisterFrameCallback(frame_cb);
}
/*******************************************************************
 * \brief SyncCtrlObj constructor
 *******************************************************************/

SyncCtrlObj::SyncCtrlObj(Camera& cam)
:m_cam(cam), m_latency(LATENCY_DEFAULT_VALUE)

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
    switch(trig_mode)
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
    if(!checkTrigMode(trig_mode))
        throw LIMA_HW_EXC(InvalidValue, "Invalid trigger mode");
    Camera::TriggerMode trig;
    switch(trig_mode)
    {
        case IntTrig: trig = Camera::INTERNAL_SINGLE;
            break;
        case IntTrigMult: trig = Camera::INTERNAL_MULTI;
            break;
        case ExtTrigSingle: trig = Camera::EXTERNAL_SINGLE;
            break;
        case ExtTrigMult: trig = Camera::EXTERNAL_MULTI;
            break;
        case ExtGate: trig = Camera::EXTERNAL_GATE;
            break;
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
        case Camera::INTERNAL_SINGLE: trig_mode = IntTrig;
            break;
        case Camera::INTERNAL_MULTI: trig_mode = IntTrigMult;
            break;
        case Camera::EXTERNAL_SINGLE: trig_mode = ExtTrigSingle;
            break;
        case Camera::EXTERNAL_MULTI: trig_mode = ExtTrigMult;
            break;
        case Camera::EXTERNAL_GATE: trig_mode = ExtGate;
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
    nb_frames = m_nb_frames;
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
void SyncCtrlObj::prepareAcq()
{

    double exposure = m_exposure_requested;
    double exposure_period = exposure + m_latency;

    m_cam.setExposurePeriod(exposure_period);

    TrigMode trig_mode;
    getTrigMode(trig_mode);
    int nb_frames = (trig_mode == IntTrigMult) ? 1 : m_nb_frames;
    m_cam.setNbImagesInSequence(nb_frames);

}
/*******************************************************************
 * \brief Hw Interface constructor
 *******************************************************************/

Interface::Interface(Camera& cam)
:m_cam(cam),
m_det_info(cam),
m_buffer(cam, m_det_info),
m_sync(cam)
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

    Camera::Status cam_status = Camera::OK;
    cam_status = m_cam.status();
    if(cam_status == Camera::DISCONNECTED)
        m_cam.connect(m_cam.serverIP().c_str(), m_cam.serverPort());
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


    if(cam_status == Camera::OK)
    {
        status.det = DetIdle;
        status.acq = AcqReady;
    }
    else if(cam_status == Camera::KILL_ACQUISITION)
    {
        status.det = DetIdle;
        if(m_buffer.isRunning())
            status.acq = AcqRunning;    
        else if(m_buffer.isTimeoutSignaled())
            status.acq = AcqFault;
        else
            status.acq = AcqReady;        
    }
    else if(cam_status == Camera::STANDBY)
    {
        status.det = DetIdle;

        int nbFrames = 0;
        m_sync.getNbHwFrames(nbFrames);
        if(getNbHwAcquiredFrames() >= nbFrames)
            status.acq = AcqReady;        
        else if(m_buffer.isRunning())
            status.acq = AcqRunning;
        else if(m_buffer.isTimeoutSignaled())
            status.acq = AcqFault;
        else
            status.acq = AcqReady;
    }
    else if(cam_status == Camera::DISCONNECTED)
    {
        status.det = DetFault;
        status.acq = AcqFault;
    }
    else if(cam_status == Camera::ERROR)
    {
        status.det = DetIdle;
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
    int acq_frames = m_buffer.getLastAcquiredFrame() + 1;
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
void Interface::setTimeout(double TO)
{
    DEB_MEMBER_FUNCT();
    m_buffer.setTimeout(TO);
}
//-----------------------------------------------------