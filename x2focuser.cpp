
#include "x2focuser.h"


X2Focuser::X2Focuser(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;		
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;

	m_bLinked = false;
	m_nPosition = 0;
	m_bCalibrating = false;

	m_CelestronFocus.SetSerxPointer(m_pSerX);
	m_CelestronFocus.setSleeper(m_pSleeper);
	m_CelestronFocus.setTheSkyXForMount(m_pTheSkyXForMounts);

}

X2Focuser::~X2Focuser()
{
    //Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();

}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return DRIVER_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
	str="Celestron Focus Motor";
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str="Celestron Focus Motor";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    str="";
	if(m_bLinked){
		X2MutexLocker ml(GetMutex());
		std::string sFirmware;
		m_CelestronFocus.getFirmwareVersion(sFirmware);
		str=sFirmware.c_str();
	}
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    str="Celestron Focus Motor";
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_CelestronFocus.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

	m_CelestronFocus.Disconnect();
    m_bLinked = false;

	return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

	if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("CelestronFocus.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

	X2MutexLocker ml(GetMutex());

	if(m_bLinked) {
		dx->setEnabled("pushButton", true);
	}
	else {
		dx->setEnabled("pushButton", false);
	}
	//Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
    }

    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	bool bComplete;

	// pushButton
	if (!strcmp(pszEvent, "on_timer")) {
		if(m_bCalibrating) {
			// check if we're done but not too often, 3 seconds should be fine
			if(m_CalibratingTimer.GetElapsedSeconds()>3) {
				m_CelestronFocus.isCalibrationDone(bComplete);
				if(bComplete) {
					m_bCalibrating = false;
					uiex->setText("pushButton", "Calibrate focuser");
					uiex->setEnabled("pushButtonCancel", true);
					uiex->setEnabled("pushButtonOK", true);
				}
				m_CalibratingTimer.Reset();
			}
		}
	}
	else if	(!strcmp(pszEvent, "on_pushButton_clicked")) {
		if(!m_bCalibrating) {
			m_CelestronFocus.startCalibration(START_CALIBRATION);
			m_bCalibrating = true;
			uiex->setText("pushButton", "Abort Calibration");
			uiex->setEnabled("pushButtonCancel", false);
			uiex->setEnabled("pushButtonOK", false);
			m_CalibratingTimer.Reset();
		} else {
			m_CelestronFocus.startCalibration(ABORT_CALIBRATION);
			m_bCalibrating = false;
			uiex->setText("pushButton", "Calibrate focuser");
			uiex->setEnabled("pushButtonCancel", true);
			uiex->setEnabled("pushButtonOK", true);
		}
	}
}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nErr = m_CelestronFocus.getPosition(nPosition);
    m_nPosition = nPosition;
    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	int nErr = SB_OK;

	if(!m_bLinked)
		return NOT_CONNECTED;

	X2MutexLocker ml(GetMutex());
	nErr = m_CelestronFocus.getPosMinLimit(nMinLimit);

	return nErr;
}

int	X2Focuser::focMaximumLimit(int& nMaxLimit)
{
	int nErr = SB_OK;

	if(!m_bLinked)
		return NOT_CONNECTED;

	X2MutexLocker ml(GetMutex());
	nErr = m_CelestronFocus.getPosMaxLimit(nMaxLimit);

	return nErr;
}

int	X2Focuser::focAbort()								
{
	int nErr = SB_OK;

	if(!m_bLinked)
		return NOT_CONNECTED;

	X2MutexLocker ml(GetMutex());
	nErr = m_CelestronFocus.abort();

	return nErr;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)	
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_CelestronFocus.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
	nErr = pMe->m_CelestronFocus.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_CelestronFocus.getPosition(m_nPosition);
    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 4;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
		case 3: strDisplayName="5000 steps"; nAmount=5000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    dTemperature = -100.0;
    return SB_OK;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}




