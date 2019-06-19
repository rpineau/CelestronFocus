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

	m_nMinLinit = -1;
	m_nMaxLinit = -1;

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
    fprintf(Logfile, "[%s] [CCelestronFocus::CCelestronFocus] Version 2019_06_7_2055.\n", timestamp);
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

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

	m_pSleeper->sleep(2000);

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] getting firmware version\n", timestamp);
	fflush(Logfile);
#endif

	// get version
	nErr = getFirmwareVersion(m_sFirmwareVersion);

#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::Connect] firmware version : %s\n", timestamp, m_sFirmwareVersion.c_str());
	fflush(Logfile);
#endif

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
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined CTL_DEBUG && CTL_DEBUG >= 2
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
		   ssFirmwareVersion << Resp[0] << "." <<Resp [1] << "." << ((Resp[2] << 8) + Resp[3]);
		else
		   ssFirmwareVersion << Resp[0] << "." <<Resp [1] ;
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

#ifdef CTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCelestronFocus::gotoPosition] goto position  : %d\n", timestamp, nPos);
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

#ifdef CTL_DEBUG
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
#if defined CTL_DEBUG && CTL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::isMoving] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		bMoving = (Resp[0] == (uint8_t)0xFF)?false:true;
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


#ifdef CTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::abort]\n", timestamp);
	fflush(Logfile);
#endif

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);
	Cmd[0] = SOM;
	Cmd[MSG_LEN] = 6;
	Cmd[SRC_DEV] = PC;
	Cmd[DST_DEV] = FOC;
	Cmd[CMD_ID] = MC_MOVE_POS;
	Cmd[5] = 0;
	Cmd[6] = 0;
	Cmd[7] = 0;
	Cmd[8] = checksum(Cmd);

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
	Cmd[5] = checksum(Cmd);

	nErr = SendCommand(Cmd, Resp, true);
	if(nErr) {
#if defined CTL_DEBUG && CTL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::getPosition] Error getting response : %d\n", timestamp, nErr);
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
#if defined CTL_DEBUG && CTL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::getPosMinLimit] Error getting response : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
	if(Resp.size()) {
		m_nMinLinit = (Resp[0] << 24) + (Resp[1] << 16) + (Resp[2] << 8) + Resp[3];
		m_nMaxLinit = (Resp[4] << 24) + (Resp[5] << 16) + (Resp[6] << 8) + Resp[7];

	}

#if defined CTL_DEBUG && CTL_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::getPosLimits] position limit (min/max) : %d / %d\n", timestamp, m_nMinLinit, m_nMaxLinit);
	fflush(Logfile);
#endif

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
	hexdump(pszCmd, cHexMessage, int(pszCmd[1])+3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] Sending %s\n", timestamp, cHexMessage);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)pszCmd, (unsigned long)(pszCmd[1])+3, ulBytesWrite);
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
				hexdump(szResp, cHexMessage, int(szResp[1])+3, LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response \"%s\"\n", timestamp, cHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		}
		memset(pszResult,0, nResultMaxLen);
		memcpy(pszResult, szResp, int(szResp[1])+3);

#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszResult, cHexMessage, int(pszResult[1])+3, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response copied to pszResult : \"%s\"\n", timestamp, cHexMessage);
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
	hexdump(Cmd.data(), szHexMessage, (int)(Cmd[1])+3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] Sending %s\n", timestamp, szHexMessage);
    fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] packet size is %d\n", timestamp, (int)(Cmd[1])+3);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)Cmd.data(), (unsigned long)(Cmd[1])+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr) {
#if defined CTL_DEBUG && CTL_DEBUG >= 3
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
				fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response \"%s\"\n", timestamp, szHexMessage);
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
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response copied to pszResult : \"%s\"\n", timestamp, szHexMessage);
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

	nLen = int(pszRespBuffer[1]);
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fflush(Logfile);
#endif
	if(nErr || ulBytesRead != (nLen+1)) {
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] error\n", timestamp);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] got %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	// verify checksum
	cChecksum = checksum(pszRespBuffer);
	if (cChecksum != *(pszRespBuffer+nLen+2)) {
		nErr = ERR_CMDFAILED;
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculated checksume is %02X, message checksum is %02X\n", timestamp, cChecksum, *(pszRespBuffer+nLen+2));
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

	nLen = int(pszRespBuffer[1]);
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined CTL_DEBUG && CTL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fflush(Logfile);
#endif
	if(nErr || ulBytesRead != (nLen+1)) {
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pszRespBuffer, cHexMessage, int(pszRespBuffer[1])+2, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] error\n", timestamp);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] got %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	// verify checksum
	cChecksum = checksum(pszRespBuffer);
	if (cChecksum != *(pszRespBuffer+nLen+2)) {
		nErr = ERR_CMDFAILED;
#if defined CTL_DEBUG && CTL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculated checksume is %02X, message checksum is %02X\n", timestamp, cChecksum, *(pszRespBuffer+nLen+2));
		fflush(Logfile);
#endif
	}
	nLen = int(pszRespBuffer[1])+2;
	RespBuffer.assign(pszRespBuffer, pszRespBuffer+nLen);
	return nErr;
}


unsigned char CCelestronFocus::checksum(const unsigned char *cMessage)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 1; nIdx < cMessage[1]+2; nIdx++) {
		cChecksum -= cMessage[nIdx];
	}
	return (unsigned char)cChecksum;
}

uint8_t CCelestronFocus::checksum(const Buffer_t cMessage)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 1; nIdx < cMessage[1]+2; nIdx++) {
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
