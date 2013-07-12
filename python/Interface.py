############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2011
# European Synchrotron Radiation Facility
# BP 220, Grenoble 38043
# FRANCE
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################
from Lima import Core

from DetInfoCtrlObj import DetInfoCtrlObj
from SyncCtrlObj import SyncCtrlObj
from BufferCtrlObj import BufferCtrlObj
from Communication import Communication

DEFAULT_SERVER_PORT = 41234

class Interface(Core.HwInterface) :
    Core.DEB_CLASS(Core.DebModCamera, "Interface")

    def __init__(self,port = DEFAULT_SERVER_PORT) :
	Core.HwInterface.__init__(self)
        self.__port = port

        self.__comm = Communication('localhost',self.__port)
        self.__detInfo = DetInfoCtrlObj()
        self.__detInfo.init()
        self.__buffer = BufferCtrlObj(self.__comm,self.__detInfo)
        self.__syncObj = SyncCtrlObj(self.__comm,self.__detInfo)
	self.__acquisition_start_flag = False
        self.__image_number = 0
        
    def __del__(self) :
        self.__comm.quit()
        self.__buffer.quit()

    def quit(self) :
        self.__comm.quit()
        self.__buffer.quit()
        
    @Core.DEB_MEMBER_FUNCT
    def getCapList(self) :
        return [Core.HwCap(x) for x in [self.__detInfo,self.__syncObj,self.__buffer]]

    @Core.DEB_MEMBER_FUNCT
    def reset(self,reset_level):
        if reset_level == self.HardReset:
            self.__comm.hard_reset()

        self.__buffer.reset()
        self.__comm.soft_reset()
        
    @Core.DEB_MEMBER_FUNCT
    def prepareAcq(self):
        camserverStatus = self.__comm.status()
        if camserverStatus == self.__comm.DISCONNECTED:
            self.__comm.connect('localhost',self.__port)

        self.__buffer.reset()
        self.__syncObj.prepareAcq()
        self.__image_number = 0

    @Core.DEB_MEMBER_FUNCT
    def startAcq(self) :
        self.__acquisition_start_flag = True
        self.__comm.start_acquisition(self.__image_number)
        self.__image_number += 1
        self.__buffer.start()
        
    @Core.DEB_MEMBER_FUNCT
    def stopAcq(self) :
        self.__comm.stop_acquisition()
        self.__buffer.stop()
        self.__acquisition_start_flag = False
        
    @Core.DEB_MEMBER_FUNCT
    def getStatus(self) :
        camserverStatus = self.__comm.status()
        status = Core.HwInterface.StatusType()

        if self.__buffer.is_error() :
            status.det = Core.DetFault
            status.acq = Core.AcqFault
            deb.Error("Buffer is in Fault stat")
        elif camserverStatus == self.__comm.ERROR:
            status.det = Core.DetFault
            status.acq = Core.AcqFault
            deb.Error("Detector is in Fault stat")
        else:
            if camserverStatus != self.__comm.OK:
                status.det = Core.DetExposure
                status.acq = Core.AcqRunning
            else:
                status.det = Core.DetIdle
                lastAcquiredFrame = self.__buffer.getLastAcquiredFrame()
                requestNbFrame = self.__syncObj.getNbFrames()
                if not self.__acquisition_start_flag or (lastAcquiredFrame >= 0 and lastAcquiredFrame == (requestNbFrame - 1)):
                    status.acq = Core.AcqReady
                else:
                    status.acq = Core.AcqRunning
            
        status.det_mask = (Core.DetExposure|Core.DetFault)
        return status
    
    @Core.DEB_MEMBER_FUNCT
    def getNbAcquiredFrames(self) :
        return self.__buffer.getLastAcquiredFrame() + 1
    
    @Core.DEB_MEMBER_FUNCT
    def getNbHwAcquiredFrames(self):
        return self.getNbAcquiredFrames()

    #get lower communication
    def communication(self) :
        return self.__comm

    #get lower buffer
    def buffer(self) :
        return self.__buffer
