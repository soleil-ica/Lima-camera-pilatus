#ifndef PILATUSINTERFACE_H
#define PILATUSINTERFACE_H

#include "HwInterface.h"
#include "HwBufferMgr.h"
#include "Debug.h"
#include "PilatusCamera.h"

namespace lima
{
namespace Pilatus
{
class Interface;

/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Pilatus detector info interface
 *******************************************************************/

class DetInfoCtrlObj: public HwDetInfoCtrlObj
{
DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj", "Pilatus");

public:
	struct Info
	{
	  Size		m_det_size;
	  std::string 	m_det_model;
	};
	DetInfoCtrlObj(const Info* = NULL);
	virtual ~DetInfoCtrlObj();

	virtual void getMaxImageSize(Size& max_image_size);
	virtual void getDetectorImageSize(Size& det_image_size);

	virtual void getDefImageType(ImageType& def_image_type);
	virtual void getCurrImageType(ImageType& curr_image_type);
	virtual void setCurrImageType(ImageType curr_image_type);

	virtual void getPixelSize(double& x_size,double &y_size);
	virtual void getDetectorType(std::string& det_type);
	virtual void getDetectorModel(std::string& det_model);

	virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
	{
		;
	}
	;
	virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
	{
		;
	}
	;

	double getMinLatTime() const {return 3e-3;}
private:
	Info	m_info;
};

/*******************************************************************
 * \class BufferCtrlObj
 * \brief Control object providing Pilatus buffering interface
 *******************************************************************/

class BufferCtrlObj: public HwBufferCtrlObj, public HwFrameCallbackGen
{
DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj", "Pilatus");

public:
	struct Info
	{
	  Info() : 
	    m_running_on_detector_pc(false),
	    m_keep_nb_images(-1) // Keep all images
	  {}
	  bool	      m_running_on_detector_pc;
	  int	      m_keep_nb_images;
	  std::string m_watch_path;
	  std::string m_file_base;
	  std::string m_file_extention;
	  std::string m_file_patern;
	};
	BufferCtrlObj(Camera& cam, 
		      DetInfoCtrlObj& det,const Info* info= NULL);
	virtual ~BufferCtrlObj();

	void start();
	void stop();
	void reset();
	virtual void setFrameDim(const FrameDim& frame_dim);
	virtual void getFrameDim(FrameDim& frame_dim);

	virtual void setNbBuffers(int nb_buffers);
	virtual void getNbBuffers(int& nb_buffers);

	virtual void setNbConcatFrames(int nb_concat_frames);
	virtual void getNbConcatFrames(int& nb_concat_frames);

	virtual void getMaxNbBuffers(int& max_nb_buffers);

	virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
	virtual void *getFramePtr(int acq_frame_nb);

	virtual void getStartTimestamp(Timestamp& start_ts);
	virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

	int getLastAcquiredFrame();

	virtual void registerFrameCallback(HwFrameCallback& frame_cb);
	virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);
private:
	class _LocalReader;
	friend class _LocalReader;
	class Reader
	{
	public:
	  virtual ~Reader() {}
	  virtual void start() = 0;
	  virtual void stop() = 0;
	  virtual void prepareAcq() = 0;
	  virtual int getLastAcquiredFrame() const = 0;
	};
	int _calcNbMaxImages();
	char* _readImage(const char*);

  Camera& 		m_cam;
  DetInfoCtrlObj& 	m_det;
  Info			m_info;
  Reader*		m_reader;
};

std::ostream& operator <<(std::ostream& os, const BufferCtrlObj::Info& info);

/*******************************************************************
 * \class SyncCtrlObj
 * \brief Control object providing Pilatus synchronization interface
 *******************************************************************/

class SyncCtrlObj: public HwSyncCtrlObj
{
DEB_CLASS_NAMESPC(DebModCamera, "SyncCtrlObj", "Pilatus");

public:

 SyncCtrlObj(Camera& cam,DetInfoCtrlObj&);
	virtual ~SyncCtrlObj();

	virtual bool checkTrigMode(TrigMode trig_mode);
	virtual void setTrigMode(TrigMode trig_mode);
	virtual void getTrigMode(TrigMode& trig_mode);

	virtual void setExpTime(double exp_time);
	virtual void getExpTime(double& exp_time);

	virtual void setLatTime(double lat_time);
	virtual void getLatTime(double& lat_time);

	virtual void setNbHwFrames(int nb_frames);
	virtual void getNbHwFrames(int& nb_frames);

	virtual void getValidRanges(ValidRangesType& valid_ranges);

	void prepareAcq();
	
private:
	Camera& m_cam;
	int m_nb_frames;
	double m_exposure_requested;
	double m_latency;
};

/*******************************************************************
 * \class Interface
 * \brief Pilatus hardware interface
 *******************************************************************/

class Interface: public HwInterface
{
DEB_CLASS_NAMESPC(DebModCamera, "PilatusInterface", "Pilatus");

public:
	struct Info
	{
	  DetInfoCtrlObj::Info 	m_det_info;
	  BufferCtrlObj::Info 	m_buffer_info;
	};
	Interface(Camera& cam,const Info* = NULL);
	virtual ~Interface();

	//- From HwInterface
	virtual void getCapList(CapList&) const;
	virtual void reset(ResetLevel reset_level);
	virtual void prepareAcq();
	virtual void startAcq();
	virtual void stopAcq();
	virtual void getStatus(StatusType& status);
	virtual int getNbHwAcquiredFrames();

	void setEnergy(double energy);
	double getEnergy(void);
	void setMxSettings(const std::string& str);
	void setThresholdGain(int threshold, Camera::Gain gain);
	int getThreshold(void);
	Camera::Gain getGain(void);
	void sendAnyCommand(const std::string& str);

private:
	Camera& m_cam;
	CapList m_cap_list;
	DetInfoCtrlObj m_det_info;
	BufferCtrlObj m_buffer;
	SyncCtrlObj m_sync;
	double m_latency;
};

} // namespace Pilatus
} // namespace lima

#endif // PILATUSINTERFACE_H
