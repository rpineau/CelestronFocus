//
//  CelestronFocus.h
//  Celestron Focus Motor X2 plugin
//
//  Created by Rodolphe Pineau on 2019/02/16.

#ifndef __CTL__
#define __CTL__
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_WIN_BUILD
#include <direct.h>
#include <time.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"

// #define PLUGIN_DEBUG 3

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256
#define CMD_SIZE 5000

#define SOM     0x3B
#define MSG_LEN 1
#define SRC_DEV 2
#define DST_DEV 3
#define CMD_ID  4

typedef std::vector<uint8_t> Buffer_t;

enum Targets {
	PC = 0x20,	// we're sending from AUX address 0x20
	AZM = 0x10,
	ALT = 0x11,
	GPS = 0xb0,
	FOC = 0x12
};

enum MC_Commands
{
	MC_GET_POSITION         = 0x01,
	MC_GOTO_FAST            = 0x02,
	MC_SLEW_DONE            = 0x13,
	MC_GOTO_SLOW            = 0x17,
	MC_MOVE_POS             = 0x24,
	MC_MOVE_NEG             = 0x25,
	MC_GET_VER              = 0xfe,
	MC_FOC_CALIB			= 0x2a,
	MC_FOC_CALIB_DONE 		= 0x2b,
	MC_FOC_GET_LIMITS 		= 0x2c,
};



enum CTL_Errors    {CTL_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, CTL_BAD_CMD_RESPONSE};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};
enum CalibrationMode {ABORT_CALIBRATION=0, START_CALIBRATION};

class CCelestronFocus
{
public:
    CCelestronFocus();
    ~CCelestronFocus();


    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
	void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };
	void        setTheSkyXForMount(TheSkyXFacadeForDriversInterface *pTheSkyXForMounts) { m_pTheSkyXForMounts = pTheSkyXForMounts; };

	int			getFirmwareVersion(std::string &sVersion);

	// move commands
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);
	int			isMoving(bool &bMoving);
	int			abort(void);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    int         getPosition(int &nPosition);
    int         getPosMaxLimit(int &nPos);
	int         getPosMinLimit(int &nPos);

	int			startCalibration(uint8_t nStart); // 0x0 to abort, 0x1 to start
	int			isCalibrationDone(bool &bComplete);

protected:

	int     SendCommand(const Buffer_t Cmd, Buffer_t &Resp, const bool bExpectResponse);
	int     ReadResponse(Buffer_t &RespBuffer, uint8_t &nTarget, int &nlen);

	unsigned char checksum(const unsigned char *cMessage);
	uint8_t checksum(const Buffer_t cMessage);

	void    hexdump(const unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize);

	int		getPosLimits();
	
	std::string&    trim(std::string &str, const std::string &filter );
	std::string&    ltrim(std::string &str, const std::string &filter);
	std::string&    rtrim(std::string &str, const std::string &filter);

    SerXInterface   *m_pSerx;
	SleeperInterface    *m_pSleeper;
	TheSkyXFacadeForDriversInterface	*m_pTheSkyXForMounts;
	
    bool		m_bDebugLog;
    bool		m_bIsConnected;
	std::string	m_sFirmwareVersion;

    int			m_nCurPos;
    int			m_nTargetPos;
	int			m_nMinLinit;
	int			m_nMaxLinit;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	int			SimulateResponse(Buffer_t &RespBuffer, uint8_t &nTarget ,int &nLen);
#endif
    

#ifdef PLUGIN_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif //__CTL__
