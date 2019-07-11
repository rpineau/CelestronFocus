//
//  CelestronFocus.cpp
//  Celesxctron focus motor X2 plugin
//
//  Created by Rodolphe Pineau on 2019/02/16.


#include "CelestronFocus.h"


CCelestronFocus::CCelestronFocus()
{

    m_pSerx = NULL;
    m_bDebugLog = false;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;

	m_nMinLinit = -1;
	m_nMaxLinit = -1;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\CelestronFocusLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CelestronFocusLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CelestronFocusLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::CCelestronFocus] Version 2019_07_8_1330.\n", timestamp);
    fprintf(Logfile, "[%s] [CCelestronFocus::CCelestronFocus] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif
}

CCelestronFocus::~CCelestronFocus()
{
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CCelestronFocus::Connect(const char *pszPort)
{
    int nErr = CTL_OK;
    bool bCalibrated;
    
    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

	nErr = m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

	m_pSleeper->sleep(1000);

	nErr = getFirmwareVersion(m_sFirmwareVersion);
    if(nErr)
        return nErr;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::Connect] firmware version : %s\n", timestamp, m_sFirmwareVersion.c_str());
    fflush(Logfile);
#endif

    nErr = getPosLimits();
    if(nErr)
        return nErr;
    
#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] position limits (min/max) : %d / %d\n", timestamp, m_nMinLinit, m_nMaxLinit);
	fflush(Logfile);
#endif

    nErr = isCalibrationDone(bCalibrated);
    if(nErr)
        return nErr;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::Connect] Focuser calibrated : %s\n", timestamp, bCalibrated?"Yes":"No");
    fflush(Logfile);
#endif

    nErr = getPosition(m_nCurPos);
    if(nErr)
        return nErr;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::Connect] Current position : %d\n", timestamp, m_nCurPos);
    fflush(Logfile);
#endif

	return nErr;
}

void CCelestronFocus::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
 
	m_bIsConnected = false;
}

int CCelestronFocus::getFirmwareVersion(std::string &sVersion)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;
    
	std::stringstream  ssFirmwareVersion;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_GET_VER;    //get firmware version
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::getFirmwareVersion] Error getting firmnware version : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		if(Resp.size() == 4)
			ssFirmwareVersion << std::dec << int(Resp[0]) << "." << int(Resp[1]) << "." << ((int(Resp[2]) << 8) + int(Resp[3]));
		else
		   ssFirmwareVersion << std::dec << int(Resp[0]) << "." << int(Resp[1]) ;
		m_sFirmwareVersion = ssFirmwareVersion.str();
	}
	else
		m_sFirmwareVersion = "Unknown";

	return nErr;
}

#pragma mark move commands
int CCelestronFocus::gotoPosition(int nPos)
{
    int nErr;
	Buffer_t Cmd;
	Buffer_t Resp;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::gotoPosition] goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    // nPos+=m_nMinLinit;
    
	Cmd.assign (SERIAL_BUFFER_SIZE, 0);
	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 6;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_GOTO_FAST;
	Cmd[5] = static_cast<uint8_t>((nPos >> 16) & 0xFF);
	Cmd[6] = static_cast<uint8_t>((nPos >> 8) & 0xFF);
	Cmd[7] = static_cast<uint8_t>( nPos & 0xFF);
	Cmd[8] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, false);
    if(nErr)
        return nErr;
    m_nTargetPos = nPos;
    return nErr;
}

int CCelestronFocus::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::moveRelativeToPosision] goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

int CCelestronFocus::isMoving(bool &bMoving)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bMoving = false;

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_SLEW_DONE;
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::isMoving] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		bMoving = (Resp[0] != (uint8_t)0xFF);
	}

	return nErr;
}


int CCelestronFocus::abort(void)
{
	int nErr;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::abort]\n", timestamp);
	fflush(Logfile);
#endif

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);
	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 4;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_MOVE_POS;
	Cmd[5] = 0;
	Cmd[6] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, false);
	if(nErr)
		return nErr;
	return nErr;
}

#pragma mark command complete functions

