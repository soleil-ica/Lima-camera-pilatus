namespace Pilatus
{

  class DetInfoCtrlObj: HwDetInfoCtrlObj
  {
%TypeHeaderCode
#include <PilatusInterface.h>
%End
  public:
    struct Info
    {
      Size		m_det_size;
      std::string 	m_det_model;
    };
    DetInfoCtrlObj(const Pilatus::DetInfoCtrlObj::Info* = NULL);
    virtual ~DetInfoCtrlObj();

    virtual void getMaxImageSize(Size& max_image_size /Out/);
    virtual void getDetectorImageSize(Size& det_image_size /Out/);

    virtual void getDefImageType(ImageType& def_image_type /Out/);
    virtual void getCurrImageType(ImageType& curr_image_type /Out/);
    virtual void setCurrImageType(ImageType curr_image_type);

    virtual void getPixelSize(double& x_size,double &y_size);
    virtual void getDetectorType(std::string& det_type);
    virtual void getDetectorModel(std::string& det_model);

    virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb);
    virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb);

    double getMinLatTime() const;
  };

  class BufferCtrlObj: HwBufferCtrlObj,HwFrameCallbackGen
  {
%TypeHeaderCode
#include <PilatusInterface.h>
%End
  public:
    struct Info
    {
      Info();

      bool	      m_running_on_detector_pc;
      int	      m_keep_nb_images;
      std::string m_watch_path;
      std::string m_file_base;
      std::string m_file_extention;
      std::string m_file_patern;
    };
    BufferCtrlObj(Pilatus::Camera& cam, 
		  Pilatus::DetInfoCtrlObj& det,
		  const Pilatus::BufferCtrlObj::Info* info= NULL);
    virtual ~BufferCtrlObj();

    void start();
    void stop();
    void reset();
    bool isStopped() const;

    virtual void setFrameDim(const FrameDim& frame_dim);
    virtual void getFrameDim(FrameDim& frame_dim /Out/);

    virtual void setNbBuffers(int nb_buffers);
    virtual void getNbBuffers(int& nb_buffers /Out/);

    virtual void setNbConcatFrames(int nb_concat_frames);
    virtual void getNbConcatFrames(int& nb_concat_frames /Out/);

    virtual void getMaxNbBuffers(int& max_nb_buffers /Out/);

    virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
    virtual void *getFramePtr(int acq_frame_nb);

    virtual void getStartTimestamp(Timestamp& start_ts /Out/);
    virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

    int getLastAcquiredFrame();

    virtual void registerFrameCallback(HwFrameCallback& frame_cb);
    virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);
  };

  class SyncCtrlObj: HwSyncCtrlObj
  {
%TypeHeaderCode
#include <PilatusInterface.h>
%End
  public:

    SyncCtrlObj(Pilatus::Camera& cam,Pilatus::DetInfoCtrlObj&);
    virtual ~SyncCtrlObj();

    virtual bool checkTrigMode(TrigMode trig_mode);
    virtual void setTrigMode(TrigMode trig_mode);
    virtual void getTrigMode(TrigMode& trig_mode /Out/);

    virtual void setExpTime(double exp_time);
    virtual void getExpTime(double& exp_time /Out/);

    virtual void setLatTime(double lat_time);
    virtual void getLatTime(double& lat_time /Out/);

    virtual void setNbHwFrames(int nb_frames);
    virtual void getNbHwFrames(int& nb_frames /Out/);

    virtual void getValidRanges(ValidRangesType& valid_ranges /Out/);

    void prepareAcq();
  };

  class Interface: HwInterface
  {
%TypeHeaderCode
#include <PilatusInterface.h>
%End
  public:
    struct Info
    {
      Pilatus::DetInfoCtrlObj::Info 	m_det_info;
      Pilatus::BufferCtrlObj::Info 	m_buffer_info;
    };
    Interface(Pilatus::Camera& cam,
	      const Pilatus::Interface::Info* = NULL);
    virtual ~Interface();

    //- From HwInterface
    //    virtual void getCapList(CapList&) const;
    virtual void	getCapList(std::vector<HwCap> &cap_list /Out/) const;
    virtual void reset(ResetLevel reset_level);
    virtual void prepareAcq();
    virtual void startAcq();
    virtual void stopAcq();
    virtual void getStatus(StatusType& status /Out/);
    virtual int getNbHwAcquiredFrames();

    void setEnergy(double energy);
    double getEnergy();
    void setMxSettings(const std::string& str);
    void setThresholdGain(int threshold, Pilatus::Camera::Gain gain);
    int getThreshold();
    Pilatus::Camera::Gain getGain();
    void sendAnyCommand(const std::string& str);
  };

}; // namespace Pilatus
