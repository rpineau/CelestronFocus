//
//  CelestronFocus.cpp
//  EF Lens Controller X2 plugin
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
    m_nPosLimit = 0;
    m_bPosLimitEnabled = 0;
    m_nCurrentApperture = 0;
	m_nLastPos = 0;
    m_bReturntoLastPos = false;
    

#ifdef CTL_DEBUG
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

#if defined CTL_DEBUG && CTL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::CCelestronFocus] Version 2019_06_5_1200.\n", timestamp);
    fprintf(Logfile, "[%s] [CCelestronFocus::CCelestronFocus] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CCelestronFocus::~CCelestronFocus()
{
#ifdef	CTL_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CCelestronFocus::Connect(const char *pszPort)
{
    int nErr = CTL_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CCelestronFocus::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

	nErr = m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CCelestronFocus::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif
	m_pSleeper->sleep(2000);

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CCelestronFocus::Connect doing a goto 0\n", timestamp);
	fflush(Logfile);
#endif

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CCelestronFocus::Connect setting apperture to %d\n", timestamp, m_nCurrentApperture);
	fflush(Logfile);
#endif

	// get version
	nErr = getFirmwareVersion(m_sFirmwareVersion);
    return nErr;
}

void CCelestronFocus::Disconnect()
{
	gotoPosition(0);
	m_pSleeper->sleep(250);
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
	Cmd[5] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Error getting AZM version : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		ssFirmwareVersion << Resp[0] << "." <<Resp [1] << "." << ((Resp[2] << 8) + Resp[3]);
		m_sFirmwareVersion = ssFirmwareVersion.str();
	}
	else
		m_sFirmwareVersion = "Unknown";
	// snprintf(AMZVersion, nStrMaxLen,"%d.%d", szResp[5] , szResp[6]);

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


    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef CTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CCelestronFocus::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);
	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 6;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_GOTO_FAST;
	Cmd[5] = (uint8_t)((nPos & 0x00ff0000) >> 16);
	Cmd[6] = (uint8_t)((nPos & 0x0000ff00) >> 8);
	Cmd[7] = (uint8_t)( nPos & 0x000000ff);
	Cmd[8] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

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

#ifdef CTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CCelestronFocus::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
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
	Cmd[5] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [SkyPortalWiFi::isMoving] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		bMoving = (Resp[0] == (uint8_t)0xFF)?false:true;
	}

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

	if(bMoving)
		return nErr;

	// check position
    getPosition(m_nCurPos);
    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
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
	Cmd[5] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		nPosition = (Resp[0] << 16) + (Resp[1] << 8) + Resp[2];
	}

	return nErr;
}

int CCelestronFocus::getPosMaxLimit(int &nPos)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nPos = 0;
	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_FOC_GET_LIMITS;
	Cmd[5] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [SkyPortalWiFi::isMoving] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		nPos = (Resp[4] << 24) + (Resp[5] << 16) + (Resp[6] << 8) + Resp[7];
	}

	return nErr;
}

int CCelestronFocus::getPosMinLimit(int &nPos)
{
	int nErr = CTL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nPos = 0;
	Cmd.assign (SERIAL_BUFFER_SIZE, 0);

	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 3;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_FOC_GET_LIMITS;
	Cmd[5] = checksum(Buffer_t(Cmd.begin()+1, Cmd.begin()+Cmd[MSG_LEN]+1), Cmd[MSG_LEN]+1);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [SkyPortalWiFi::isMoving] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		nPos = (Resp[0] << 24) + (Resp[1] << 16) + (Resp[2] << 8) + Resp[3];
	}

	return nErr;
}

#pragma mark Celestron command and response functions

int CCelestronFocus::SendCommand(const unsigned char *pszCmd, unsigned char *pszResult, int nResultMaxLen)
{
	int nErr = CTL_OK;
	unsigned char szResp[SERIAL_BUFFER_SIZE];
	unsigned long  ulBytesWrite;
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	int timeout = 0;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszCmd, cHexMessage, pszCmd[1]+3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] Sending %s\n", timestamp, cHexMessage);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)pszCmd, pszCmd[1]+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr)
		return nErr;

	if(pszResult) {
		memset(szResp,0,SERIAL_BUFFER_SIZE);
		// Read responses until one is for us or we reach a timeout ?
		while(szResp[DST_DEV]!=PC) {
			//we're waiting for the answer
			if(timeout>50) {
				return COMMAND_FAILED;
			}
			// read response
			nErr = ReadResponse(szResp, SERIAL_BUFFER_SIZE);
#if defined CTL_DEBUG && CTL_DEBUG >= 3
			if(szResp[DST_DEV]==PC) { // filter out command echo
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				hexdump(szResp, cHexMessage, szResp[1]+3, LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] response \"%s\"\n", timestamp, cHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		}
		memset(pszResult,0, nResultMaxLen);
		memcpy(pszResult, szResp, szResp[1]+3);

#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszResult, cHexMessage, pszResult[1]+3, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] response copied to pszResult : \"%s\"\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
	}
	return nErr;
}

