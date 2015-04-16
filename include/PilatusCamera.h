#ifndef PILATUSCAMERA_H
#define PILATUSCAMERA_H

#include "lima/Debug.h"


static const char  SOCKET_SEPARATOR = '\030';
static const char* SPLIT_SEPARATOR  = "\x18"; // '\x18' == '\030'

namespace lima
{
namespace Pilatus
{
class Camera
{
    DEB_CLASS_NAMESPC(DebModCameraCom,"Camera","Pilatus");

public:
    enum Status
    {
        ERROR,
        DISCONNECTED,
        OK,
        SETTING_ENERGY,
        SETTING_THRESHOLD,
        SETTING_EXPOSURE,
        SETTING_NB_IMAGE_IN_SEQUENCE,
        SETTING_EXPOSURE_PERIOD,
        SETTING_HARDWARE_TRIGGER_DELAY,
        SETTING_EXPOSURE_PER_FRAME,
        KILL_ACQUISITION,
        RUNNING,
        ANYCMD,
        STANDBY
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

    Camera(const char *host = NULL,int port = 0, const std::string& cam_def_file_name = "camera.def");
    ~Camera();
    
    void connect(const char* host,int port);
    
    std::string serverIP() const;
    int serverPort() const;
    
	void enableReaderWatcher(void);
	void disableReaderWatcher(void);
	bool isReaderWatcher();

    void setImgpath(const std::string& path);
    const std::string& imgpath(void);
    
    void setFileName(const std::string& name);
    const std::string& fileName(void);
    
    const std::string& camDefFileName(void);    
    
    Status status() const;

    double energy() const;
    void setEnergy(double val);

    void setThreshold(double val);
    int threshold() const;
    Gain gain() const;
    void setThresholdGain(int threshold,Gain gain = DEFAULT_GAIN);

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

    bool gapfill() const;
    void setGapfill(bool onOff);
    
    void send(const std::string& message);
    
    void sendAnyCommand(const std::string& message);    
    std::string sendAnyCommandAndGetErrorMsg(const std::string& message);
    int nbAcquiredImages();
    
    //s
    static const long long          DEFAULT_TMPFS_SIZE = 24LL * 1024 * 1024 * 1024;// 24Go
    static const double             TIME_OUT = 10.;
 
    
private:
    
    const        std::string& _errorMessage() const;
    void         _softReset();
    void         _hardReset();
    
    static void* _runFunc(void*);
    void         _run();    
    void         _initVariable();
    void         _resync();
    void         _reinit();
    
    std::map<std::string,Gain>    GAIN_SERVER_RESPONSE;
    std::map<Gain,std::string>    GAIN_VALUE2SERVER;


    //socket/synchronization with pilatus variables
    std::string             m_server_ip;
    int                     m_server_port; 
    std::string             m_cam_def_file_name;
    int                     m_socket;
    bool                    m_stop;
    pthread_t               m_thread_id;
    int                     m_pipes[2];
    Status                  m_state;
    mutable Cond            m_cond;

    //Cache variables
    bool 					m_use_reader_watcher;
    std::string             m_error_message;
    double                  m_energy;
    double                  m_exposure;
    int                     m_exposure_per_frame;
    double                  m_exposure_period;
    Gain                    m_gain;
    bool                    m_gap_fill;
    double                  m_hardware_trigger_delay;
    int                     m_nimages;
    int                     m_threshold;
    TriggerMode             m_trigger_mode;
    std::string             m_imgpath;
    std::string             m_file_name;
    std::string             m_file_pattern;    
    int						m_nb_acquired_images;
};
}
}
#endif//PILATUSCAMERA_H