int CCelestronFocus::isGoToComplete(bool &bComplete)
{
    int nErr = CTL_OK;
	bool bMoving;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	bComplete = false;

	// is it still moving ?
	nErr = isMoving(bMoving);
	if(nErr)
		return nErr;

    if(bMoving) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCelestronFocus::isGoToComplete] Focuser is still moving\n", timestamp);
        fflush(Logfile);
#endif
		return nErr;
    }
	// check position
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::isGoToComplete] Focuser has stopped\n", timestamp);
    fflush(Logfile);
#endif

    getPosition(m_nCurPos);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::isGoToComplete] Focuser position : %d\n", timestamp, m_nCurPos);
    fprintf(Logfile, "[%s] [CCelestronFocus::isGoToComplete] Target position : %d\n", timestamp, m_nTargetPos);
    fflush(Logfile);
#endif

    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::isGoToComplete] bComplete : %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    return nErr;
}

#pragma mark getters and setters
int CCelestronFocus::getPosition(int &nPosition)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_GET_POSITION;
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::getPosition] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size() >= 3) {
		nPosition = (int(Resp[0]) << 16) + (int(Resp[1]) << 8) + int(Resp[2]);
	}

	return nErr;
}

int CCelestronFocus::getPosMaxLimit(int &nPos)
{
	int nErr = CTL_OK;

	if(m_nMaxLinit <0)
		nErr = getPosLimits();

	if(nErr)
		return nErr;

	nPos = m_nMaxLinit;
	return nErr;
}

int CCelestronFocus::getPosMinLimit(int &nPos)
{
	int nErr = CTL_OK;

	if(m_nMinLinit <0)
		nErr = getPosLimits();

	if(nErr)
		return nErr;

	nPos = m_nMinLinit;
	return nErr;
}


int CCelestronFocus::getPosLimits()
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_FOC_GET_LIMITS;
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::getPosMinLimit] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size() >= 8) {
		m_nMinLinit = (int(Resp[0]) << 24) + (int(Resp[1]) << 16) + (int(Resp[2]) << 8) + int(Resp[3]);
		m_nMaxLinit = (int(Resp[4]) << 24) + (int(Resp[5]) << 16) + (int(Resp[6]) << 8) + int(Resp[7]);
		if(m_nMaxLinit == 0)
			m_nMaxLinit = 65535; // could be 0xFFFFFF too, instruction are not clear.
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::getPosLimits] position limit (min/max) : %d / %d\n", timestamp, m_nMinLinit, m_nMaxLinit);
	fflush(Logfile);
#endif

	return nErr;
}


int CCelestronFocus::startCalibration(uint8_t nStart)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 4;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_FOC_CALIB;
	Cmd[5] = nStart; // 0x0 to abort, 0x1 to start
	Cmd[6] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::startCalibration] Error getting response : %d\n", timestamp, nErr);
	fflush(Logfile);
#endif

	return nErr;
}

int CCelestronFocus::isCalibrationDone(bool &bComplete)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bComplete = false;
	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_FOC_CALIB_DONE;
	Cmd[5] = checksum(Cmd);
	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::isCalibrationDone] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}

	bComplete = (int(Resp[0]) > 0);
	if(bComplete)
		getPosLimits();
	
	return nErr;
}

#pragma mark Celestron command and response functions

int CCelestronFocus::SendCommand(const Buffer_t Cmd, Buffer_t &Resp, const bool bExpectResponse)
{
	int nErr = CTL_OK;
	unsigned long  ulBytesWrite;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	unsigned char szHexMessage[LOG_BUFFER_SIZE];
#endif
	int timeout = 0;
	int nRespLen;
	uint8_t nTarget;

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, int(Cmd[1]+3), LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] Sending %s\n", timestamp, szHexMessage);
    fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] packet size is %d\n", timestamp, (int)(Cmd[1])+3);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)Cmd.data(), (unsigned long)(Cmd[1])+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] m_pSerx->writeFile Error %d\n", timestamp, nErr);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] m_pSerx->writeFile ulBytesWrite : %lu\n", timestamp, ulBytesWrite);
		fflush(Logfile);
