#include <algorithm>
#include "Debug.h"
#include "Data.h"
#include "PilatusReader.h"
#include "PilatusInterface.h"



/*******************************************************************
 * \brief DetInfoCtrlObj constructor
 *******************************************************************/
DetInfoCtrlObj::DetInfoCtrlObj(Communication& com)
				:m_com(com)
{
	DEB_CONSTRUCTOR();
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
	size= Size(2463,2527);
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
		THROW_HW_ERROR(Error) << "Cannot change to "  << DEB_VAR2(image_type, valid_image_type);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void DetInfoCtrlObj::getPixelSize(double& size)
{
	DEB_MEMBER_FUNCT();
	size= 172.0;
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
	model = "Pilatus_6M";
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double DetInfoCtrlObj::getMinLatency()
{
	return 0.003;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double DetInfoCtrlObj::get_max_latency()
{
	return 2^31;
}
	
/*******************************************************************
 * \brief BufferCtrlObj constructor
 *******************************************************************/

BufferCtrlObj::BufferCtrlObj(Communication& com, DetInfoCtrlObj& det)
				:
					m_buffer_cb_mgr(m_buffer_alloc_mgr),
					m_buffer_ctrl_mgr(m_buffer_cb_mgr),
					m_com(com),
					m_det(det)
{
	DEB_CONSTRUCTOR();
	////m_reader = new Reader(com,*this);
	////m_reader->go(2000);	
}

//-----------------------------------------------------
//
//-----------------------------------------------------
BufferCtrlObj::~BufferCtrlObj()
{
	DEB_DESTRUCTOR();
	////m_reader->stop();	
	////m_reader->exit();
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
	
	/*
	Size image_size;
	m_det.getMaxImageSize(image_size);
	frame_dim.setSize(image_size);
	
	ImageType image_type;	
	m_det.getDefImageType(image_type);
	frame_dim.setImageType(image_type);
	*/
	m_buffer_ctrl_mgr.getFrameDim(frame_dim);//remove or not ??	
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::start()
{
	DEB_MEMBER_FUNCT();
	////m_reader->start();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::stop()
{
	DEB_MEMBER_FUNCT();
	////m_reader->stop();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void BufferCtrlObj::reset()
{
	DEB_MEMBER_FUNCT();
	////m_reader->reset();
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
	max_nb_buffers = ( (Communication::DEFAULT_TMPFS_SIZE)/(imageSize.getWidth() * imageSize.getHeight() * 4) )/2; //4 == image 32bits	
cout<<"> max_nb_buffers = "<<max_nb_buffers<<endl;
	////m_buffer_ctrl_mgr.getMaxNbBuffers(max_nb_buffers);
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

SyncCtrlObj::SyncCtrlObj(Communication& com, HwBufferCtrlObj& buffer_ctrl, DetInfoCtrlObj& det)
			: HwSyncCtrlObj(buffer_ctrl), m_com(com), m_det(det)
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
	case ExtGate:
		valid_mode = true;
		break;

	default:
		valid_mode = false;
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
		THROW_HW_ERROR(InvalidValue) << "Invalid " << DEB_VAR1(trig_mode);
	Communication::TriggerMode trig;
	switch(trig_mode)
	{
		case IntTrig :	 trig = Communication::INTERNAL; break;
		case IntTrigMult : trig = Communication::INTERNAL_TRIG_MULTI; break;
		case ExtTrigSingle : trig = Communication::EXTERNAL_START; break;
		case ExtTrigMult : trig = Communication::EXTERNAL_MULTI_START; break;
		case ExtGate : trig = Communication::EXTERNAL_GATE; break;
	}
								   
	m_com.setTriggerMode(trig);

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getTrigMode(TrigMode& trig_mode)
{
	Communication::TriggerMode trig = m_com.triggerMode();
	switch(trig)
	{
		case Communication::INTERNAL :	 trig_mode = IntTrig; break;
		case Communication::INTERNAL_TRIG_MULTI : trig_mode = IntTrigMult; break;
		case Communication::EXTERNAL_START : trig_mode = ExtTrigSingle; break;
		case Communication::EXTERNAL_MULTI_START : trig_mode = ExtTrigMult; break;
		case Communication::EXTERNAL_GATE : trig_mode = ExtGate; break;
	}	
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setExpTime(double exp_time)
{

	m_com.setExposure(exp_time);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getExpTime(double& exp_time)
{
	exp_time = m_com.exposure();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::setLatTime(double lat_time)
{
	//@TODO
}

void SyncCtrlObj::getLatTime(double& lat_time)
{
	//@TODO
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
	double min_time = 10e-9;
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

	double exposure =  m_com.exposure();
cout<<"> exposure = "<<exposure<<endl;
	double latency = m_det.getMinLatency();
cout<<"> latency = "<<latency<<endl;
	double exposure_period = exposure + latency;
cout<<"> exposure_period = "<<exposure_period<<endl;
	m_com.setExposurePeriod(exposure_period);

	TrigMode trig_mode;
	getTrigMode(trig_mode);
	int nb_frames = (trig_mode == IntTrigMult)?1:m_nb_frames;
	m_com.setNbImagesInSequence(nb_frames);

}


/*******************************************************************
 * \brief Hw Interface constructor
 *******************************************************************/

Interface::Interface(Communication& com)
			: 	m_com(com),
				m_det_info(com),
				m_buffer(com,m_det_info ),
				m_sync(com, m_buffer, m_det_info)
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

	Communication::Status com_status = Communication::OK;
	com_status = m_com.status();	
	if (com_status == Communication::DISCONNECTED)
		m_com.connect(m_com.serverIP().c_str(),m_com.serverPort());
    m_buffer.reset();
	m_sync.prepareAcq();    

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::startAcq()
{
	DEB_MEMBER_FUNCT();
	m_com.startAcquisition();	
	m_buffer.start();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::stopAcq()
{
	DEB_MEMBER_FUNCT();
	m_com.stopAcquisition();
	m_buffer.stop();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getStatus(StatusType& status)
{

	Communication::Status com_status = Communication::OK;
	com_status = m_com.status();
	if(com_status == Communication::ERROR)
	{
		status.det = DetFault;
		status.acq = AcqFault;		
	}
	else
	{
		if(com_status != Communication::OK)
		{
			status.det = DetExposure;
			status.acq = AcqRunning;		
		}
		else
		{
			status.det = DetIdle;
            int lastAcquiredFrame = -1;//self.__buffer.getLastAcquiredFrame()
            int requestNbFrame = -1;
			m_sync.getNbHwFrames(requestNbFrame);
			if(lastAcquiredFrame >= 0 && lastAcquiredFrame == (requestNbFrame - 1))
				status.acq = AcqReady;
			else
				status.acq = AcqRunning;
		}
	}
	status.det_mask = DetExposure|DetFault;

}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Interface::getNbHwAcquiredFrames()
{
	DEB_MEMBER_FUNCT();
	int acq_frames = 1;
	//self.__buffer.getLastAcquiredFrame() + 1
	return acq_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setMxSettings(const std::string& str)
{
	std::string str_to_send ="mxsettings ";
	str_to_send+=str;
	m_com.send(str);	
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::setThresholdGain(int threshold, Communication::Gain gain)
{
	m_com.setThresholdGain(threshold, gain);
}

//-----------------------------------------------------
//
//-----------------------------------------------------	
int Interface::getThreshold(void)
{
	return m_com.threshold();
}


//-----------------------------------------------------
//
//-----------------------------------------------------	
Communication::Gain Interface::getGain(void)
{
	return m_com.gain();
}
//-----------------------------------------------------