int CCelestronFocus::SendCommand(const Buffer_t Cmd, Buffer_t Resp, const bool bExpectResponse)
{
	int nErr = CTL_OK;
	unsigned long  ulBytesWrite;
	unsigned char szHexMessage[LOG_BUFFER_SIZE];
	int timeout = 0;
	int nRespLen;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, Cmd[3]+1, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] Sending %s\n", timestamp, szHexMessage);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)Cmd.data(), Cmd[1]+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr)
		return nErr;

	if(bExpectResponse) {
		// Read responses until one is for us or we reach a timeout ?
		do {
			//we're waiting for the answer
			if(timeout>50) {
				return COMMAND_FAILED;
			}
			// read response
			nErr = ReadResponse(Resp, nRespLen);
#if defined CTL_DEBUG && CTL_DEBUG >= 3
			if(Resp.size() && Resp[DST_DEV]==PC) { // filter out command echo
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				hexdump(Resp.data(), szHexMessage, Resp[3]+1, LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] response \"%s\"\n", timestamp, szHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		} while(Resp.size() && Resp[DST_DEV]!=PC);

#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(Resp.data(), szHexMessage, Resp[3]+1, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::SkyPortalWiFiSendCommand] response copied to pszResult : \"%s\"\n", timestamp, szHexMessage);
		fflush(Logfile);
#endif
	}
	return nErr;
}


int CCelestronFocus::ReadResponse(unsigned char *pszRespBuffer, int nBufferLen)
{
	int nErr = CTL_OK;
	unsigned long ulBytesRead = 0;
	int nLen = SERIAL_BUFFER_SIZE;
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	unsigned char cChecksum;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	memset(pszRespBuffer, 0, (size_t) nBufferLen);

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

	nLen = pszRespBuffer[1];
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fflush(Logfile);
#endif
	if(nErr || ulBytesRead != (nLen+1)) {
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] error\n", timestamp);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] got %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	// verify checksum
	cChecksum = checksum(pszRespBuffer+1, nLen+1);
	if (cChecksum != *(pszRespBuffer+nLen+2)) {
		nErr = ERR_CMDFAILED;
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] Calculated checksume is %02X, message checksum is %02X\n", timestamp, cChecksum, *(pszRespBuffer+nLen+2));
		fflush(Logfile);
#endif
	}
	return nErr;
}

int CCelestronFocus::ReadResponse(Buffer_t RespBuffer, int &nLen)
{
	int nErr = CTL_OK;
	unsigned long ulBytesRead = 0;
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	unsigned char cChecksum;
	unsigned char pszRespBuffer[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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

	nLen = pszRespBuffer[1];
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fflush(Logfile);
#endif
	if(nErr || ulBytesRead != (nLen+1)) {
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszRespBuffer, cHexMessage, pszRespBuffer[1]+2, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] error\n", timestamp);
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] got %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	// verify checksum
	cChecksum = checksum(pszRespBuffer+1, nLen+1);
	if (cChecksum != *(pszRespBuffer+nLen+2)) {
		nErr = ERR_CMDFAILED;
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CSkyPortalWiFiController::readResponse] Calculated checksume is %02X, message checksum is %02X\n", timestamp, cChecksum, *(pszRespBuffer+nLen+2));
		fflush(Logfile);
#endif
	}
	nLen = pszRespBuffer[1]+2;
	RespBuffer.assign(pszRespBuffer, pszRespBuffer+nLen);
	return nErr;
}


unsigned char CCelestronFocus::checksum(const unsigned char *cMessage, int nLen)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 0; nIdx < nLen && nIdx < SERIAL_BUFFER_SIZE; nIdx++) {
		cChecksum -= cMessage[nIdx];
	}
	return (unsigned char)cChecksum;
}

uint8_t CCelestronFocus::checksum(const Buffer_t cMessage, int nLen)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 0; nIdx < nLen && nIdx < SERIAL_BUFFER_SIZE; nIdx++) {
		cChecksum -= cMessage[nIdx];
	}
	return (uint8_t)cChecksum;
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
