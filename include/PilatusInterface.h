#ifndef PILATUSINTERFACE_H
#define PILATUSINTERFACE_H

#include "HwInterface.h"
#include "PilatusCommunication.h"

namespace lima
{
namespace PilatusCpp
{
class Interface;

/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Pilatus detector info interface
 *******************************************************************/

class DetInfoCtrlObj : public HwDetInfoCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj", "Pilatus");

public:
	DetInfoCtrlObj(Communication& com);
	virtual ~DetInfoCtrlObj();

	virtual void getMaxImageSize(Size& max_image_size);
	virtual void getDetectorImageSize(Size& det_image_size);

	virtual void getDefImageType(ImageType& def_image_type);
	virtual void getCurrImageType(ImageType& curr_image_type);
	virtual void setCurrImageType(ImageType  curr_image_type);

	virtual void getPixelSize(double& pixel_size);
	virtual void getDetectorType(std::string& det_type);
	virtual void getDetectorModel(std::string& det_model);

	virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb){;};
	virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb){;};

private:
	Communication& m_com;
};


/*******************************************************************
 * \class BufferCtrlObj
 * \brief Control object providing Pilatus buffering interface
 *******************************************************************/

class BufferCtrlObj : public HwBufferCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj", "Pilatus");

public:
	BufferCtrlObj(Communication& simu);
	virtual ~BufferCtrlObj();

	virtual void setFrameDim(const FrameDim& frame_dim);
	virtual void getFrameDim(      FrameDim& frame_dim);

	virtual void setNbBuffers(int  nb_buffers);
	virtual void getNbBuffers(int& nb_buffers);

	virtual void setNbConcatFrames(int  nb_concat_frames);
	virtual void getNbConcatFrames(int& nb_concat_frames);

	virtual void getMaxNbBuffers(int& max_nb_buffers);

	virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
	virtual void *getFramePtr(int acq_frame_nb);

	virtual void getStartTimestamp(Timestamp& start_ts);
	virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

	virtual void registerFrameCallback(HwFrameCallback& frame_cb);
	virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);

private:
	BufferCtrlMgr& m_buffer_mgr;
};

/*******************************************************************
 * \class SyncCtrlObj
 * \brief Control object providing Pilatus synchronization interface
 *******************************************************************/

class SyncCtrlObj : public HwSyncCtrlObj
{
	DEB_CLASS_NAMESPC(DebModCamera, "SyncCtrlObj", "Pilatus");

public:
	SyncCtrlObj(Communication& com, HwBufferCtrlObj& buffer_ctrl);
	virtual ~SyncCtrlObj();

	virtual bool checkTrigMode(TrigMode trig_mode);
	virtual void setTrigMode(TrigMode  trig_mode);
	virtual void getTrigMode(TrigMode& trig_mode);

	virtual void setExpTime(double  exp_time);
	virtual void getExpTime(double& exp_time);

	virtual void setLatTime(double  lat_time);
	virtual void getLatTime(double& lat_time);

	virtual void setNbHwFrames(int  nb_frames);
	virtual void getNbHwFrames(int& nb_frames);

	virtual void getValidRanges(ValidRangesType& valid_ranges);

private:
	Communication& m_com;
};


/*******************************************************************
 * \class Interface
 * \brief Pilatus hardware interface
 *******************************************************************/

class Interface : public HwInterface
{
	DEB_CLASS_NAMESPC(DebModCamera, "PilatusInterface", "Pilatus");

public:
	Interface(Communication& com);
	virtual 		~Interface();

	//- From HwInterface
	virtual void 	getCapList(CapList&) const;
	virtual void	reset(ResetLevel reset_level);
	virtual void 	prepareAcq();
	virtual void 	startAcq();
	virtual void 	stopAcq();
	virtual void 	getStatus(StatusType& status);
	virtual int 	getNbHwAcquiredFrames();

private:
	Communication&	m_com;
	CapList 		m_cap_list;
	DetInfoCtrlObj	m_det_info;
	BufferCtrlObj	m_buffer;
	SyncCtrlObj		m_sync;
};



} // namespace Pilatus
} // namespace lima

#endif // PILATUSINTERFACE_H