#endif
		return nErr;
	}

    if(bExpectResponse) {
		// Read responses until one is for us or we reach a timeout ?
		do {
			//we're waiting for the answer
			if(timeout>50) {
				return ERR_CMDFAILED;
			}
			// read response
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 5
			nErr = SimulateResponse(Resp, nTarget , nRespLen);
#else
			nErr = ReadResponse(Resp, nTarget, nRespLen);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
			if(Resp.size() && nTarget == PC) { // filter out command echo
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				hexdump(Resp.data(), szHexMessage, int(Resp.size()), LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response \"%s\"\n", timestamp, szHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		} while(Resp.size() && nTarget != PC);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(Resp.data(), szHexMessage, int(Resp.size()), LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response copied to pszResult : \"%s\"\n", timestamp, szHexMessage);
		fflush(Logfile);
#endif
	}
	return nErr;
}


int CCelestronFocus::ReadResponse(Buffer_t &RespBuffer, uint8_t &nTarget, int &nLen)
{
	int nErr = CTL_OK;
	unsigned long ulBytesRead = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
#endif
    uint8_t cChecksum;
	uint8_t cRespChecksum;

	unsigned char pszRespBuffer[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	RespBuffer.clear();
	memset(pszRespBuffer, 0, (size_t) SERIAL_BUFFER_SIZE);

	// Look for a SOM starting character, until timeout occurs
	while (*pszRespBuffer != SOM && nErr == CTL_OK) {
		nErr = m_pSerx->readFile(pszRespBuffer, 1, ulBytesRead, MAX_TIMEOUT);
		if (ulBytesRead !=1) // timeout
			nErr = CTL_BAD_CMD_RESPONSE;
	}

	if(*pszRespBuffer != SOM || nErr != CTL_OK)
		return ERR_CMDFAILED;

	// Read message length
	nErr = m_pSerx->readFile(pszRespBuffer + 1, 1, ulBytesRead, MAX_TIMEOUT);
	if (nErr != CTL_OK || ulBytesRead!=1)
		return ERR_CMDFAILED;

	nLen = int(pszRespBuffer[1]);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, nLen + 3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] pszRespBuffer = %s\n", timestamp, cHexMessage);
	fflush(Logfile);
#endif

	if(nErr || (ulBytesRead != (nLen+1))) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] error not enough byte in the response\n", timestamp);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculating response checksum\n", timestamp);
	fflush(Logfile);
#endif
	// verify checksum
	cChecksum = checksum(pszRespBuffer);
	cRespChecksum = uint8_t(*(pszRespBuffer+nLen+2));

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculated checksume is 0x%02X, message checksum is 0x%02X\n", timestamp, cChecksum, cRespChecksum);
	fflush(Logfile);
#endif
	if (cChecksum != cRespChecksum) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse]  if (cChecksum != cRespChecksum) failed  !! WTF !!!  0x%02X, 0x%02X\n", timestamp, cChecksum, cRespChecksum);
		fflush(Logfile);
#endif
		nErr = ERR_CMDFAILED;
	}
	nLen = int(pszRespBuffer[MSG_LEN]-3); // SRC DST CMD [data]
	nTarget = pszRespBuffer[DST_DEV];

	RespBuffer.assign(pszRespBuffer+2+3, pszRespBuffer+2+3+nLen); // just the data without SOM, LEN , SRC, DEST, CMD_ID and checksum
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nTarget = 0x%02X\n", timestamp, nTarget);
	hexdump(RespBuffer.data(), cHexMessage, int(RespBuffer.size()), LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Resp data =  %s\n", timestamp, cHexMessage);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nErr =  %d\n", timestamp, nErr);
	fflush(Logfile);
