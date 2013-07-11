namespace Pilatus
{
  class Camera
  {
%TypeHeaderCode
#include <PilatusInterface.h>
#include <PilatusCamera.h>
%End
  public:
    enum Status
      {
        ERROR,
        DISCONNECTED,
        STANDBY,
        SETTING_ENERGY,
        SETTING_THRESHOLD,
        SETTING_EXPOSURE,
        SETTING_NB_IMAGE_IN_SEQUENCE,
        SETTING_EXPOSURE_PERIOD,
        SETTING_HARDWARE_TRIGGER_DELAY,
        SETTING_EXPOSURE_PER_FRAME,
        KILL_ACQUISITION,
        RUNNING
      };

    enum Gain
      {
        DEFAULT_GAIN,
        LOW,
        MID,
        HIGH,
        UHIGH
      };

    enum TriggerMode
      {
        INTERNAL_SINGLE,
        INTERNAL_MULTI,
        EXTERNAL_SINGLE,
        EXTERNAL_MULTI,
        EXTERNAL_GATE
      };

    Camera(const char *host = "localhost",int port = 41234);
    ~Camera();
    
    void connect(const char* host,int port);
    
    const char* serverIP() const;
    int serverPort() const;

    void setImgpath(const std::string& path);
    const std::string& imgpath() const;
    
    void setFileName(const std::string& name);
    const std::string& fileName() const;
    
    Status status() const;

    double energy() const;
    void setEnergy(double val);

    int threshold() const;
    Pilatus::Camera::Gain gain() const;
    void setThresholdGain(int threshold,
			  Pilatus::Camera::Gain gain = DEFAULT_GAIN);

    double exposure() const;
    void setExposure(double expo);

    double exposurePeriod() const;
    void setExposurePeriod(double expo_period);

    int nbImagesInSequence() const;
    void setNbImagesInSequence(int nb);

    double hardwareTriggerDelay() const;
    void setHardwareTriggerDelay(double);

    int nbExposurePerFrame() const;
    void setNbExposurePerFrame(int);

    TriggerMode triggerMode() const;
    void setTriggerMode(TriggerMode);

    void startAcquisition(int image_number = 0);
    void stopAcquisition();
    void errorStopAcquisition();

    bool gapfill() const;
    void setGapfill(bool onOff);
    
    void send(const std::string& message);
    
    void sendAnyCommand(const std::string& message);    

    int nbAcquiredImages() const;
   };
};
