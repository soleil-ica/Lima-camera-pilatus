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
    Interface(Pilatus::Camera& cam,
	      const Pilatus::DetInfoCtrlObj::Info* = NULL);
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
    void setThresholdGain(int threshold, Pilatus::Camera::Gain gain);
    int getThreshold();
    Pilatus::Camera::Gain getGain();
    void sendAnyCommand(const std::string& str);
  };

}; // namespace Pilatus
