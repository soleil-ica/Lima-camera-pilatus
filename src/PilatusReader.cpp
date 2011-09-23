#include <yat/threading/Mutex.h>
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
#include "Debug.h"
#include "Data.h"
#include "PilatusReader.h"
#include "PilatusInterface.h"

//---------------------------
//- Ctor
//---------------------------
Reader::Reader(Camera& cam, HwBufferCtrlObj& buffer_ctrl) :
		m_cam(cam), m_buffer(buffer_ctrl), m_stop_already_done(true), m_image_size(PILATUS_6M_WIDTH, PILATUS_6M_HEIGHT), m_dw(0), m_use_dw(true)
{
	DEB_CONSTRUCTOR();
	try
	{
		m_use_dw = m_cam.isDirectoryWatcherEnabled();
		m_image_number = 0;
		enable_timeout_msg(false);
		enable_periodic_msg(false);
		set_periodic_msg_period(kTASK_PERIODIC_TIMEOUT_MS);
		m_image = new uint32_t[m_image_size.getWidth() * m_image_size.getHeight()];
		memset((uint32_t*) m_image, 0, m_image_size.getWidth() * m_image_size.getHeight() * 4);
	}
	catch (yat::Exception& ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
}

//---------------------------
//- Dtor
//---------------------------
Reader::~Reader()
{
	DEB_DESTRUCTOR();
	try
	{
		if (m_dw != 0)
		{
			delete m_dw;
			m_dw = 0;
		}

		if (m_image != 0)
		{
			delete m_image;
			m_image = 0;
		}
	}
	catch (yat::Exception& ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
}

//---------------------------
//- Reader::start()
//---------------------------
void Reader::start()
{
	DEB_MEMBER_FUNCT();
	try
	{
		if (m_dw != 0)
		{
			delete m_dw;
			m_dw = 0;
		}

		if(m_use_dw)
			m_dw = new gdshare::DirectoryWatcher(m_cam.imgpath());

		this->post(new yat::Message(PILATUS_START_MSG), kPOST_MSG_TMO);
	}
	catch (yat::Exception &ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
}

//---------------------------
//- Reader::stop()
//---------------------------
void Reader::stop()
{
	DEB_MEMBER_FUNCT();
	try
	{
		if (m_dw != 0)
		{
			delete m_dw;
			m_dw = 0;
		}
		this->post(new yat::Message(PILATUS_STOP_MSG), kPOST_MSG_TMO);
	}
	catch (yat::Exception& ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
}

//---------------------------
//- Reader::reset()
//---------------------------
void Reader::reset()
{
	DEB_MEMBER_FUNCT();
	try
	{
		this->post(new yat::Message(PILATUS_RESET_MSG), kPOST_MSG_TMO);
	}
	catch (yat::Exception& ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
}

//---------------------------
//- Reader::getLastAcquiredFrame()
//---------------------------
int Reader::getLastAcquiredFrame(void)
{
	DEB_MEMBER_FUNCT();
	yat::MutexLock scoped_lock(contextual_lock_);
	return m_image_number;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Reader::handle_message(yat::Message& msg) throw (yat::Exception)
{
	DEB_MEMBER_FUNCT();
	try
	{
		switch (msg.type())
		{
			//-----------------------------------------------------
			case yat::TASK_INIT:
			{
				DEB_TRACE() << "Reader::->TASK_INIT";
			}
			break;
			//-----------------------------------------------------
			case yat::TASK_EXIT:
			{
				DEB_TRACE() << "Reader::->TASK_EXIT";
			}
			break;
			//-----------------------------------------------------
			case yat::TASK_TIMEOUT:
			{
				DEB_TRACE() << "Reader::->TASK_TIMEOUT";
			}
			break;
			//-----------------------------------------------------
			case yat::TASK_PERIODIC:
			{
				DEB_TRACE() << "Reader::->TASK_PERIODIC";
				if (m_stop_already_done)
				{
					if (m_elapsed_seconds_from_stop >= 0) //disable TO
					{
						enable_periodic_msg(false);
						return;
					}
					m_elapsed_seconds_from_stop++;
					DEB_TRACE() << "Elapsed seconds since stop() = " << m_elapsed_seconds_from_stop << " s";
				}

				if(m_use_dw)
				{
					if (m_dw)
					{
						//get new created or changed files in the imagepath directory
						gdshare::FileNamePtrVector vecNewFiles, vecChangedFiles, vecNewAndChangedFiles;
						m_dw->GetChanges(&vecNewFiles, &vecChangedFiles);
						vecNewAndChangedFiles.resize(vecNewFiles.size()+vecChangedFiles.size());

						//concatenation
						copy(vecNewFiles.begin(), vecNewFiles.end(), vecNewAndChangedFiles.begin());
						copy(vecChangedFiles.begin(), vecChangedFiles.end(), vecNewAndChangedFiles.begin()+vecNewFiles.size());

						//foreach new or changed file in the watched directory, frame is incremented and an image is copied to the frame ptr
						for (int i = 0; i < vecNewAndChangedFiles.size(); i++)
						{
							if (vecNewAndChangedFiles.at(i)->FileExists())
							{
								DEB_TRACE() << "file : " << vecNewAndChangedFiles.at(i)->NameExt();
								addNewFrame();
							}
						}
					}
				}
				else // use response of camserver at end of exposure to increment nb_frame once for all
				{
					if(m_cam.nbAcquiredImages()!=0)
					{
						DEB_TRACE() << "Exposure SUCCEDED received from CamServer !";//all images (nbImagesInSequence()) are acquired !
						for(int i =0; i<m_cam.nbImagesInSequence();i++)
						{
							addNewFrame();
						}
					}
				}
			}
			break;
			//-----------------------------------------------------
			case PILATUS_START_MSG:
			{
				DEB_TRACE() << "Reader::->PILATUS_START_MSG";
				m_image_number = 0;
				m_elapsed_seconds_from_stop = 1;
				m_stop_already_done = false;
				enable_periodic_msg(true);
			}
			break;
			//-----------------------------------------------------
			case PILATUS_STOP_MSG:
			{
				DEB_TRACE() << "Reader::->PILATUS_STOP_MSG";
				if (!m_stop_already_done)
				{
					m_elapsed_seconds_from_stop = 1;
					m_stop_already_done = true;
				}
			}
			break;
			//-----------------------------------------------------
			case PILATUS_RESET_MSG:
			{
				DEB_TRACE() << "Reader::->PILATUS_RESET_MSG";
			}
			break;
			//-----------------------------------------------------
		}
	}
	catch (yat::Exception& ex)
	{
		DEB_ERROR() << "Error : " << ex.errors[0].desc;
		throw;
	}

}

//---------------------------
//- Reader::addNewFrame()
//---------------------------
void Reader::addNewFrame(void)
{
	DEB_MEMBER_FUNCT();
	try
	{
		StdBufferCbMgr& buffer_mgr = ((reinterpret_cast<BufferCtrlObj&>(m_buffer)).getBufferCbMgr());
		bool continueAcq = false;
		int buffer_nb, concat_frame_nb;

		DEB_TRACE() << "image#" << m_image_number << " acquired !";
		buffer_mgr.setStartTimestamp(Timestamp::now());
		buffer_mgr.acqFrameNb2BufferNb(m_image_number, buffer_nb, concat_frame_nb);

		//simulate an image !
		DEB_TRACE() << "-- copy image in buffer";
		void *ptr = buffer_mgr.getBufferPtr(buffer_nb, concat_frame_nb);
		memcpy((uint32_t *) ptr, (uint32_t *) (m_image), m_image_size.getWidth() * m_image_size.getHeight() * 4); //*4 because 32bits

		DEB_TRACE() << "-- newFrameReady";
		HwFrameInfoType frame_info;
		frame_info.acq_frame_nb = m_image_number;
		continueAcq = buffer_mgr.newFrameReady(frame_info);

		// if nb acquired image < requested frames
		if (continueAcq && (!m_cam.nbImagesInSequence() || m_image_number < (m_cam.nbImagesInSequence() - 1)))
		{
			yat::MutexLock scoped_lock(contextual_lock_);
			{
				m_image_number++;
			}
		}
		else
		{
			stop();
		}
	}
	catch (yat::Exception& ex)
	{
		// Error handling
		DEB_ERROR() << ex.errors[0].desc;
		throw LIMA_HW_EXC(Error, ex.errors[0].desc);
	}
	return;
}
//-----------------------------------------------------