#endif
	return nErr;
}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
int CCelestronFocus::SimulateResponse(Buffer_t &RespBuffer, uint8_t &nTarget ,int &nLen)
{
	unsigned char pszRespBuffer[] = {0x3B, 0x07, 0x12, 0x20, 0xFE, 0x07, 0x0F, 0x20, 0x30, 0x63};
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	uint8_t cChecksum;
	uint8_t cRespChecksum;
	int nErr = CTL_OK;

	RespBuffer.clear();
	nLen = int(pszRespBuffer[1]);

	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::test] nLen = %d\n", timestamp, nLen);
	hexdump(pszRespBuffer, cHexMessage, nLen + 3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::test] pszRespBuffer = %s\n", timestamp, cHexMessage);
	fflush(Logfile);
	fflush(Logfile);

	cChecksum = checksum(pszRespBuffer);
	cRespChecksum = uint8_t(*(pszRespBuffer+nLen+2));
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::test] Calculated checksume is 0x%02X, message checksum is 0x%02X\n", timestamp, cChecksum, cRespChecksum);
	fflush(Logfile);

	if (cChecksum != cRespChecksum) {
		fprintf(Logfile, "[%s] [CCelestronFocus::test] (cChecksum != uint8_t(*(pszRespBuffer+nLen+2)) : 0x%02X != 0x%02X\n", timestamp, cChecksum, cRespChecksum);
		fflush(Logfile);
		nErr = ERR_CMDFAILED;
	}
	nLen = int(pszRespBuffer[MSG_LEN]-3); // SRC DST CMD [data]
	nTarget = pszRespBuffer[DST_DEV];
	
	fprintf(Logfile, "[%s] [CCelestronFocus::test] nLen = %d\n", timestamp, nLen);
	fprintf(Logfile, "[%s] [CCelestronFocus::test] nTarget = 0x%02X\n", timestamp, nTarget);
	fflush(Logfile);
	RespBuffer.assign(pszRespBuffer+2+3, pszRespBuffer+2+3+nLen); // just the data without SOM, LEN , SRC, DEST, CMD_ID and checksum
	return nErr;
}
#endif

unsigned char CCelestronFocus::checksum(const unsigned char *cMessage)
{
	int nIdx;
	char cChecksum = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum(const unsigned char *cMessage)]\n", timestamp);
	fflush(Logfile);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1] = %d\n", timestamp, int(cMessage[1]));
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1]+2 = %d\n", timestamp, int(cMessage[1])+2);
	fflush(Logfile);
#endif

	for (nIdx = 1; nIdx < int(cMessage[1])+2; nIdx++) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::checksum] nIdx = %d  cMessage[nIdx] = '0x%02X'\n", timestamp, nIdx, int(cMessage[nIdx]));
		fflush(Logfile);
#endif
		cChecksum += cMessage[nIdx];
	}
	return (unsigned char)(-cChecksum & 0xff);
}

uint8_t CCelestronFocus::checksum(const Buffer_t cMessage)
{
	int nIdx;
	char cChecksum = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum(const Buffer_t cMessage)]\n", timestamp);
	fflush(Logfile);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1] = %d\n", timestamp, int(cMessage[1]));
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1]+2 = %d\n", timestamp, int(cMessage[1])+2);
	fflush(Logfile);
#endif

	for (nIdx = 1; nIdx < int(cMessage[1])+2; nIdx++) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::checksum] nIdx = %d  cMessage[nIdx] = '0x%02X'\n", timestamp, nIdx, int(cMessage[nIdx]));
		fflush(Logfile);
#endif
		cChecksum += cMessage[nIdx];
	}
	return (uint8_t)(-cChecksum & 0xff);
}

void CCelestronFocus::hexdump(const unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize)
{
	unsigned char *pszBuf = pszOutputBuffer;
	int nIdx=0;

	memset(pszOutputBuffer, 0, nOutpuBufferSize);
	for(nIdx=0; nIdx < nInputBufferSize && pszBuf < (pszOutputBuffer + nOutpuBufferSize -3); nIdx++){
        snprintf((char *)pszBuf,4,"%02X ", pszInputBuffer[nIdx]);
		pszBuf+=3;
	}
}


std::string& CCelestronFocus::trim(std::string &str, const std::string& filter )
{
	return ltrim(rtrim(str, filter), filter);
}

std::string& CCelestronFocus::ltrim(std::string& str, const std::string& filter)
{
	str.erase(0, str.find_first_not_of(filter));
	return str;
}

std::string& CCelestronFocus::rtrim(std::string& str, const std::string& filter)
{
	str.erase(str.find_last_not_of(filter) + 1);
	return str;
}
